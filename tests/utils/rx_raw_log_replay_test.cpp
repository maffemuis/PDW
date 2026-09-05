#include "rx_raw_log_replay.h"

#include <stdio.h>
#include <string.h>

struct SinkState
{
    DWORD calls;
    DWORD bytes;
    DWORD timestamps[4];
    BYTE data[16];
};

static bool Check(bool condition, const char *name)
{
    if (condition) return true;
    fprintf(stderr, "RX_RAW_LOG_REPLAY_TEST_FAIL: %s\n", name);
    return false;
}

static BOOL Sink(DWORD relative_ms, const BYTE *data, DWORD length, void *context)
{
    SinkState *state = (SinkState *)context;
    if (!state || state->calls >= 4 || state->bytes + length > sizeof(state->data)) return FALSE;
    state->timestamps[state->calls++] = relative_ms;
    memcpy(state->data + state->bytes, data, length);
    state->bytes += length;
    return TRUE;
}

static bool WriteFileText(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    const size_t length = strlen(text);
    const bool ok = fwrite(text, 1, length, file) == length;
    fclose(file);
    return ok;
}

int main()
{
    const char *path = "pdw-rx-replay-test.log";
    const char *valid =
        "PDW RAW RX LOG\r\n"
        "format: relative_ms RX bytes=<n> hex=<bytes> bits=<MSB-to-LSB>\r\n"
        "events: decoder state only; no SMTP credentials or message payload text\r\n"
        "limit_bytes=8192\r\n"
        "0000000001 EV RAW_LOG_STARTED path=test\r\n"
        "0000000010 RX bytes=2 hex=A55A bits=10100101 01011010\r\n"
        "0000000025 RX bytes=1 hex=00 bits=00000000\r\n"
        "RAW_LOG_STOPPED\r\n";

    if (!Check(WriteFileText(path, valid), "write valid fixture")) return 1;

    SinkState state = {};
    RX_RAW_REPLAY_STATS stats = {};
    char error[256] = {0};
    int result = RxRawReplayFile(path, 8192, Sink, &state, &stats, error, sizeof(error));
    remove(path);

    if (!Check(result == RX_RAW_REPLAY_OK, "valid replay")) return 2;
    if (!Check(state.calls == 2 && state.bytes == 3, "sink totals")) return 3;
    if (!Check(state.timestamps[0] == 10 && state.timestamps[1] == 25, "timestamps")) return 4;
    if (!Check(state.data[0] == 0xA5 && state.data[1] == 0x5A && state.data[2] == 0x00, "byte preservation")) return 5;
    if (!Check(stats.rx_records == 2 && stats.rx_bytes == 3, "stats totals")) return 6;
    if (!Check(stats.first_relative_ms == 10 && stats.last_relative_ms == 25, "stats timestamps")) return 7;

    const char *bad_bits = "0000000010 RX bytes=1 hex=A5 bits=10100100\r\n";
    if (!Check(WriteFileText(path, bad_bits), "write bad bits")) return 8;
    state = SinkState();
    result = RxRawReplayFile(path, 8192, Sink, &state, NULL, error, sizeof(error));
    remove(path);
    if (!Check(result == RX_RAW_REPLAY_BAD_BITS && state.calls == 0, "reject bit mismatch")) return 9;

    const char *bad_length = "0000000010 RX bytes=2 hex=A5 bits=10100101\r\n";
    if (!Check(WriteFileText(path, bad_length), "write bad length")) return 10;
    result = RxRawReplayFile(path, 8192, Sink, &state, NULL, error, sizeof(error));
    remove(path);
    if (!Check(result == RX_RAW_REPLAY_BAD_LENGTH, "reject length mismatch")) return 11;

    const char *backwards =
        "0000000020 RX bytes=1 hex=00 bits=00000000\r\n"
        "0000000019 RX bytes=1 hex=00 bits=00000000\r\n";
    if (!Check(WriteFileText(path, backwards), "write backwards time")) return 12;
    state = SinkState();
    result = RxRawReplayFile(path, 8192, Sink, &state, NULL, error, sizeof(error));
    remove(path);
    if (!Check(result == RX_RAW_REPLAY_NON_MONOTONIC_TIME && state.calls == 1, "reject backwards time")) return 13;

    if (!Check(WriteFileText(path, valid), "write size fixture")) return 14;
    result = RxRawReplayFile(path, 8, Sink, &state, NULL, error, sizeof(error));
    remove(path);
    if (!Check(result == RX_RAW_REPLAY_FILE_TOO_LARGE, "bounded input")) return 15;

    fprintf(stdout, "RX_RAW_LOG_REPLAY_TEST_PASS\n");
    return 0;
}
