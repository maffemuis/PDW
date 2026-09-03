#include "preservation_capture.h"

#include <stdio.h>
#include <stdlib.h>

namespace
{
void WriteJsonString(FILE *file, const char *value, size_t max_bytes)
{
    fputc('"', file);

    if (value)
    {
        for (size_t i = 0; i < max_bytes && value[i] != '\0'; ++i)
        {
            const unsigned char ch = (unsigned char)value[i];

            switch (ch)
            {
                case '"':
                    fputs("\\\"", file);
                    break;
                case '\\':
                    fputs("\\\\", file);
                    break;
                case '\b':
                    fputs("\\b", file);
                    break;
                case '\f':
                    fputs("\\f", file);
                    break;
                case '\n':
                    fputs("\\n", file);
                    break;
                case '\r':
                    fputs("\\r", file);
                    break;
                case '\t':
                    fputs("\\t", file);
                    break;
                default:
                    if (ch < 0x20 || ch >= 0x7f)
                    {
                        fprintf(file, "\\u%04X", (unsigned int)ch);
                    }
                    else
                    {
                        fputc(ch, file);
                    }
                    break;
            }
        }
    }

    fputc('"', file);
}

void WriteField(FILE *file, const char *name, const char *value, size_t max_bytes)
{
    fputs(",\"", file);
    fputs(name, file);
    fputs("\":", file);
    WriteJsonString(file, value, max_bytes);
}
}

void PreservationCaptureMessage(
    const char *capcode,
    const char *time,
    const char *date,
    const char *mode,
    const char *type,
    const char *bitrate,
    const char *message,
    const char *mobitex,
    size_t max_field_bytes)
{
    const char *path = getenv("PDW_PRESERVATION_CAPTURE");

    if (!path || !path[0])
    {
        return;
    }

    FILE *file = fopen(path, "ab");
    if (!file)
    {
        return;
    }

    fputs("{\"schema\":\"pdw-preservation-v1\"", file);
    fputs(",\"capture_point\":\"decoder_to_showmessage\"", file);
    WriteField(file, "capcode", capcode, max_field_bytes);
    WriteField(file, "time", time, max_field_bytes);
    WriteField(file, "date", date, max_field_bytes);
    WriteField(file, "mode", mode, max_field_bytes);
    WriteField(file, "type", type, max_field_bytes);
    WriteField(file, "bitrate", bitrate, max_field_bytes);
    WriteField(file, "message", message, max_field_bytes);
    WriteField(file, "mobitex", mobitex, max_field_bytes);
    fputs("}\n", file);

    fclose(file);
}
