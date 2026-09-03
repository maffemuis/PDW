#ifndef PDW_PRESERVATION_GOLDEN_H
#define PDW_PRESERVATION_GOLDEN_H

#include <stddef.h>

enum PreservationGoldenResult
{
    PRESERVATION_GOLDEN_OK = 0,
    PRESERVATION_GOLDEN_EXPECTED_OPEN_FAILED = 1,
    PRESERVATION_GOLDEN_ACTUAL_OPEN_FAILED = 2,
    PRESERVATION_GOLDEN_DIFFERENT = 3
};

int PreservationCompareGoldenFiles(
    const char *expected_path,
    const char *actual_path,
    size_t *different_line,
    char *error,
    size_t error_size);

#endif
