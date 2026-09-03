#include "preservation_capture.h"

#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <iterator>
#include <string>

int main()
{
    const char *path = "pdw-preservation-capture-test.jsonl";
    remove(path);

    _putenv_s("PDW_PRESERVATION_CAPTURE", "");

    PreservationCaptureMessage(
        "0000000", "00:00:00", "00-00-00", "NONE", "NONE", "0",
        "disabled", "", 128);

    {
        std::ifstream disabled_file(path, std::ios::binary);
        if (disabled_file.good())
        {
            fprintf(stderr, "capture file was created while capture was disabled\n");
            remove(path);
            return 1;
        }
    }

    if (_putenv_s("PDW_PRESERVATION_CAPTURE", path) != 0)
    {
        fprintf(stderr, "failed to enable preservation capture for the test\n");
        return 2;
    }

    const char escaped_message[] = {
        'A', '"', '\\', '\n', '\x01', (char)0xE9, '\0'
    };

    PreservationCaptureMessage(
        "1234567",
        "12:34:56",
        "03-09-26",
        "POCSAG",
        "ALPHA",
        "1200",
        escaped_message,
        "",
        128);

    PreservationCaptureMessage(
        "7654321",
        "12:35:00",
        "03-09-26",
        "FLEX",
        "ALPHA",
        "1600",
        "second",
        "",
        128);

    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        fprintf(stderr, "capture file was not created\n");
        return 3;
    }

    const std::string actual(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    const std::string expected =
        "{\"schema\":\"pdw-preservation-v1\",\"capture_point\":\"decoder_to_showmessage\",\"capcode\":\"1234567\",\"time\":\"12:34:56\",\"date\":\"03-09-26\",\"mode\":\"POCSAG\",\"type\":\"ALPHA\",\"bitrate\":\"1200\",\"message\":\"A\\\"\\\\\\n\\u0001\\u00E9\",\"mobitex\":\"\"}\n"
        "{\"schema\":\"pdw-preservation-v1\",\"capture_point\":\"decoder_to_showmessage\",\"capcode\":\"7654321\",\"time\":\"12:35:00\",\"date\":\"03-09-26\",\"mode\":\"FLEX\",\"type\":\"ALPHA\",\"bitrate\":\"1600\",\"message\":\"second\",\"mobitex\":\"\"}\n";

    if (actual != expected)
    {
        fprintf(stderr, "capture output differed from the canonical JSONL contract\n");
        fprintf(stderr, "actual:\n%s", actual.c_str());
        remove(path);
        return 4;
    }

    _putenv_s("PDW_PRESERVATION_CAPTURE", "");
    remove(path);
    return 0;
}
