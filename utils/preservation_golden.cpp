#include "preservation_golden.h"

#include <stdio.h>

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
}

int PreservationCompareGoldenFiles(
    const char *expected_path,
    const char *actual_path,
    size_t *different_line,
    char *error,
    size_t error_size)
{
    if (different_line)
    {
        *different_line = 0;
    }
    SetError(error, error_size, "");

    FILE *expected = fopen(expected_path, "rb");
    if (!expected)
    {
        SetError(error, error_size, "unable to open expected golden file");
        return PRESERVATION_GOLDEN_EXPECTED_OPEN_FAILED;
    }

    FILE *actual = fopen(actual_path, "rb");
    if (!actual)
    {
        fclose(expected);
        SetError(error, error_size, "unable to open actual golden file");
        return PRESERVATION_GOLDEN_ACTUAL_OPEN_FAILED;
    }

    size_t line = 1;

    for (;;)
    {
        const int expected_byte = fgetc(expected);
        const int actual_byte = fgetc(actual);

        if (expected_byte != actual_byte)
        {
            if (different_line)
            {
                *different_line = line;
            }

            char detail[160];
            snprintf(
                detail,
                sizeof(detail),
                "golden output differs at line %u (expected byte %d, actual byte %d)",
                (unsigned int)line,
                expected_byte,
                actual_byte);
            detail[sizeof(detail) - 1] = '\0';
            SetError(error, error_size, detail);

            fclose(expected);
            fclose(actual);
            return PRESERVATION_GOLDEN_DIFFERENT;
        }

        if (expected_byte == EOF)
        {
            break;
        }

        if (expected_byte == '\n')
        {
            line++;
        }
    }

    fclose(expected);
    fclose(actual);
    return PRESERVATION_GOLDEN_OK;
}
