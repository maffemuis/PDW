#include <windows.h>
#include <winbase.h>
#include <stdio.h>
#include "..\headers\slicer.h"
#include "..\utils\debug.h"
#include "..\utils\ostype.h"
#include "..\headers\pdw.h"
#include "rs232.h"

#define SLICER_BUFSIZE 10000

volatile HANDLE m_hRxThread = INVALID_HANDLE_VALUE;
ULONG WINAPI RxThread(LPVOID pCl);
BOOL m_bConnectedToComport = FALSE;
HANDLE m_ComPortHandle = INVALID_HANDLE_VALUE;
DWORD m_dwThreadId = 0;
BOOL bKeepThreadAlive;
double nTiming;
BOOL bOrgcomPortRS232;
BOOL bSlicerDriver = FALSE;

WORD rs232_freqdata[SLICER_BUFSIZE];
BYTE rs232_linedata[SLICER_BUFSIZE];
DWORD rs232_cpstn;
BYTE byRS232Data[SLICER_BUFSIZE * (sizeof(WORD) + sizeof(BYTE))];

static RS232_DIAGNOSTICS g_rs232_diag = {};
static HANDLE g_raw_log = INVALID_HANDLE_VALUE;
static SRWLOCK g_raw_log_lock = SRWLOCK_INIT;
static DWORD g_raw_log_start_tick = 0;
static DWORD g_diag_open_tick = 0;

#define assert(a) if(!(a)) { OUTPUTDEBUGMSG(("SIMULATE ASSERT in file %s at %d\n", __FILE__, __LINE__)); }

static void rs232_diag_reset_runtime(void)
{
    g_rs232_diag.rx_bytes = 0;
    g_rs232_diag.rx_bits = 0;
    g_rs232_diag.last_rx_tick = 0;
    g_rs232_diag.read_errors = 0;
    g_rs232_diag.framing_errors = 0;
    g_rs232_diag.parity_errors = 0;
    g_rs232_diag.overrun_errors = 0;
    g_rs232_diag.rx_queue_bytes = 0;
    g_rs232_diag.ring_position = rs232_cpstn;
    g_rs232_diag.ring_wraps = 0;
    g_diag_open_tick = GetTickCount();
}

static void rs232_diag_record_comm_errors(DWORD errors, const COMSTAT *stat)
{
    if(errors & CE_FRAME) g_rs232_diag.framing_errors++;
    if(errors & CE_RXPARITY) g_rs232_diag.parity_errors++;
    if(errors & (CE_OVERRUN | CE_RXOVER)) g_rs232_diag.overrun_errors++;
    if(stat) g_rs232_diag.rx_queue_bytes = stat->cbInQue;
}

static void rs232_raw_log_write_byte(BYTE value)
{
    char line[96];
    DWORD written = 0;
    int len;

    AcquireSRWLockExclusive(&g_raw_log_lock);
    if(g_raw_log == INVALID_HANDLE_VALUE || !g_rs232_diag.raw_log_enabled) {
        ReleaseSRWLockExclusive(&g_raw_log_lock);
        return;
    }

    len = _snprintf(line, sizeof(line) - 1,
        "%lu ms RX %02X bits=%d%d%d%d%d%d%d%d\r\n",
        GetTickCount() - g_raw_log_start_tick,
        (unsigned int)value,
        (value >> 7) & 1, (value >> 6) & 1, (value >> 5) & 1, (value >> 4) & 1,
        (value >> 3) & 1, (value >> 2) & 1, (value >> 1) & 1, value & 1);
    if(len < 0) len = 0;
    line[sizeof(line) - 1] = 0;

    if(g_rs232_diag.raw_log_bytes + (DWORD)len > RS232_RAW_LOG_MAX_BYTES) {
        g_rs232_diag.raw_log_limit_reached = TRUE;
        g_rs232_diag.raw_log_enabled = FALSE;
        CloseHandle(g_raw_log);
        g_raw_log = INVALID_HANDLE_VALUE;
        ReleaseSRWLockExclusive(&g_raw_log_lock);
        return;
    }

    if(!WriteFile(g_raw_log, line, (DWORD)len, &written, NULL) || written != (DWORD)len) {
        g_rs232_diag.raw_log_enabled = FALSE;
        CloseHandle(g_raw_log);
        g_raw_log = INVALID_HANDLE_VALUE;
        ReleaseSRWLockExclusive(&g_raw_log_lock);
        return;
    }
    g_rs232_diag.raw_log_bytes += written;
    ReleaseSRWLockExclusive(&g_raw_log_lock);
}

void rs232_get_diagnostics(RS232_DIAGNOSTICS *out)
{
    if(!out) return;
    *out = g_rs232_diag;
    out->connected = m_bConnectedToComport;
    out->original_rs232 = bOrgcomPortRS232;
    out->slicer_driver = bSlicerDriver;
    out->ring_position = rs232_cpstn;
}

int rs232_format_diagnostics(char *buffer, int buffer_len)
{
    RS232_DIAGNOSTICS d;
    DWORD now;
    DWORD age;
    DWORD elapsed;
    DWORD bytes_per_sec;
    int rc;
    if(!buffer || buffer_len <= 0) return 0;
    rs232_get_diagnostics(&d);
    now = GetTickCount();
    age = d.last_rx_tick ? now - d.last_rx_tick : 0;
    elapsed = (d.connected && g_diag_open_tick) ? now - g_diag_open_tick : 0;
    bytes_per_sec = elapsed ? (DWORD)(((ULONGLONG)d.rx_bytes * 1000ULL) / elapsed) : 0;
    rc = _snprintf(buffer, buffer_len - 1,
        "Serial/RS232: %s COM%d\r\n"
        "DCB: %lu baud, %u data, parity=%u, stop=%u, CTS=%s, DSR=%s, XON-in=%s, XON-out=%s\r\n"
        "RX: bytes=%lu rate=%lu B/s bits=%lu last=%s%lu ms queue=%lu ring=%lu wraps=%lu\r\n"
        "Errors: read=%lu framing=%lu parity=%lu overrun=%lu\r\n"
        "Raw RX log: %s bytes=%lu/%lu limit=%s",
        d.connected ? "OPEN" : "CLOSED", d.com_port,
        d.baud_rate, d.byte_size, d.parity, d.stop_bits,
        d.cts_flow ? "on" : "off", d.dsr_flow ? "on" : "off",
        d.xon_xoff_in ? "on" : "off", d.xon_xoff_out ? "on" : "off",
        d.rx_bytes, bytes_per_sec, d.rx_bits, d.last_rx_tick ? "" : "never/", age,
        d.rx_queue_bytes, d.ring_position, d.ring_wraps,
        d.read_errors, d.framing_errors, d.parity_errors, d.overrun_errors,
        d.raw_log_enabled ? "ON" : "OFF", d.raw_log_bytes, (DWORD)RS232_RAW_LOG_MAX_BYTES,
        d.raw_log_limit_reached ? "REACHED" : "no");
    buffer[buffer_len - 1] = 0;
    return rc;
}

int rs232_raw_log_start(const char *path)
{
    const char *use_path = (path && *path) ? path : "pdw-rx-raw.log";
    HANDLE h;
    AcquireSRWLockExclusive(&g_raw_log_lock);
    if(g_raw_log != INVALID_HANDLE_VALUE) {
        CloseHandle(g_raw_log);
        g_raw_log = INVALID_HANDLE_VALUE;
    }
    h = CreateFile(use_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                   FILE_ATTRIBUTE_NORMAL, NULL);
    if(h == INVALID_HANDLE_VALUE) {
        g_rs232_diag.raw_log_enabled = FALSE;
        ReleaseSRWLockExclusive(&g_raw_log_lock);
        return 0;
    }
    g_raw_log = h;
    g_raw_log_start_tick = GetTickCount();
    g_rs232_diag.raw_log_bytes = 0;
    g_rs232_diag.raw_log_limit_reached = FALSE;
    g_rs232_diag.raw_log_enabled = TRUE;
    ReleaseSRWLockExclusive(&g_raw_log_lock);
    return 1;
}

void rs232_raw_log_stop(void)
{
    AcquireSRWLockExclusive(&g_raw_log_lock);
    g_rs232_diag.raw_log_enabled = FALSE;
    if(g_raw_log != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(g_raw_log);
        CloseHandle(g_raw_log);
        g_raw_log = INVALID_HANDLE_VALUE;
    }
    ReleaseSRWLockExclusive(&g_raw_log_lock);
}

int rs232_connect(const SLICER_IN_STR *pInSlicer, SLICER_OUT_STR *pOutSlicer)
{
    extern double ct1600;
    int rc = RS232_NO_DUT;
    char pcComPort[32] = "COM1:";
    DCB m_comDCB = {};
    DCB activeDCB = {};
    COMMPROP ComProp = {};
    COMMTIMEOUTS ComTimeOuts = {};

    bOrgcomPortRS232 = Profile.comPortRS232;
    g_rs232_diag.com_port = pInSlicer->com_port;
    g_rs232_diag.original_rs232 = bOrgcomPortRS232;
    if(pInSlicer->com_port > 9) {
        _snprintf(pcComPort, sizeof(pcComPort) - 1, R"(\\.\COM%d)", pInSlicer->com_port);
        pcComPort[sizeof(pcComPort) - 1] = '\0';
    }
    else {
        _snprintf(pcComPort, sizeof(pcComPort) - 1, "COM%d", pInSlicer->com_port);
        pcComPort[sizeof(pcComPort) - 1] = '\0';
    }

    switch(Profile.comPortRS232) {
        case 1: nTiming = 500; break;
        case 2: default: nTiming = ct1600; break;
        case 3: nTiming = 1.0 / ((float)8000 * 839.22e-9); break;
    }

    OUTPUTDEBUGMSG((("calling: rs232_connect(%s)\n"), pcComPort));
    pOutSlicer->freqdata = rs232_freqdata;
    pOutSlicer->linedata = rs232_linedata;
    pOutSlicer->cpstn = &rs232_cpstn;
    pOutSlicer->bufsize = SLICER_BUFSIZE;

    if(m_bConnectedToComport) {
        rc = rs232_disconnect();
        assert(rc >= 0);
        if(rc < 0) return rc;
    }

    m_ComPortHandle = CreateFile(pcComPort, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if(m_ComPortHandle == INVALID_HANDLE_VALUE) {
        OUTPUTDEBUGMSG((("ERROR: CreateFile() %08lX!\n"), GetLastError()));
        return RS232_NO_DUT;
    }

    if(!GetCommProperties(m_ComPortHandle, &ComProp)) {
        OUTPUTDEBUGMSG((("ERROR: GetCommProperties() %08lX!\n"), GetLastError()));
        CloseHandle(m_ComPortHandle);
        m_ComPortHandle = INVALID_HANDLE_VALUE;
        return RS232_NO_DUT;
    }

    bSlicerDriver = (ComProp.dwProvSpec1 == 0x48576877 && ComProp.dwProvSpec2 == 0x68774857);
    g_rs232_diag.slicer_driver = bSlicerDriver;
    if(!bOrgcomPortRS232 && !bSlicerDriver) {
        CloseHandle(m_ComPortHandle);
        m_ComPortHandle = INVALID_HANDLE_VALUE;
        MessageBox(NULL, "Please install the Slicer driver from the install package!",
                   "Slicer Driver Not Installed", MB_OK | MB_ICONEXCLAMATION);
        return RS232_NO_DUT;
    }

    if(!GetCommState(m_ComPortHandle, &m_comDCB)) {
        OUTPUTDEBUGMSG((("ERROR: GetCommState() %08lX!\n"), GetLastError()));
        CloseHandle(m_ComPortHandle);
        m_ComPortHandle = INVALID_HANDLE_VALUE;
        return RS232_NO_DUT;
    }

    m_comDCB.BaudRate = bOrgcomPortRS232 ? CBR_19200 : (nOSType == OS_WIN2000) ? CBR_SLICER_2K : CBR_SLICER_XP;
    m_comDCB.ByteSize = 8;
    m_comDCB.Parity = NOPARITY;
    m_comDCB.StopBits = ONESTOPBIT;
    m_comDCB.fBinary = TRUE;
    m_comDCB.fParity = FALSE;
    m_comDCB.fDtrControl = DTR_CONTROL_DISABLE;
    m_comDCB.fRtsControl = bOrgcomPortRS232 ? RTS_CONTROL_DISABLE : RTS_CONTROL_ENABLE;

    /* A normal RS232 capture is a receive-only 19200 8N1 stream. Do not inherit
       hardware/software flow-control from an existing Windows COM profile. */
    if(bOrgcomPortRS232) {
        m_comDCB.fOutxCtsFlow = FALSE;
        m_comDCB.fOutxDsrFlow = FALSE;
        m_comDCB.fDsrSensitivity = FALSE;
        m_comDCB.fTXContinueOnXoff = TRUE;
        m_comDCB.fOutX = FALSE;
        m_comDCB.fInX = FALSE;
        m_comDCB.fErrorChar = FALSE;
        m_comDCB.fNull = FALSE;
        m_comDCB.fAbortOnError = FALSE;
    }

    if(!SetCommState(m_ComPortHandle, &m_comDCB)) {
        OUTPUTDEBUGMSG((("ERROR: SetCommState() %08lX!\n"), GetLastError()));
        CloseHandle(m_ComPortHandle);
        m_ComPortHandle = INVALID_HANDLE_VALUE;
        return RS232_NO_DUT;
    }

    activeDCB.DCBlength = sizeof(activeDCB);
    if(!GetCommState(m_ComPortHandle, &activeDCB)) {
        CloseHandle(m_ComPortHandle);
        m_ComPortHandle = INVALID_HANDLE_VALUE;
        return RS232_NO_DUT;
    }
    g_rs232_diag.baud_rate = activeDCB.BaudRate;
    g_rs232_diag.byte_size = activeDCB.ByteSize;
    g_rs232_diag.parity = activeDCB.Parity;
    g_rs232_diag.stop_bits = activeDCB.StopBits;
    g_rs232_diag.cts_flow = activeDCB.fOutxCtsFlow;
    g_rs232_diag.dsr_flow = activeDCB.fOutxDsrFlow;
    g_rs232_diag.xon_xoff_in = activeDCB.fInX;
    g_rs232_diag.xon_xoff_out = activeDCB.fOutX;

    if(bOrgcomPortRS232 && (activeDCB.BaudRate != CBR_19200 || activeDCB.ByteSize != 8 ||
       activeDCB.Parity != NOPARITY || activeDCB.StopBits != ONESTOPBIT ||
       activeDCB.fOutxCtsFlow || activeDCB.fOutxDsrFlow || activeDCB.fInX || activeDCB.fOutX)) {
        CloseHandle(m_ComPortHandle);
        m_ComPortHandle = INVALID_HANDLE_VALUE;
        return RS232_NO_DUT;
    }

    if(!SetCommMask(m_ComPortHandle, bOrgcomPortRS232 ? 0 : EV_CTS | EV_DSR | EV_RLSD)) {
        CloseHandle(m_ComPortHandle);
        m_ComPortHandle = INVALID_HANDLE_VALUE;
        return RS232_NO_DUT;
    }
    if(!PurgeComm(m_ComPortHandle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR)) {
        CloseHandle(m_ComPortHandle);
        m_ComPortHandle = INVALID_HANDLE_VALUE;
        return RS232_NO_DUT;
    }
    if(bOrgcomPortRS232) {
        ComTimeOuts.ReadIntervalTimeout = MAXDWORD;
        ComTimeOuts.ReadTotalTimeoutMultiplier = MAXDWORD;
        ComTimeOuts.ReadTotalTimeoutConstant = 100;
        if(!SetCommTimeouts(m_ComPortHandle, &ComTimeOuts)) {
            CloseHandle(m_ComPortHandle);
            m_ComPortHandle = INVALID_HANDLE_VALUE;
            return RS232_NO_DUT;
        }
    }

    rs232_diag_reset_runtime();
    m_bConnectedToComport = TRUE;
    g_rs232_diag.connected = TRUE;
    bKeepThreadAlive = TRUE;
    m_hRxThread = CreateThread(NULL, 0, RxThread, (LPVOID)NULL, CREATE_SUSPENDED, &m_dwThreadId);
    if(m_hRxThread == NULL || m_hRxThread == INVALID_HANDLE_VALUE) {
        m_bConnectedToComport = FALSE;
        g_rs232_diag.connected = FALSE;
        CloseHandle(m_ComPortHandle);
        m_ComPortHandle = INVALID_HANDLE_VALUE;
        return RS232_NO_DUT;
    }
    ResumeThread(m_hRxThread);
    return RS232_SUCCESS;
}

int rs232_disconnect()
{
    int rc;
    COMMTIMEOUTS ComTimeOuts = {0};
    if(!m_bConnectedToComport) return RS232_NO_CONNECTION;
    bKeepThreadAlive = FALSE;
    Sleep(250);
    if(m_hRxThread != INVALID_HANDLE_VALUE && m_hRxThread != NULL) TerminateThread(m_hRxThread, -1);
    Sleep(100);
    SetCommTimeouts(m_ComPortHandle, &ComTimeOuts);
    SetCommMask(m_ComPortHandle, 0);
    assert(m_ComPortHandle != INVALID_HANDLE_VALUE);
    rc = CloseHandle(m_ComPortHandle);
    if(!rc) rc = RS232_UNKNOWN;
    else {
        m_ComPortHandle = INVALID_HANDLE_VALUE;
        m_bConnectedToComport = FALSE;
        g_rs232_diag.connected = FALSE;
    }
    rs232_raw_log_stop();
    return RS232_SUCCESS;
}

DWORD WINAPI RxThread(LPVOID pCl)
{
    do {
        if(bOrgcomPortRS232) rs232_read();
        else slicer_read();
        Sleep(50);
    } while(bKeepThreadAlive);
    m_hRxThread = INVALID_HANDLE_VALUE;
    ExitThread(0L);
    return 0;
}

int rs232_read(void)
{
    DWORD dwRead = 0;
    DWORD commErrors = 0;
    COMSTAT stat = {};
    int bit;
    BYTE byData[256];

    if(m_ComPortHandle == INVALID_HANDLE_VALUE) return 0;
    if(ClearCommError(m_ComPortHandle, &commErrors, &stat)) rs232_diag_record_comm_errors(commErrors, &stat);

    if(!ReadFile(m_ComPortHandle, byData, sizeof(byData), &dwRead, 0)) {
        g_rs232_diag.read_errors++;
        ClearCommError(m_ComPortHandle, &commErrors, &stat);
        rs232_diag_record_comm_errors(commErrors, &stat);
        PurgeComm(m_ComPortHandle, PURGE_RXCLEAR);
        return 0;
    }

    if(dwRead) {
        g_rs232_diag.rx_bytes += dwRead;
        g_rs232_diag.last_rx_tick = GetTickCount();
    }
    for(DWORD i = 0; i < dwRead; i++) {
        rs232_raw_log_write_byte(byData[i]);
        for(int j = 7; j >= 0; j--) {
            if(Profile.fourlevel) {
                j--;
                bit = (byData[i] >> j) & 3;
                g_rs232_diag.rx_bits += 2;
            }
            else {
                bit = (byData[i] >> j) & 1;
                g_rs232_diag.rx_bits++;
            }
            rs232_linedata[rs232_cpstn] = bit << 4;
            rs232_freqdata[rs232_cpstn++] = (WORD)nTiming;
            if(rs232_cpstn >= SLICER_BUFSIZE) {
                rs232_cpstn = 0;
                g_rs232_diag.ring_wraps++;
            }
        }
    }
    g_rs232_diag.ring_position = rs232_cpstn;
    if(ClearCommError(m_ComPortHandle, &commErrors, &stat)) rs232_diag_record_comm_errors(commErrors, &stat);
    return (int)dwRead;
}

int slicer_read(void)
{
    DWORD dwRead = 0, i, num;
    WORD *freq;
    BYTE *line;
    if(m_ComPortHandle == INVALID_HANDLE_VALUE) return 0;
    if(!ReadFile(m_ComPortHandle, byRS232Data, sizeof(byRS232Data), &dwRead, 0)) {
        PurgeComm(m_ComPortHandle, PURGE_RXCLEAR);
        return 0;
    }
    num = dwRead / (sizeof(WORD) + sizeof(BYTE));
    line = byRS232Data;
    freq = (WORD *)(byRS232Data + num * sizeof(BYTE));
    for(i = 0; i < num; i++) {
        rs232_linedata[rs232_cpstn] = *line++;
        rs232_freqdata[rs232_cpstn++] = *freq++;
        if(rs232_cpstn >= SLICER_BUFSIZE) rs232_cpstn = 0;
    }
    return (int)i;
}

#define _COMPORT_1 0
#define _COMPORT_2 1
#define _COMPORT_3 2
#define _COMPORT_4 3

int nComPort2 = _COMPORT_1;
HANDLE m_ComPortHandle2 = INVALID_HANDLE_VALUE;
BOOL m_bConnectedToComport2 = FALSE;

int OpenComPort(void)
{
    int rc = RS232_NO_DUT;
    char pcComPort[32] = "COM1:";
    DCB m_comDCB = {};
    pcComPort[3] = '1' + nComPort2;
    if(m_bConnectedToComport2) {
        rc = CloseComPort();
        assert(rc >= 0);
        if(rc < 0) return rc;
    }
    m_ComPortHandle2 = CreateFile(pcComPort, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if(m_ComPortHandle2 == INVALID_HANDLE_VALUE) return RS232_NO_DUT;
    if(!GetCommState(m_ComPortHandle2, &m_comDCB)) {
        CloseHandle(m_ComPortHandle2);
        m_ComPortHandle2 = INVALID_HANDLE_VALUE;
        return RS232_NO_DUT;
    }
    m_comDCB.BaudRate = CBR_19200;
    m_comDCB.ByteSize = 8;
    m_comDCB.Parity = NOPARITY;
    m_comDCB.StopBits = ONESTOPBIT;
    m_comDCB.fBinary = TRUE;
    m_comDCB.fParity = FALSE;
    m_comDCB.fDtrControl = DTR_CONTROL_ENABLE;
    m_comDCB.fRtsControl = RTS_CONTROL_ENABLE;
    if(!SetCommState(m_ComPortHandle2, &m_comDCB)) {
        CloseHandle(m_ComPortHandle2);
        m_ComPortHandle2 = INVALID_HANDLE_VALUE;
        return RS232_NO_DUT;
    }
    if(!PurgeComm(m_ComPortHandle2, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR)) {
        CloseHandle(m_ComPortHandle2);
        m_ComPortHandle2 = INVALID_HANDLE_VALUE;
        return RS232_NO_DUT;
    }
    m_bConnectedToComport2 = TRUE;
    return rc;
}

char chStartChar = 10;
char chEndChar = 4;

int WriteComPort(char *szLine)
{
    char szTemp[1024];
    int len = 0;
    DWORD dwWrite;
    if(chStartChar) szTemp[len++] = chStartChar;
    len += wsprintf(szTemp + len, "%s", szLine);
    if(chEndChar) {
        szTemp[len++] = chEndChar;
        szTemp[len] = 0;
    }
    if(!WriteFile(m_ComPortHandle2, szTemp, len, &dwWrite, 0)) return RS232_UNKNOWN;
    return 0;
}

int CloseComPort(void)
{
    int rc = CloseHandle(m_ComPortHandle2);
    if(!rc) rc = RS232_UNKNOWN;
    else {
        m_ComPortHandle2 = INVALID_HANDLE_VALUE;
        m_bConnectedToComport2 = FALSE;
    }
    return 0;
}

int nComPortsArr[11];

int *FindComPorts(void)
{
    DWORD error;
    int nNumFound = 0;
    char szPort[32];
    HANDLE hCom;
    for(int i = 1; i < 50; i++) {
        if(i > 9) wsprintf(szPort, "\\\\.\\COM%d", i);
        else wsprintf(szPort, "COM%d", i);
        error = ERROR_SUCCESS;
        hCom = CreateFile(szPort, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
        if(hCom == INVALID_HANDLE_VALUE) error = GetLastError();
        if(error != ERROR_FILE_NOT_FOUND && nNumFound < 10) nComPortsArr[nNumFound++] = i;
        if(hCom != INVALID_HANDLE_VALUE) CloseHandle(hCom);
    }
    nComPortsArr[nNumFound] = 0;
    return nComPortsArr;
}

int GetRs232DriverType(void)
{
    return m_bConnectedToComport ? (bSlicerDriver ? DRIVER_TYPE_SLICER : DRIVER_TYPE_RS232) : DRIVER_TYPE_NOT_LOADED;
}
