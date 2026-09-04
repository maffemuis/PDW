#include "rx_diagnostics.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

namespace {

volatile LONG g_initialized = 0;

volatile LONG g_com_open = 0;
volatile LONG g_com_port = 0;
volatile LONG g_baud_rate = 0;
volatile LONG g_byte_size = 0;
volatile LONG g_parity = 0;
volatile LONG g_stop_bits = 0;
volatile LONG g_flow_mask = 0;

volatile LONG g_rx_bytes = 0;
volatile LONG g_rx_reads = 0;
volatile LONG g_rx_symbols = 0;
volatile LONG g_last_rx_tick = 0;
volatile LONG g_read_errors = 0;
volatile LONG g_last_read_error = 0;
volatile LONG g_comm_error_events = 0;
volatile LONG g_framing_errors = 0;
volatile LONG g_parity_errors = 0;
volatile LONG g_overrun_errors = 0;
volatile LONG g_rx_overflow_errors = 0;
volatile LONG g_input_queue = 0;

volatile LONG g_preamble_512 = 0;
volatile LONG g_preamble_1200 = 0;
volatile LONG g_preamble_2400 = 0;
volatile LONG g_detected_512 = 0;
volatile LONG g_detected_1200 = 0;
volatile LONG g_detected_2400 = 0;
volatile LONG g_current_pocsag_baud = 0;
volatile LONG g_pocsag_active = 0;
volatile LONG g_configured_invert = 0;

volatile LONG g_sync_locked = 0;
volatile LONG g_sync_found = 0;
volatile LONG g_sync_lost = 0;
volatile LONG g_last_sync_distance = -1;
volatile LONG g_best_sync_distance = -1;
volatile LONG g_last_sync_inverted = 0;

volatile LONG g_codewords_total = 0;
volatile LONG g_codewords_good = 0;
volatile LONG g_codewords_corrected = 0;
volatile LONG g_codewords_uncorrectable = 0;

CRITICAL_SECTION g_raw_lock;
FILE *g_raw_file = NULL;
volatile LONG g_raw_enabled = 0;
volatile LONG g_raw_limit_reached = 0;
volatile LONG g_raw_bytes = 0;
volatile LONG g_raw_limit = RX_DIAG_DEFAULT_RAW_LIMIT;
DWORD g_raw_start_tick = 0;
char g_raw_path[MAX_PATH] = "";

LONG ReadLong(volatile LONG *value)
{
    return InterlockedCompareExchange(value, 0, 0);
}

void SetLong(volatile LONG *value, LONG new_value)
{
    InterlockedExchange(value, new_value);
}

void AddLong(volatile LONG *value, LONG amount)
{
    InterlockedExchangeAdd(value, amount);
}

void EnsureInitialized()
{
    if (!ReadLong(&g_initialized))
    {
        RxDiagnosticsInit();
    }
}

void RawWriteLocked(const char *text)
{
    if (!g_raw_file || !ReadLong(&g_raw_enabled) || !text)
    {
        return;
    }

    const size_t length = strlen(text);
    const DWORD used = (DWORD)ReadLong(&g_raw_bytes);
    const DWORD limit = (DWORD)ReadLong(&g_raw_limit);
    const DWORD remaining = used < limit ? limit - used : 0;

    if (length > (size_t)remaining)
    {
        static const char marker[] = "RAW_LOG_LIMIT_REACHED\r\n";
        const size_t marker_length = sizeof(marker) - 1;
        if (marker_length <= remaining)
        {
            fwrite(marker, 1, marker_length, g_raw_file);
            fflush(g_raw_file);
            SetLong(&g_raw_bytes, (LONG)(used + (DWORD)marker_length));
        }
        SetLong(&g_raw_limit_reached, 1);
        SetLong(&g_raw_enabled, 0);
        fclose(g_raw_file);
        g_raw_file = NULL;
        return;
    }

    fwrite(text, 1, length, g_raw_file);
    fflush(g_raw_file);
    SetLong(&g_raw_bytes, (LONG)(used + (DWORD)length));
}

void RawEvent(const char *format, ...)
{
    if (!ReadLong(&g_raw_enabled))
    {
        return;
    }

    char body[1024];
    char line[1200];
    va_list args;
    va_start(args, format);
    _vsnprintf(body, sizeof(body) - 1, format, args);
    va_end(args);
    body[sizeof(body) - 1] = '\0';

    const DWORD elapsed = GetTickCount() - g_raw_start_tick;
    _snprintf(line, sizeof(line) - 1, "%010lu EV %s\r\n", (unsigned long)elapsed, body);
    line[sizeof(line) - 1] = '\0';

    EnterCriticalSection(&g_raw_lock);
    RawWriteLocked(line);
    LeaveCriticalSection(&g_raw_lock);
}

const char *ParityName(BYTE parity)
{
    switch (parity)
    {
        case NOPARITY: return "N";
        case ODDPARITY: return "O";
        case EVENPARITY: return "E";
        case MARKPARITY: return "M";
        case SPACEPARITY: return "S";
        default: return "?";
    }
}

const char *StopBitsName(BYTE stop_bits)
{
    switch (stop_bits)
    {
        case ONESTOPBIT: return "1";
        case ONE5STOPBITS: return "1.5";
        case TWOSTOPBITS: return "2";
        default: return "?";
    }
}

} // namespace

void RxDiagnosticsInit(void)
{
    if (InterlockedCompareExchange(&g_initialized, 1, 0) == 0)
    {
        InitializeCriticalSection(&g_raw_lock);
    }
}

void RxDiagnosticsReset(void)
{
    EnsureInitialized();
    RxDiagnosticsStopRawLog();

    SetLong(&g_com_open, 0);
    SetLong(&g_com_port, 0);
    SetLong(&g_baud_rate, 0);
    SetLong(&g_byte_size, 0);
    SetLong(&g_parity, 0);
    SetLong(&g_stop_bits, 0);
    SetLong(&g_flow_mask, 0);

    SetLong(&g_rx_bytes, 0);
    SetLong(&g_rx_reads, 0);
    SetLong(&g_rx_symbols, 0);
    SetLong(&g_last_rx_tick, 0);
    SetLong(&g_read_errors, 0);
    SetLong(&g_last_read_error, 0);
    SetLong(&g_comm_error_events, 0);
    SetLong(&g_framing_errors, 0);
    SetLong(&g_parity_errors, 0);
    SetLong(&g_overrun_errors, 0);
    SetLong(&g_rx_overflow_errors, 0);
    SetLong(&g_input_queue, 0);

    SetLong(&g_preamble_512, 0);
    SetLong(&g_preamble_1200, 0);
    SetLong(&g_preamble_2400, 0);
    SetLong(&g_detected_512, 0);
    SetLong(&g_detected_1200, 0);
    SetLong(&g_detected_2400, 0);
    SetLong(&g_current_pocsag_baud, 0);
    SetLong(&g_pocsag_active, 0);
    SetLong(&g_configured_invert, 0);

    SetLong(&g_sync_locked, 0);
    SetLong(&g_sync_found, 0);
    SetLong(&g_sync_lost, 0);
    SetLong(&g_last_sync_distance, -1);
    SetLong(&g_best_sync_distance, -1);
    SetLong(&g_last_sync_inverted, 0);

    SetLong(&g_codewords_total, 0);
    SetLong(&g_codewords_good, 0);
    SetLong(&g_codewords_corrected, 0);
    SetLong(&g_codewords_uncorrectable, 0);

    SetLong(&g_raw_limit_reached, 0);
    SetLong(&g_raw_bytes, 0);
    SetLong(&g_raw_limit, RX_DIAG_DEFAULT_RAW_LIMIT);
    EnterCriticalSection(&g_raw_lock);
    g_raw_path[0] = '\0';
    LeaveCriticalSection(&g_raw_lock);
}

void RxDiagnosticsOnComOpen(int com_port, const DCB *dcb)
{
    EnsureInitialized();
    SetLong(&g_com_open, 1);
    SetLong(&g_com_port, com_port);

    if (dcb)
    {
        SetLong(&g_baud_rate, (LONG)dcb->BaudRate);
        SetLong(&g_byte_size, (LONG)dcb->ByteSize);
        SetLong(&g_parity, (LONG)dcb->Parity);
        SetLong(&g_stop_bits, (LONG)dcb->StopBits);

        DWORD flow = 0;
        if (dcb->fOutxCtsFlow) flow |= 0x01;
        if (dcb->fOutxDsrFlow) flow |= 0x02;
        if (dcb->fOutX || dcb->fInX) flow |= 0x04;
        SetLong(&g_flow_mask, (LONG)flow);
    }

    RawEvent("COM_OPEN port=%d baud=%lu data=%u parity=%u stop=%u flow=0x%lx",
             com_port,
             dcb ? (unsigned long)dcb->BaudRate : 0UL,
             dcb ? (unsigned)dcb->ByteSize : 0U,
             dcb ? (unsigned)dcb->Parity : 0U,
             dcb ? (unsigned)dcb->StopBits : 0U,
             (unsigned long)ReadLong(&g_flow_mask));
}

void RxDiagnosticsOnComClosed(void)
{
    SetLong(&g_com_open, 0);
    SetLong(&g_input_queue, 0);
    RawEvent("COM_CLOSED");
}

void RxDiagnosticsOnComOpenError(DWORD error_code)
{
    AddLong(&g_read_errors, 1);
    SetLong(&g_last_read_error, (LONG)error_code);
    SetLong(&g_com_open, 0);
    RawEvent("COM_OPEN_ERROR error=%lu", (unsigned long)error_code);
}

void RxDiagnosticsOnRead(const BYTE *data, DWORD length, BOOL four_level)
{
    if (!data || !length)
    {
        return;
    }

    AddLong(&g_rx_bytes, (LONG)length);
    AddLong(&g_rx_reads, 1);
    AddLong(&g_rx_symbols, (LONG)(length * (four_level ? 4UL : 8UL)));
    SetLong(&g_last_rx_tick, (LONG)GetTickCount());

    if (!ReadLong(&g_raw_enabled))
    {
        return;
    }

    char line[8192];
    size_t pos = 0;
    const DWORD elapsed = GetTickCount() - g_raw_start_tick;
    int n = _snprintf(line, sizeof(line) - 1, "%010lu RX bytes=%lu hex=",
                      (unsigned long)elapsed, (unsigned long)length);
    if (n < 0) return;
    pos = (size_t)n;

    for (DWORD i = 0; i < length && pos + 3 < sizeof(line); ++i)
    {
        n = _snprintf(line + pos, sizeof(line) - pos - 1, "%02X", (unsigned)data[i]);
        if (n < 0) break;
        pos += (size_t)n;
    }

    if (pos + 7 < sizeof(line))
    {
        memcpy(line + pos, " bits=", 6);
        pos += 6;
    }

    for (DWORD i = 0; i < length && pos + 10 < sizeof(line); ++i)
    {
        for (int bit = 7; bit >= 0; --bit)
        {
            line[pos++] = ((data[i] >> bit) & 1) ? '1' : '0';
        }
        if (i + 1 < length) line[pos++] = ' ';
    }

    if (pos + 3 < sizeof(line))
    {
        line[pos++] = '\r';
        line[pos++] = '\n';
    }
    line[pos] = '\0';

    EnterCriticalSection(&g_raw_lock);
    RawWriteLocked(line);
    LeaveCriticalSection(&g_raw_lock);
}

void RxDiagnosticsOnReadError(DWORD error_code)
{
    AddLong(&g_read_errors, 1);
    SetLong(&g_last_read_error, (LONG)error_code);
    RawEvent("READ_ERROR error=%lu", (unsigned long)error_code);
}

void RxDiagnosticsOnCommStatus(DWORD errors, DWORD input_queue)
{
    SetLong(&g_input_queue, (LONG)input_queue);
    if (!errors) return;

    AddLong(&g_comm_error_events, 1);
    if (errors & CE_FRAME) AddLong(&g_framing_errors, 1);
    if (errors & CE_RXPARITY) AddLong(&g_parity_errors, 1);
    if (errors & CE_OVERRUN) AddLong(&g_overrun_errors, 1);
    if (errors & CE_RXOVER) AddLong(&g_rx_overflow_errors, 1);

    RawEvent("COMM_ERROR flags=0x%08lX in_queue=%lu",
             (unsigned long)errors, (unsigned long)input_queue);
}

void RxDiagnosticsSetConfiguredInvert(BOOL invert)
{
    SetLong(&g_configured_invert, invert ? 1 : 0);
}

void RxDiagnosticsOnPreambleCounters(int count_512, int count_1200, int count_2400)
{
    SetLong(&g_preamble_512, count_512);
    SetLong(&g_preamble_1200, count_1200);
    SetLong(&g_preamble_2400, count_2400);
}

void RxDiagnosticsOnRateDetected(int baud)
{
    SetLong(&g_current_pocsag_baud, baud);
    SetLong(&g_pocsag_active, 1);
    if (baud == 512) AddLong(&g_detected_512, 1);
    else if (baud == 1200) AddLong(&g_detected_1200, 1);
    else if (baud == 2400) AddLong(&g_detected_2400, 1);
    RawEvent("POCSAG_RATE_DETECTED baud=%d", baud);
}

void RxDiagnosticsOnPocsagInactive(void)
{
    SetLong(&g_pocsag_active, 0);
    SetLong(&g_current_pocsag_baud, 0);
    SetLong(&g_sync_locked, 0);
}

void RxDiagnosticsOnSyncSearch(void)
{
    SetLong(&g_sync_locked, 0);
}

void RxDiagnosticsOnSyncSearchDistance(int distance, BOOL inverted_candidate)
{
    SetLong(&g_last_sync_distance, distance);
    SetLong(&g_last_sync_inverted, inverted_candidate ? 1 : 0);

    LONG best = ReadLong(&g_best_sync_distance);
    if (best < 0 || distance < best) SetLong(&g_best_sync_distance, distance);
}

void RxDiagnosticsOnSyncFound(int distance, BOOL inverted)
{
    SetLong(&g_sync_locked, 1);
    SetLong(&g_last_sync_distance, distance);
    SetLong(&g_last_sync_inverted, inverted ? 1 : 0);
    AddLong(&g_sync_found, 1);

    LONG best = ReadLong(&g_best_sync_distance);
    if (best < 0 || distance < best) SetLong(&g_best_sync_distance, distance);

    RawEvent("SYNC_FOUND distance=%d inverted=%d", distance, inverted ? 1 : 0);
}

void RxDiagnosticsOnSyncLost(void)
{
    if (ReadLong(&g_sync_locked)) AddLong(&g_sync_lost, 1);
    SetLong(&g_sync_locked, 0);
    RawEvent("SYNC_LOST");
}

void RxDiagnosticsOnCodeword(int corrected_bits)
{
    AddLong(&g_codewords_total, 1);
    if (corrected_bits <= 0) AddLong(&g_codewords_good, 1);
    else if (corrected_bits <= 2) AddLong(&g_codewords_corrected, 1);
    else AddLong(&g_codewords_uncorrectable, 1);
    RawEvent("CODEWORD correction=%d", corrected_bits);
}

BOOL RxDiagnosticsStartRawLog(const char *base_directory, DWORD limit_bytes)
{
    EnsureInitialized();
    RxDiagnosticsStopRawLog();

    char directory[MAX_PATH] = "";
    if (base_directory && base_directory[0])
    {
        strncpy(directory, base_directory, sizeof(directory) - 1);
        directory[sizeof(directory) - 1] = '\0';
    }
    else
    {
        GetCurrentDirectoryA(sizeof(directory), directory);
    }

    SYSTEMTIME now;
    GetLocalTime(&now);

    _snprintf(g_raw_path, sizeof(g_raw_path) - 1,
              "%s\\PDW-RX-%04u%02u%02u-%02u%02u%02u.log",
              directory,
              (unsigned)now.wYear, (unsigned)now.wMonth, (unsigned)now.wDay,
              (unsigned)now.wHour, (unsigned)now.wMinute, (unsigned)now.wSecond);
    g_raw_path[sizeof(g_raw_path) - 1] = '\0';

    EnterCriticalSection(&g_raw_lock);
    g_raw_file = fopen(g_raw_path, "wb");
    if (!g_raw_file)
    {
        g_raw_path[0] = '\0';
        LeaveCriticalSection(&g_raw_lock);
        return FALSE;
    }

    SetLong(&g_raw_bytes, 0);
    SetLong(&g_raw_limit, (LONG)(limit_bytes ? limit_bytes : RX_DIAG_DEFAULT_RAW_LIMIT));
    SetLong(&g_raw_limit_reached, 0);
    g_raw_start_tick = GetTickCount();
    SetLong(&g_raw_enabled, 1);

    char header[512];
    _snprintf(header, sizeof(header) - 1,
              "PDW RAW RX LOG\r\n"
              "format: relative_ms RX bytes=<n> hex=<bytes> bits=<MSB-to-LSB>\r\n"
              "events: decoder state only; no SMTP credentials or message payload text\r\n"
              "limit_bytes=%lu\r\n",
              (unsigned long)ReadLong(&g_raw_limit));
    header[sizeof(header) - 1] = '\0';
    RawWriteLocked(header);
    LeaveCriticalSection(&g_raw_lock);

    RawEvent("RAW_LOG_STARTED path=%s", g_raw_path);
    return TRUE;
}

void RxDiagnosticsStopRawLog(void)
{
    if (!ReadLong(&g_initialized)) return;

    EnterCriticalSection(&g_raw_lock);
    if (g_raw_file)
    {
        static const char footer[] = "RAW_LOG_STOPPED\r\n";
        if (ReadLong(&g_raw_enabled)) RawWriteLocked(footer);
        if (g_raw_file)
        {
            fflush(g_raw_file);
            fclose(g_raw_file);
            g_raw_file = NULL;
        }
    }
    SetLong(&g_raw_enabled, 0);
    LeaveCriticalSection(&g_raw_lock);
}

void RxDiagnosticsGetSnapshot(RX_DIAG_SNAPSHOT *snapshot)
{
    if (!snapshot) return;
    EnsureInitialized();
    ZeroMemory(snapshot, sizeof(*snapshot));

    snapshot->com_open = ReadLong(&g_com_open) ? TRUE : FALSE;
    snapshot->com_port = (int)ReadLong(&g_com_port);
    snapshot->baud_rate = (DWORD)ReadLong(&g_baud_rate);
    snapshot->byte_size = (BYTE)ReadLong(&g_byte_size);
    snapshot->parity = (BYTE)ReadLong(&g_parity);
    snapshot->stop_bits = (BYTE)ReadLong(&g_stop_bits);
    snapshot->flow_mask = (DWORD)ReadLong(&g_flow_mask);

    snapshot->rx_bytes = (DWORD)ReadLong(&g_rx_bytes);
    snapshot->rx_reads = (DWORD)ReadLong(&g_rx_reads);
    snapshot->rx_symbols = (DWORD)ReadLong(&g_rx_symbols);
    snapshot->last_rx_tick = (DWORD)ReadLong(&g_last_rx_tick);
    snapshot->read_errors = (DWORD)ReadLong(&g_read_errors);
    snapshot->last_read_error = (DWORD)ReadLong(&g_last_read_error);
    snapshot->comm_error_events = (DWORD)ReadLong(&g_comm_error_events);
    snapshot->framing_errors = (DWORD)ReadLong(&g_framing_errors);
    snapshot->parity_errors = (DWORD)ReadLong(&g_parity_errors);
    snapshot->overrun_errors = (DWORD)ReadLong(&g_overrun_errors);
    snapshot->rx_overflow_errors = (DWORD)ReadLong(&g_rx_overflow_errors);
    snapshot->input_queue = (DWORD)ReadLong(&g_input_queue);

    snapshot->preamble_512 = ReadLong(&g_preamble_512);
    snapshot->preamble_1200 = ReadLong(&g_preamble_1200);
    snapshot->preamble_2400 = ReadLong(&g_preamble_2400);
    snapshot->detected_512 = (DWORD)ReadLong(&g_detected_512);
    snapshot->detected_1200 = (DWORD)ReadLong(&g_detected_1200);
    snapshot->detected_2400 = (DWORD)ReadLong(&g_detected_2400);
    snapshot->current_pocsag_baud = (int)ReadLong(&g_current_pocsag_baud);
    snapshot->pocsag_active = ReadLong(&g_pocsag_active) ? TRUE : FALSE;
    snapshot->configured_invert = ReadLong(&g_configured_invert) ? TRUE : FALSE;

    snapshot->sync_locked = ReadLong(&g_sync_locked) ? TRUE : FALSE;
    snapshot->sync_found = (DWORD)ReadLong(&g_sync_found);
    snapshot->sync_lost = (DWORD)ReadLong(&g_sync_lost);
    snapshot->last_sync_distance = (int)ReadLong(&g_last_sync_distance);
    snapshot->best_sync_distance = (int)ReadLong(&g_best_sync_distance);
    snapshot->last_sync_inverted = ReadLong(&g_last_sync_inverted) ? TRUE : FALSE;

    snapshot->codewords_total = (DWORD)ReadLong(&g_codewords_total);
    snapshot->codewords_good = (DWORD)ReadLong(&g_codewords_good);
    snapshot->codewords_corrected = (DWORD)ReadLong(&g_codewords_corrected);
    snapshot->codewords_uncorrectable = (DWORD)ReadLong(&g_codewords_uncorrectable);

    snapshot->raw_log_enabled = ReadLong(&g_raw_enabled) ? TRUE : FALSE;
    snapshot->raw_log_limit_reached = ReadLong(&g_raw_limit_reached) ? TRUE : FALSE;
    snapshot->raw_log_bytes = (DWORD)ReadLong(&g_raw_bytes);
    snapshot->raw_log_limit = (DWORD)ReadLong(&g_raw_limit);

    EnterCriticalSection(&g_raw_lock);
    strncpy(snapshot->raw_log_path, g_raw_path, sizeof(snapshot->raw_log_path) - 1);
    snapshot->raw_log_path[sizeof(snapshot->raw_log_path) - 1] = '\0';
    LeaveCriticalSection(&g_raw_lock);
}

void RxDiagnosticsFormatSnapshot(const RX_DIAG_SNAPSHOT *snapshot, char *buffer, size_t buffer_size)
{
    if (!snapshot || !buffer || buffer_size == 0) return;

    char last_rx[32];
    if (snapshot->last_rx_tick)
    {
        _snprintf(last_rx, sizeof(last_rx) - 1, "%lu ms", (unsigned long)(GetTickCount() - snapshot->last_rx_tick));
        last_rx[sizeof(last_rx) - 1] = '\0';
    }
    else
    {
        strcpy(last_rx, "never");
    }

    _snprintf(buffer, buffer_size - 1,
              "COM: %s COM%d | %lu %u%s%s | flow=0x%lX\r\n"
              "Traffic: bytes=%lu reads=%lu symbols=%lu last=%s queue=%lu\r\n"
              "Errors: read=%lu last=%lu comm=%lu frame=%lu parity=%lu overrun=%lu rxover=%lu\r\n"
              "Preamble: 512=%ld | 1200=%ld | 2400=%ld\r\n"
              "Detected: 512=%lu | 1200=%lu | 2400=%lu | current=%d active=%s\r\n"
              "Polarity: configured=%s | last-sync=%s\r\n"
              "Sync: %s found=%lu lost=%lu last-distance=%d best=%d\r\n"
              "Codewords: total=%lu good=%lu corrected=%lu uncorrectable=%lu\r\n"
              "Raw log: %s %lu/%lu bytes%s\r\n"
              "%s",
              snapshot->com_open ? "OPEN" : "CLOSED",
              snapshot->com_port,
              (unsigned long)snapshot->baud_rate,
              (unsigned)snapshot->byte_size,
              ParityName(snapshot->parity),
              StopBitsName(snapshot->stop_bits),
              (unsigned long)snapshot->flow_mask,
              (unsigned long)snapshot->rx_bytes,
              (unsigned long)snapshot->rx_reads,
              (unsigned long)snapshot->rx_symbols,
              last_rx,
              (unsigned long)snapshot->input_queue,
              (unsigned long)snapshot->read_errors,
              (unsigned long)snapshot->last_read_error,
              (unsigned long)snapshot->comm_error_events,
              (unsigned long)snapshot->framing_errors,
              (unsigned long)snapshot->parity_errors,
              (unsigned long)snapshot->overrun_errors,
              (unsigned long)snapshot->rx_overflow_errors,
              snapshot->preamble_512,
              snapshot->preamble_1200,
              snapshot->preamble_2400,
              (unsigned long)snapshot->detected_512,
              (unsigned long)snapshot->detected_1200,
              (unsigned long)snapshot->detected_2400,
              snapshot->current_pocsag_baud,
              snapshot->pocsag_active ? "YES" : "NO",
              snapshot->configured_invert ? "INVERT" : "NORMAL",
              snapshot->last_sync_inverted ? "INVERTED" : "NORMAL",
              snapshot->sync_locked ? "LOCKED" : "SEARCH",
              (unsigned long)snapshot->sync_found,
              (unsigned long)snapshot->sync_lost,
              snapshot->last_sync_distance,
              snapshot->best_sync_distance,
              (unsigned long)snapshot->codewords_total,
              (unsigned long)snapshot->codewords_good,
              (unsigned long)snapshot->codewords_corrected,
              (unsigned long)snapshot->codewords_uncorrectable,
              snapshot->raw_log_enabled ? "ON" : "OFF",
              (unsigned long)snapshot->raw_log_bytes,
              (unsigned long)snapshot->raw_log_limit,
              snapshot->raw_log_limit_reached ? " LIMIT REACHED" : "",
              snapshot->raw_log_path);
    buffer[buffer_size - 1] = '\0';
}

BOOL RxDiagnosticsCopySnapshotToClipboard(HWND owner)
{
    RX_DIAG_SNAPSHOT snapshot;
    char text[4096];
    RxDiagnosticsGetSnapshot(&snapshot);
    RxDiagnosticsFormatSnapshot(&snapshot, text, sizeof(text));

    const size_t bytes = strlen(text) + 1;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) return FALSE;

    void *target = GlobalLock(memory);
    if (!target)
    {
        GlobalFree(memory);
        return FALSE;
    }
    memcpy(target, text, bytes);
    GlobalUnlock(memory);

    if (!OpenClipboard(owner))
    {
        GlobalFree(memory);
        return FALSE;
    }

    EmptyClipboard();
    if (!SetClipboardData(CF_TEXT, memory))
    {
        CloseClipboard();
        GlobalFree(memory);
        return FALSE;
    }

    CloseClipboard();
    return TRUE;
}
