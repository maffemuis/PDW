#include "preservation_wav.h"

#include <stdio.h>
#include <string.h>

#include <vector>

namespace
{
void SetError(char *error, size_t error_size, const char *text)
{
    if (!error || error_size == 0)
    {
        return;
    }

    if (!text)
    {
        error[0] = '\0';
        return;
    }

    snprintf(error, error_size, "%s", text);
    error[error_size - 1] = '\0';
}

unsigned short ReadLe16(const unsigned char *p)
{
    return (unsigned short)(p[0] | ((unsigned short)p[1] << 8));
}

unsigned int ReadLe32(const unsigned char *p)
{
    return (unsigned int)p[0]
        | ((unsigned int)p[1] << 8)
        | ((unsigned int)p[2] << 16)
        | ((unsigned int)p[3] << 24);
}

bool ReadExact(FILE *file, void *buffer, size_t bytes)
{
    return fread(buffer, 1, bytes, file) == bytes;
}

bool SkipChunk(FILE *file, unsigned int chunk_size)
{
    if (chunk_size > 0x7fffffffU)
    {
        return false;
    }

    if (fseek(file, (long)chunk_size, SEEK_CUR) != 0)
    {
        return false;
    }

    if (chunk_size & 1U)
    {
        if (fseek(file, 1, SEEK_CUR) != 0)
        {
            return false;
        }
    }

    return true;
}
}

int PreservationReplayPcm8Wav(
    const char *path,
    size_t chunk_bytes,
    PreservationPcm8Sink sink,
    void *context,
    PreservationWavInfo *info,
    char *error,
    size_t error_size)
{
    SetError(error, error_size, "");

    if (!path || !path[0])
    {
        SetError(error, error_size, "recording path is empty");
        return PRESERVATION_WAV_OPEN_FAILED;
    }

    FILE *file = fopen(path, "rb");
    if (!file)
    {
        SetError(error, error_size, "unable to open recording");
        return PRESERVATION_WAV_OPEN_FAILED;
    }

    unsigned char riff[12];
    if (!ReadExact(file, riff, sizeof(riff))
        || memcmp(riff, "RIFF", 4) != 0
        || memcmp(riff + 8, "WAVE", 4) != 0)
    {
        fclose(file);
        SetError(error, error_size, "not a RIFF/WAVE file");
        return PRESERVATION_WAV_INVALID_RIFF;
    }

    bool have_format = false;
    unsigned short format_tag = 0;
    unsigned short channels = 0;
    unsigned int sample_rate = 0;
    unsigned int byte_rate = 0;
    unsigned short block_align = 0;
    unsigned short bits_per_sample = 0;
    long data_offset = -1;
    unsigned int data_bytes = 0;

    for (;;)
    {
        unsigned char chunk_header[8];
        const size_t got = fread(chunk_header, 1, sizeof(chunk_header), file);
        if (got == 0)
        {
            break;
        }
        if (got != sizeof(chunk_header))
        {
            fclose(file);
            SetError(error, error_size, "truncated WAV chunk header");
            return PRESERVATION_WAV_IO_ERROR;
        }

        const unsigned int chunk_size = ReadLe32(chunk_header + 4);

        if (memcmp(chunk_header, "fmt ", 4) == 0)
        {
            if (chunk_size < 16)
            {
                fclose(file);
                SetError(error, error_size, "WAV fmt chunk is too short");
                return PRESERVATION_WAV_MISSING_FORMAT;
            }

            unsigned char fmt[16];
            if (!ReadExact(file, fmt, sizeof(fmt)))
            {
                fclose(file);
                SetError(error, error_size, "truncated WAV fmt chunk");
                return PRESERVATION_WAV_IO_ERROR;
            }

            format_tag = ReadLe16(fmt + 0);
            channels = ReadLe16(fmt + 2);
            sample_rate = ReadLe32(fmt + 4);
            byte_rate = ReadLe32(fmt + 8);
            block_align = ReadLe16(fmt + 12);
            bits_per_sample = ReadLe16(fmt + 14);
            have_format = true;

            const unsigned int remaining = chunk_size - 16U;
            if (!SkipChunk(file, remaining))
            {
                fclose(file);
                SetError(error, error_size, "unable to skip extended WAV fmt data");
                return PRESERVATION_WAV_IO_ERROR;
            }
        }
        else if (memcmp(chunk_header, "data", 4) == 0)
        {
            data_offset = ftell(file);
            data_bytes = chunk_size;

            if (data_offset < 0 || !SkipChunk(file, chunk_size))
            {
                fclose(file);
                SetError(error, error_size, "invalid WAV data chunk");
                return PRESERVATION_WAV_IO_ERROR;
            }
        }
        else
        {
            if (!SkipChunk(file, chunk_size))
            {
                fclose(file);
                SetError(error, error_size, "unable to skip WAV chunk");
                return PRESERVATION_WAV_IO_ERROR;
            }
        }

        if (have_format && data_offset >= 0)
        {
            break;
        }
    }

    if (!have_format)
    {
        fclose(file);
        SetError(error, error_size, "WAV fmt chunk was not found");
        return PRESERVATION_WAV_MISSING_FORMAT;
    }

    if (data_offset < 0)
    {
        fclose(file);
        SetError(error, error_size, "WAV data chunk was not found");
        return PRESERVATION_WAV_MISSING_DATA;
    }

    if (format_tag != 1
        || channels != 1
        || bits_per_sample != 8
        || block_align != 1
        || byte_rate != sample_rate)
    {
        fclose(file);
        SetError(error, error_size, "PDW preservation replay requires mono 8-bit PCM WAV");
        return PRESERVATION_WAV_UNSUPPORTED_FORMAT;
    }

    if (info)
    {
        info->sample_rate = sample_rate;
        info->channels = channels;
        info->bits_per_sample = bits_per_sample;
        info->data_bytes = data_bytes;
    }

    if (!sink || data_bytes == 0)
    {
        fclose(file);
        return PRESERVATION_WAV_OK;
    }

    if (fseek(file, data_offset, SEEK_SET) != 0)
    {
        fclose(file);
        SetError(error, error_size, "unable to seek to WAV sample data");
        return PRESERVATION_WAV_IO_ERROR;
    }

    if (chunk_bytes == 0)
    {
        chunk_bytes = 8192;
    }

    std::vector<unsigned char> buffer(chunk_bytes);
    unsigned int remaining = data_bytes;

    while (remaining > 0)
    {
        const size_t want = remaining < buffer.size()
            ? (size_t)remaining
            : buffer.size();
        const size_t count = fread(&buffer[0], 1, want, file);

        if (count != want)
        {
            fclose(file);
            SetError(error, error_size, "truncated WAV sample data");
            return PRESERVATION_WAV_IO_ERROR;
        }

        if (!sink(&buffer[0], count, context))
        {
            fclose(file);
            SetError(error, error_size, "replay sink rejected sample data");
            return PRESERVATION_WAV_SINK_FAILED;
        }

        remaining -= (unsigned int)count;
    }

    fclose(file);
    return PRESERVATION_WAV_OK;
}
