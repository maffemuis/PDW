#include "rx_raw_log_replay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{
void SetError(char *error, size_t error_size, const char *text)
{
    if (!error || error_size == 0) return;
    strncpy(error, text ? text : "", error_size - 1);
    error[error_size - 1] = '\0';
}

int HexNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool IsIgnoredLine(const char *line)
{
    return line[0] == '\0' ||
           strcmp(line, "PDW RAW RX LOG") == 0 ||
           strncmp(line, "format: ", 8) == 0 ||
           strncmp(line, "events: ", 8) == 0 ||
           strncmp(line, "limit_bytes=", 12) == 0 ||
           strcmp(line, "RAW_LOG_STOPPED") == 0 ||
           strcmp(line, "RAW_LOG_LIMIT_REACHED") == 0 ||
           strstr(line, " EV ") != NULL;
}

bool VerifyBits(const char *bits, const BYTE *data, DWORD length)
{
    for (DWORD i = 0; i < length; ++i)
    {
        for (int bit = 7; bit >= 0; --bit)
        {
            const char expected = ((data[i] >> bit) & 1) ? '1' : '0';
            if (*bits++ != expected) return false;
        }
        if (i + 1 < length)
        {
            if (*bits++ != ' ') return false;
        }
    }
    return *bits == '\0';
}

BOOL ReplayRs232Sink(DWORD relative_ms, const BYTE *data, DWORD length, void *context)
{
    (void)relative_ms;
    return RxRs232FeedBytes((RX_RS232_BOUNDARY *)context, data, length);
}
}

BOOL RxRs232FeedBytes(RX_RS232_BOUNDARY *boundary, const BYTE *data, DWORD length)
{
    if (!boundary || !boundary->freqdata || !boundary->linedata || !boundary->position ||
        boundary->capacity == 0 || *boundary->position >= boundary->capacity ||
        (length != 0 && !data))
        return FALSE;

    for (DWORD i = 0; i < length; ++i)
    {
        if (boundary->fourlevel)
        {
            for (int shift = 6; shift >= 0; shift -= 2)
            {
                const int symbol = (data[i] >> shift) & 3;
                boundary->linedata[*boundary->position] = (BYTE)(symbol << 4);
                boundary->freqdata[*boundary->position] = (WORD)boundary->timing;
                ++*boundary->position;
                if (*boundary->position >= boundary->capacity) *boundary->position = 0;
            }
        }
        else
        {
            for (int shift = 7; shift >= 0; --shift)
            {
                const int bit = (data[i] >> shift) & 1;
                boundary->linedata[*boundary->position] = (BYTE)(bit << 4);
                boundary->freqdata[*boundary->position] = (WORD)boundary->timing;
                ++*boundary->position;
                if (*boundary->position >= boundary->capacity) *boundary->position = 0;
            }
        }
    }
    return TRUE;
}

const char *RxRawReplayError(int result)
{
    switch (result)
    {
        case RX_RAW_REPLAY_OK: return "OK";
        case RX_RAW_REPLAY_INVALID_ARGUMENT: return "invalid replay argument";
        case RX_RAW_REPLAY_OPEN_FAILED: return "raw RX log could not be opened";
        case RX_RAW_REPLAY_FILE_TOO_LARGE: return "raw RX log exceeds the configured size limit";
        case RX_RAW_REPLAY_LINE_TOO_LONG: return "raw RX log contains an overlong record";
        case RX_RAW_REPLAY_BAD_FORMAT: return "raw RX log contains an invalid record";
        case RX_RAW_REPLAY_BAD_LENGTH: return "raw RX log byte count does not match its payload";
        case RX_RAW_REPLAY_BAD_HEX: return "raw RX log contains invalid hexadecimal data";
        case RX_RAW_REPLAY_BAD_BITS: return "raw RX log bit rendering does not match its bytes";
        case RX_RAW_REPLAY_NON_MONOTONIC_TIME: return "raw RX log timestamps are not monotonic";
        case RX_RAW_REPLAY_SINK_FAILED: return "raw RX replay sink rejected a record";
        case RX_RAW_REPLAY_IO_ERROR: return "raw RX log read failed";
        default: return "unknown raw RX replay error";
    }
}

int RxRawReplayFile(const char *path,
                    DWORD file_limit,
                    RX_RAW_REPLAY_SINK sink,
                    void *context,
                    RX_RAW_REPLAY_STATS *stats,
                    char *error,
                    size_t error_size)
{
    if (stats) ZeroMemory(stats, sizeof(*stats));
    SetError(error, error_size, "");

    if (!path || !path[0] || !sink)
    {
        SetError(error, error_size, RxRawReplayError(RX_RAW_REPLAY_INVALID_ARGUMENT));
        return RX_RAW_REPLAY_INVALID_ARGUMENT;
    }

    FILE *file = fopen(path, "rb");
    if (!file)
    {
        SetError(error, error_size, RxRawReplayError(RX_RAW_REPLAY_OPEN_FAILED));
        return RX_RAW_REPLAY_OPEN_FAILED;
    }

    const DWORD limit = file_limit ? file_limit : RX_RAW_REPLAY_DEFAULT_FILE_LIMIT;
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        SetError(error, error_size, RxRawReplayError(RX_RAW_REPLAY_IO_ERROR));
        return RX_RAW_REPLAY_IO_ERROR;
    }
    const long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        SetError(error, error_size, RxRawReplayError(RX_RAW_REPLAY_IO_ERROR));
        return RX_RAW_REPLAY_IO_ERROR;
    }
    if ((unsigned long)size > (unsigned long)limit)
    {
        fclose(file);
        SetError(error, error_size, RxRawReplayError(RX_RAW_REPLAY_FILE_TOO_LARGE));
        return RX_RAW_REPLAY_FILE_TOO_LARGE;
    }

    char line[16384];
    BYTE bytes[RX_RAW_REPLAY_MAX_CHUNK];
    bool have_timestamp = false;
    DWORD last_timestamp = 0;
    int result = RX_RAW_REPLAY_OK;

    while (fgets(line, sizeof(line), file))
    {
        const size_t raw_length = strlen(line);
        if (raw_length == sizeof(line) - 1 && line[raw_length - 1] != '\n')
        {
            result = RX_RAW_REPLAY_LINE_TOO_LONG;
            break;
        }

        while (line[0] && (line[strlen(line) - 1] == '\r' || line[strlen(line) - 1] == '\n'))
            line[strlen(line) - 1] = '\0';

        if (IsIgnoredLine(line)) continue;

        char *end = NULL;
        const unsigned long timestamp_value = strtoul(line, &end, 10);
        if (end == line || !end || strncmp(end, " RX bytes=", 10) != 0 || timestamp_value > 0xffffffffUL)
        {
            result = RX_RAW_REPLAY_BAD_FORMAT;
            break;
        }
        const DWORD timestamp = (DWORD)timestamp_value;
        if (have_timestamp && timestamp < last_timestamp)
        {
            result = RX_RAW_REPLAY_NON_MONOTONIC_TIME;
            break;
        }

        const char *count_text = end + 10;
        char *count_end = NULL;
        const unsigned long count_value = strtoul(count_text, &count_end, 10);
        if (count_end == count_text || !count_end || strncmp(count_end, " hex=", 5) != 0 ||
            count_value == 0 || count_value > RX_RAW_REPLAY_MAX_CHUNK)
        {
            result = RX_RAW_REPLAY_BAD_LENGTH;
            break;
        }
        const DWORD count = (DWORD)count_value;
        const char *hex = count_end + 5;
        const char *bits_marker = strstr(hex, " bits=");
        if (!bits_marker || (size_t)(bits_marker - hex) != (size_t)count * 2U)
        {
            result = RX_RAW_REPLAY_BAD_LENGTH;
            break;
        }

        for (DWORD i = 0; i < count; ++i)
        {
            const int hi = HexNibble(hex[i * 2]);
            const int lo = HexNibble(hex[i * 2 + 1]);
            if (hi < 0 || lo < 0)
            {
                result = RX_RAW_REPLAY_BAD_HEX;
                break;
            }
            bytes[i] = (BYTE)((hi << 4) | lo);
        }
        if (result != RX_RAW_REPLAY_OK) break;

        if (!VerifyBits(bits_marker + 6, bytes, count))
        {
            result = RX_RAW_REPLAY_BAD_BITS;
            break;
        }

        if (!sink(timestamp, bytes, count, context))
        {
            result = RX_RAW_REPLAY_SINK_FAILED;
            break;
        }

        if (stats)
        {
            if (stats->rx_records == 0) stats->first_relative_ms = timestamp;
            stats->last_relative_ms = timestamp;
            ++stats->rx_records;
            stats->rx_bytes += count;
        }
        have_timestamp = true;
        last_timestamp = timestamp;
    }

    if (result == RX_RAW_REPLAY_OK && ferror(file)) result = RX_RAW_REPLAY_IO_ERROR;
    fclose(file);

    if (result != RX_RAW_REPLAY_OK) SetError(error, error_size, RxRawReplayError(result));
    return result;
}

int RxRawReplayFileToRs232Boundary(const char *path,
                                   DWORD file_limit,
                                   RX_RS232_BOUNDARY *boundary,
                                   RX_RAW_REPLAY_STATS *stats,
                                   char *error,
                                   size_t error_size)
{
    if (!boundary || !boundary->freqdata || !boundary->linedata || !boundary->position ||
        boundary->capacity == 0 || *boundary->position >= boundary->capacity)
    {
        if (stats) ZeroMemory(stats, sizeof(*stats));
        SetError(error, error_size, RxRawReplayError(RX_RAW_REPLAY_INVALID_ARGUMENT));
        return RX_RAW_REPLAY_INVALID_ARGUMENT;
    }
    return RxRawReplayFile(path, file_limit, ReplayRs232Sink, boundary, stats, error, error_size);
}
