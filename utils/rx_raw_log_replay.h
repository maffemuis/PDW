#pragma once

#include <windows.h>
#include <stddef.h>

#define RX_RAW_REPLAY_DEFAULT_FILE_LIMIT (16UL * 1024UL * 1024UL)
#define RX_RAW_REPLAY_MAX_CHUNK 4096UL

typedef BOOL (*RX_RAW_REPLAY_SINK)(DWORD relative_ms, const BYTE *data, DWORD length, void *context);

enum RX_RAW_REPLAY_RESULT
{
    RX_RAW_REPLAY_OK = 0,
    RX_RAW_REPLAY_INVALID_ARGUMENT = 1,
    RX_RAW_REPLAY_OPEN_FAILED = 2,
    RX_RAW_REPLAY_FILE_TOO_LARGE = 3,
    RX_RAW_REPLAY_LINE_TOO_LONG = 4,
    RX_RAW_REPLAY_BAD_FORMAT = 5,
    RX_RAW_REPLAY_BAD_LENGTH = 6,
    RX_RAW_REPLAY_BAD_HEX = 7,
    RX_RAW_REPLAY_BAD_BITS = 8,
    RX_RAW_REPLAY_NON_MONOTONIC_TIME = 9,
    RX_RAW_REPLAY_SINK_FAILED = 10,
    RX_RAW_REPLAY_IO_ERROR = 11
};

typedef struct RX_RAW_REPLAY_STATS
{
    DWORD rx_records;
    DWORD rx_bytes;
    DWORD first_relative_ms;
    DWORD last_relative_ms;
} RX_RAW_REPLAY_STATS;

int RxRawReplayFile(const char *path,
                    DWORD file_limit,
                    RX_RAW_REPLAY_SINK sink,
                    void *context,
                    RX_RAW_REPLAY_STATS *stats,
                    char *error,
                    size_t error_size);

const char *RxRawReplayError(int result);
