#include "preservation_golden.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: pdw_preservation_compare <expected.jsonl> <actual.jsonl>\n");
        return 2;
    }

    size_t line = 0;
    char error[256] = {0};
    const int result = PreservationCompareGoldenFiles(
        argv[1],
        argv[2],
        &line,
        error,
        sizeof(error));

    if (result == PRESERVATION_GOLDEN_OK)
    {
        return 0;
    }

    fprintf(stderr, "%s\n", error[0] ? error : "golden comparison failed");
    return 1;
}
