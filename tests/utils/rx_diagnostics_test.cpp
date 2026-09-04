#include "rx_diagnostics.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

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

    assert(snapshot.com_open == TRUE);
    assert(snapshot.com_port == 2);
    assert(snapshot.baud_rate == CBR_19200);
    assert(snapshot.byte_size == 8);
    assert(snapshot.rx_bytes == 2);
    assert(snapshot.rx_reads == 1);
    assert(snapshot.rx_symbols == 16);
    assert(snapshot.preamble_512 == 7);
    assert(snapshot.preamble_1200 == 17);
    assert(snapshot.preamble_2400 == 179);
    assert(snapshot.detected_2400 == 1);
    assert(snapshot.current_pocsag_baud == 2400);
    assert(snapshot.sync_found == 1);
    assert(snapshot.sync_lost == 1);
    assert(snapshot.codewords_total == 3);
    assert(snapshot.codewords_good == 1);
    assert(snapshot.codewords_corrected == 1);
    assert(snapshot.codewords_uncorrectable == 1);

    char formatted[4096];
    RxDiagnosticsFormatSnapshot(&snapshot, formatted, sizeof(formatted));
    assert(strstr(formatted, "OPEN COM2") != NULL);
    assert(strstr(formatted, "19200 8N1") != NULL);
    assert(strstr(formatted, "Preamble:") != NULL);
    assert(strstr(formatted, "2400=179") != NULL);
    assert(strstr(formatted, "uncorrectable=1") != NULL);

    assert(RxDiagnosticsStartRawLog(".", 2048) == TRUE);
    RxDiagnosticsOnRead(bytes, sizeof(bytes), FALSE);
    RxDiagnosticsOnRateDetected(2400);
    RxDiagnosticsOnSyncFound(1, FALSE);
    RxDiagnosticsOnCodeword(0);
    RxDiagnosticsGetSnapshot(&snapshot);
    assert(snapshot.raw_log_enabled == TRUE);
    assert(snapshot.raw_log_path[0] != '\0');
    assert(snapshot.raw_log_bytes > 0);
    RxDiagnosticsStopRawLog();

    FILE *raw = fopen(snapshot.raw_log_path, "rb");
    assert(raw != NULL);
    fclose(raw);
    remove(snapshot.raw_log_path);

    RxDiagnosticsOnComClosed();
    RxDiagnosticsGetSnapshot(&snapshot);
    assert(snapshot.com_open == FALSE);

    return 0;
}
