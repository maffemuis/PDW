#ifndef PDW_PRESERVATION_WAV_H
#define PDW_PRESERVATION_WAV_H

#include <stddef.h>

typedef struct
{
    unsigned int sample_rate;
    unsigned short channels;
    unsigned short bits_per_sample;
    unsigned int data_bytes;
} PreservationWavInfo;

typedef bool (*PreservationPcm8Sink)(
    const unsigned char *samples,
    size_t sample_count,
    void *context);

enum PreservationWavResult
{
    PRESERVATION_WAV_OK = 0,
    PRESERVATION_WAV_OPEN_FAILED = 1,
    PRESERVATION_WAV_INVALID_RIFF = 2,
    PRESERVATION_WAV_MISSING_FORMAT = 3,
    PRESERVATION_WAV_MISSING_DATA = 4,
    PRESERVATION_WAV_UNSUPPORTED_FORMAT = 5,
    PRESERVATION_WAV_IO_ERROR = 6,
    PRESERVATION_WAV_SINK_FAILED = 7
};

int PreservationReplayPcm8Wav(
    const char *path,
    size_t chunk_bytes,
    PreservationPcm8Sink sink,
    void *context,
    PreservationWavInfo *info,
    char *error,
    size_t error_size);

#endif
