#include "preservation_wav.h"

#include <stdio.h>
#include <string.h>

#include <vector>

namespace
{
void WriteLe16(FILE *file, unsigned short value)
{
    unsigned char b[2] = {
        (unsigned char)(value & 0xff),
        (unsigned char)((value >> 8) & 0xff)
    };
    fwrite(b, 1, sizeof(b), file);
}

void WriteLe32(FILE *file, unsigned int value)
{
    unsigned char b[4] = {
        (unsigned char)(value & 0xff),
        (unsigned char)((value >> 8) & 0xff),
        (unsigned char)((value >> 16) & 0xff),
        (unsigned char)((value >> 24) & 0xff)
    };
    fwrite(b, 1, sizeof(b), file);
}

bool WriteWave(const char *path, unsigned short bits_per_sample)
{
    FILE *file = fopen(path, "wb+");
    if (!file)
    {
        return false;
    }

    fwrite("RIFF", 1, 4, file);
    WriteLe32(file, 0);
    fwrite("WAVE", 1, 4, file);

    fwrite("JUNK", 1, 4, file);
    WriteLe32(file, 3);
    fwrite("abc", 1, 3, file);
    fputc(0, file);

    fwrite("fmt ", 1, 4, file);
    WriteLe32(file, 16);
    WriteLe16(file, 1);
    WriteLe16(file, 1);
    WriteLe32(file, 44100);

    const unsigned short bytes_per_sample = (unsigned short)(bits_per_sample / 8);
    WriteLe32(file, 44100U * bytes_per_sample);
    WriteLe16(file, bytes_per_sample);
    WriteLe16(file, bits_per_sample);

    const unsigned char samples[] = { 0, 64, 128, 255, 1 };
    fwrite("data", 1, 4, file);
    WriteLe32(file, (unsigned int)sizeof(samples));
    fwrite(samples, 1, sizeof(samples), file);
    fputc(0, file);

    const long end = ftell(file);
    if (end < 8 || fseek(file, 4, SEEK_SET) != 0)
    {
        fclose(file);
        return false;
    }

    WriteLe32(file, (unsigned int)(end - 8));
    fclose(file);
    return true;
}

struct SinkState
{
    std::vector<unsigned char> samples;
    int calls;
};

bool CollectSamples(const unsigned char *samples, size_t count, void *context)
{
    SinkState *state = (SinkState *)context;
    state->samples.insert(state->samples.end(), samples, samples + count);
    state->calls++;
    return true;
}
}

int main()
{
    const char *valid_path = "pdw-preservation-valid.wav";
    const char *invalid_path = "pdw-preservation-invalid.wav";
    remove(valid_path);
    remove(invalid_path);

    if (!WriteWave(valid_path, 8) || !WriteWave(invalid_path, 16))
    {
        fprintf(stderr, "failed to create WAV fixtures\n");
        return 1;
    }

    PreservationWavInfo info = {0};
    SinkState state;
    state.calls = 0;
    char error[256] = {0};

    const int result = PreservationReplayPcm8Wav(
        valid_path,
        2,
        CollectSamples,
        &state,
        &info,
        error,
        sizeof(error));

    const unsigned char expected[] = { 0, 64, 128, 255, 1 };
    if (result != PRESERVATION_WAV_OK
        || info.sample_rate != 44100
        || info.channels != 1
        || info.bits_per_sample != 8
        || info.data_bytes != sizeof(expected)
        || state.calls != 3
        || state.samples.size() != sizeof(expected)
        || memcmp(&state.samples[0], expected, sizeof(expected)) != 0)
    {
        fprintf(stderr, "valid WAV replay failed: result=%d error=%s\n", result, error);
        remove(valid_path);
        remove(invalid_path);
        return 2;
    }

    memset(error, 0, sizeof(error));
    const int invalid_result = PreservationReplayPcm8Wav(
        invalid_path,
        8192,
        CollectSamples,
        &state,
        &info,
        error,
        sizeof(error));

    if (invalid_result != PRESERVATION_WAV_UNSUPPORTED_FORMAT || error[0] == '\0')
    {
        fprintf(stderr, "unsupported WAV format was not rejected\n");
        remove(valid_path);
        remove(invalid_path);
        return 3;
    }

    remove(valid_path);
    remove(invalid_path);
    return 0;
}
