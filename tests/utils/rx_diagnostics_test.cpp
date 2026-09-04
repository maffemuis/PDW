#include "rx_diagnostics.h"

#include <stdio.h>
#include <string.h>

static bool Check(bool condition, const char *name)
{
    if (condition) return true;
    fprintf(stderr, "RX_DIAGNOSTICS_TEST_FAIL: %s\n", name);
    fflush(stderr);
    return false;
}

int main()
{
    RxDiagnosticsInit();
    RxDiagnosticsReset();

    DCB dcb = {};
    dcb.BaudRate = CBR_19200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    RxDiagnosticsOnComOpen(2, &dcb);

    const BYTE bytes[] = { 0xA5, 0x5A };
    RxDiagnosticsOnRead(bytes, sizeof(bytes), FALSE);
    RxDiagnosticsOnPreambleCounters(7, 17, 179);
    RxDiagnosticsSetConfiguredInvert(FALSE);
    RxDiagnosticsOnRateDetected(2400);
    RxDiagnosticsOnSyncSearchDistance(6, FALSE);
    RxDiagnosticsOnSyncFound(2, FALSE);
    RxDiagnosticsOnCodeword(0);
    RxDiagnosticsOnCodeword(1);
    RxDiagnosticsOnCodeword(3);
    RxDiagnosticsOnSyncLost();

    RX_DIAG_SNAPSHOT snapshot;
    RxDiagnosticsGetSnapshot(&snapshot);

    if (!Check(snapshot.com_open == TRUE, "com_open")) return 1;
    if (!Check(snapshot.com_port == 2, "com_port")) return 2;
    if (!Check(snapshot.baud_rate == CBR_19200, "baud_rate")) return 3;
    if (!Check(snapshot.byte_size == 8, "byte_size")) return 4;
    if (!Check(snapshot.rx_bytes == 2, "rx_bytes")) return 5;
    if (!Check(snapshot.rx_reads == 1, "rx_reads")) return 6;
    if (!Check(snapshot.rx_symbols == 16, "rx_symbols")) return 7;
    if (!Check(snapshot.preamble_512 == 7, "preamble_512")) return 8;
    if (!Check(snapshot.preamble_1200 == 17, "preamble_1200")) return 9;
    if (!Check(snapshot.preamble_2400 == 179, "preamble_2400")) return 10;
    if (!Check(snapshot.detected_2400 == 1, "detected_2400")) return 11;
    if (!Check(snapshot.current_pocsag_baud == 2400, "current_pocsag_baud")) return 12;
    if (!Check(snapshot.sync_found == 1, "sync_found")) return 13;
    if (!Check(snapshot.sync_lost == 1, "sync_lost")) return 14;
    if (!Check(snapshot.codewords_total == 3, "codewords_total")) return 15;
    if (!Check(snapshot.codewords_good == 1, "codewords_good")) return 16;
    if (!Check(snapshot.codewords_corrected == 1, "codewords_corrected")) return 17;
    if (!Check(snapshot.codewords_uncorrectable == 1, "codewords_uncorrectable")) return 18;

    char formatted[4096];
    RxDiagnosticsFormatSnapshot(&snapshot, formatted, sizeof(formatted));
    fprintf(stdout, "%s\n", formatted);
    fflush(stdout);
    if (!Check(strstr(formatted, "OPEN COM2") != NULL, "format COM2")) return 19;
    if (!Check(strstr(formatted, "19200 8N1") != NULL, "format 19200 8N1")) return 20;
    if (!Check(strstr(formatted, "Preamble:") != NULL, "format preamble")) return 21;
    if (!Check(strstr(formatted, "2400=179") != NULL, "format preamble 2400")) return 22;
    if (!Check(strstr(formatted, "uncorrectable=1") != NULL, "format uncorrectable")) return 23;

    if (!Check(RxDiagnosticsStartRawLog(".", 8192) == TRUE, "raw log start")) return 24;
    RxDiagnosticsOnRead(bytes, sizeof(bytes), FALSE);
    RxDiagnosticsOnRateDetected(2400);
    RxDiagnosticsOnSyncFound(1, FALSE);
    RxDiagnosticsOnCodeword(0);
    RxDiagnosticsGetSnapshot(&snapshot);
    if (!Check(snapshot.raw_log_enabled == TRUE, "raw log enabled")) return 25;
    if (!Check(snapshot.raw_log_path[0] != '\0', "raw log path")) return 26;
    if (!Check(snapshot.raw_log_bytes > 0, "raw log bytes")) return 27;
    RxDiagnosticsStopRawLog();

    FILE *raw = fopen(snapshot.raw_log_path, "rb");
    if (!Check(raw != NULL, "raw log exists")) return 28;
    fclose(raw);
    remove(snapshot.raw_log_path);

    RxDiagnosticsOnComClosed();
    RxDiagnosticsGetSnapshot(&snapshot);
    if (!Check(snapshot.com_open == FALSE, "com closed")) return 29;

    fprintf(stdout, "RX_DIAGNOSTICS_TEST_PASS\n");
    return 0;
}
