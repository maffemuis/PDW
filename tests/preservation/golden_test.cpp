#include "preservation_golden.h"

#include <stdio.h>
#include <string.h>

namespace
{
bool WriteText(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    if (!file)
    {
        return false;
    }

    const size_t length = strlen(text);
    const bool ok = fwrite(text, 1, length, file) == length;
    fclose(file);
    return ok;
}
}

int main()
{
    const char *expected_path = "pdw-golden-expected.jsonl";
    const char *actual_path = "pdw-golden-actual.jsonl";
    remove(expected_path);
    remove(actual_path);

    const char *baseline =
        "{\"schema\":\"pdw-golden-v1\",\"message\":\"first\"}\n"
        "{\"schema\":\"pdw-golden-v1\",\"message\":\"second\"}\n";

    if (!WriteText(expected_path, baseline) || !WriteText(actual_path, baseline))
    {
        fprintf(stderr, "failed to write golden test fixtures\n");
        return 1;
    }

    size_t line = 0;
    char error[256] = {0};

    int result = PreservationCompareGoldenFiles(
        expected_path,
        actual_path,
        &line,
        error,
        sizeof(error));

    if (result != PRESERVATION_GOLDEN_OK || line != 0 || error[0] != '\0')
    {
        fprintf(stderr, "identical golden files did not compare equal\n");
        remove(expected_path);
        remove(actual_path);
        return 2;
    }

    const char *windows_baseline =
        "{\"schema\":\"pdw-golden-v1\",\"message\":\"first\"}\r\n"
        "{\"schema\":\"pdw-golden-v1\",\"message\":\"second\"}\r\n";

    if (!WriteText(expected_path, windows_baseline) || !WriteText(actual_path, baseline))
    {
        fprintf(stderr, "failed to write cross-platform newline fixtures\n");
        return 3;
    }

    memset(error, 0, sizeof(error));
    line = 0;
    result = PreservationCompareGoldenFiles(
        expected_path,
        actual_path,
        &line,
        error,
        sizeof(error));

    if (result != PRESERVATION_GOLDEN_OK || line != 0 || error[0] != '\0')
    {
        fprintf(stderr, "CRLF and LF golden files did not compare equal\n");
        remove(expected_path);
        remove(actual_path);
        return 4;
    }

    const char *changed =
        "{\"schema\":\"pdw-golden-v1\",\"message\":\"first\"}\n"
        "{\"schema\":\"pdw-golden-v1\",\"message\":\"changed\"}\n";

    if (!WriteText(actual_path, changed))
    {
        fprintf(stderr, "failed to rewrite changed golden fixture\n");
        return 5;
    }

    memset(error, 0, sizeof(error));
    line = 0;
    result = PreservationCompareGoldenFiles(
        expected_path,
        actual_path,
        &line,
        error,
        sizeof(error));

    if (result != PRESERVATION_GOLDEN_DIFFERENT || line != 2 || error[0] == '\0')
    {
        fprintf(stderr, "golden mismatch did not report line 2\n");
        remove(expected_path);
        remove(actual_path);
        return 6;
    }

    const char *missing_newline =
        "{\"schema\":\"pdw-golden-v1\",\"message\":\"first\"}\n"
        "{\"schema\":\"pdw-golden-v1\",\"message\":\"second\"}";

    if (!WriteText(actual_path, missing_newline))
    {
        fprintf(stderr, "failed to write newline mismatch fixture\n");
        return 7;
    }

    memset(error, 0, sizeof(error));
    line = 0;
    result = PreservationCompareGoldenFiles(
        expected_path,
        actual_path,
        &line,
        error,
        sizeof(error));

    if (result != PRESERVATION_GOLDEN_DIFFERENT || line != 2)
    {
        fprintf(stderr, "missing final newline was not detected\n");
        remove(expected_path);
        remove(actual_path);
        return 8;
    }

    remove(expected_path);
    remove(actual_path);
    return 0;
}
