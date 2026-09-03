#include "preservation_decoder_replay_policy.h"

#include <stdio.h>
#include <string.h>

namespace
{
int ExpectResult(
    const char *name,
    int paging,
    int acars,
    int mobitex,
    int ermes,
    unsigned int configured_rate,
    unsigned int recording_rate,
    const char *capture_path,
    int expected_result,
    PreservationReplayRoute expected_route)
{
    PreservationReplayRoute route = PRESERVATION_REPLAY_ROUTE_NONE;
    const int result = PreservationValidateReplayPolicy(
        paging,
        acars,
        mobitex,
        ermes,
        configured_rate,
        recording_rate,
        capture_path,
        &route);

    if (result != expected_result || route != expected_route)
    {
        fprintf(
            stderr,
            "%s failed: result=%d route=%d error=%s\n",
            name,
            result,
            (int)route,
            PreservationReplayPolicyError(result));
        return 1;
    }

    return 0;
}
}

int main()
{
    int failures = 0;

    failures += ExpectResult(
        "paging",
        1, 0, 0, 0,
        44100, 44100,
        "capture.jsonl",
        PRESERVATION_REPLAY_POLICY_OK,
        PRESERVATION_REPLAY_ROUTE_PAGING);

    failures += ExpectResult(
        "acars",
        0, 1, 0, 0,
        44100, 44100,
        "capture.jsonl",
        PRESERVATION_REPLAY_POLICY_OK,
        PRESERVATION_REPLAY_ROUTE_ACARS);

    failures += ExpectResult(
        "mobitex",
        0, 0, 1, 0,
        44100, 44100,
        "capture.jsonl",
        PRESERVATION_REPLAY_POLICY_OK,
        PRESERVATION_REPLAY_ROUTE_MOBITEX);

    failures += ExpectResult(
        "ermes",
        0, 0, 0, 1,
        44100, 44100,
        "capture.jsonl",
        PRESERVATION_REPLAY_POLICY_OK,
        PRESERVATION_REPLAY_ROUTE_ERMES);

    failures += ExpectResult(
        "no mode",
        0, 0, 0, 0,
        44100, 44100,
        "capture.jsonl",
        PRESERVATION_REPLAY_POLICY_NO_MODE,
        PRESERVATION_REPLAY_ROUTE_NONE);

    failures += ExpectResult(
        "multiple modes",
        1, 1, 0, 0,
        44100, 44100,
        "capture.jsonl",
        PRESERVATION_REPLAY_POLICY_MULTIPLE_MODES,
        PRESERVATION_REPLAY_ROUTE_NONE);

    failures += ExpectResult(
        "sample mismatch",
        1, 0, 0, 0,
        44100, 48000,
        "capture.jsonl",
        PRESERVATION_REPLAY_POLICY_SAMPLE_RATE_MISMATCH,
        PRESERVATION_REPLAY_ROUTE_NONE);

    failures += ExpectResult(
        "missing capture",
        1, 0, 0, 0,
        44100, 44100,
        "",
        PRESERVATION_REPLAY_POLICY_CAPTURE_REQUIRED,
        PRESERVATION_REPLAY_ROUTE_NONE);

    if (strcmp(PreservationReplayRouteName(PRESERVATION_REPLAY_ROUTE_PAGING), "paging") != 0
        || strcmp(PreservationReplayRouteName(PRESERVATION_REPLAY_ROUTE_ACARS), "acars") != 0
        || strcmp(PreservationReplayRouteName(PRESERVATION_REPLAY_ROUTE_MOBITEX), "mobitex") != 0
        || strcmp(PreservationReplayRouteName(PRESERVATION_REPLAY_ROUTE_ERMES), "ermes") != 0)
    {
        fprintf(stderr, "route names are not stable\n");
        failures++;
    }

    return failures ? 1 : 0;
}
