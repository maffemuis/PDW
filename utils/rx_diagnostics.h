#pragma once

#include <windows.h>
#include <stddef.h>

#define RX_DIAG_DEFAULT_RAW_LIMIT (16UL * 1024UL * 1024UL)

typedef struct RX_DIAG_SNAPSHOT
{
    BOOL com_open;
    int com_port;
    DWORD baud_rate;
    BYTE byte_size;
    BYTE parity;
    BYTE stop_bits;
    DWORD flow_mask;

    DWORD rx_bytes;
    DWORD rx_reads;
    DWORD rx_symbols;
    DWORD last_rx_tick;
    DWORD read_errors;
    DWORD last_read_error;
    DWORD comm_error_events;
    DWORD framing_errors;
    DWORD parity_errors;
    DWORD overrun_errors;
    DWORD rx_overflow_errors;
    DWORD input_queue;

    LONG preamble_512;
    LONG preamble_1200;
    LONG preamble_2400;
    DWORD detected_512;
    DWORD detected_1200;
    DWORD detected_2400;
    int current_pocsag_baud;
    BOOL pocsag_active;
    BOOL configured_invert;

    BOOL sync_locked;
    DWORD sync_found;
    DWORD sync_lost;
    int last_sync_distance;
    int best_sync_distance;
    BOOL last_sync_inverted;

    DWORD codewords_total;
    DWORD codewords_good;
    DWORD codewords_corrected;
    DWORD codewords_uncorrectable;

    BOOL raw_log_enabled;
    BOOL raw_log_limit_reached;
    DWORD raw_log_bytes;
    DWORD raw_log_limit;
    char raw_log_path[MAX_PATH];
} RX_DIAG_SNAPSHOT;

void RxDiagnosticsInit(void);
void RxDiagnosticsReset(void);

void RxDiagnosticsOnComOpen(int com_port, const DCB *dcb);
void RxDiagnosticsOnComClosed(void);
void RxDiagnosticsOnComOpenError(DWORD error_code);
void RxDiagnosticsOnRead(const BYTE *data, DWORD length, BOOL four_level);
void RxDiagnosticsOnReadError(DWORD error_code);
void RxDiagnosticsOnCommStatus(DWORD errors, DWORD input_queue);

void RxDiagnosticsSetConfiguredInvert(BOOL invert);
void RxDiagnosticsOnPreambleCounters(int count_512, int count_1200, int count_2400);
void RxDiagnosticsOnRateDetected(int baud);
void RxDiagnosticsOnPocsagInactive(void);
void RxDiagnosticsOnSyncSearch(void);
void RxDiagnosticsOnSyncSearchDistance(int distance, BOOL inverted_candidate);
void RxDiagnosticsOnSyncFound(int distance, BOOL inverted);
void RxDiagnosticsOnSyncLost(void);
void RxDiagnosticsOnCodeword(int corrected_bits);

BOOL RxDiagnosticsStartRawLog(const char *base_directory, DWORD limit_bytes);
void RxDiagnosticsStopRawLog(void);

void RxDiagnosticsGetSnapshot(RX_DIAG_SNAPSHOT *snapshot);
void RxDiagnosticsFormatSnapshot(const RX_DIAG_SNAPSHOT *snapshot, char *buffer, size_t buffer_size);
BOOL RxDiagnosticsCopySnapshotToClipboard(HWND owner);
