#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\Headers\pdw.h"
#include "..\Headers\initapp.h"
#include "..\Headers\sound_in.h"
#include "preservation_decoder_replay_policy.h"
#include "preservation_wav.h"

BOOL LegacyStart_Capturing(void);

extern int flex_timer;
extern bool bFlexActive;
extern int flex_blk;
extern int g_sps;
extern int g_sps2;
extern int level;
extern int flex_speed;
extern FLEX phase_A;

namespace
{
struct ReplaySinkContext
{
    PreservationReplayRoute route;
};

bool ReplayExitRequested()
{
    const char *value = getenv("PDW_PRESERVATION_REPLAY_EXIT");
    return value && strcmp(value, "1") == 0;
}

void WriteFlexDiagnosticSnapshot()
{
    const char *path = getenv("PDW_PRESERVATION_FLEX_DIAGNOSTIC");
    if (!path || !path[0])
    {
        return;
    }

    FILE *file = fopen(path, "wb");
    if (!file)
    {
        return;
    }

    fprintf(
        file,
        "{\"schema\":\"pdw-flex-diagnostic-v1\","
        "\"flex_timer\":%d,\"flex_active\":%s,\"flex_blk\":%d,"
        "\"g_sps\":%d,\"g_sps2\":%d,\"level\":%d,"
        "\"flex_speed\":%d,\"baud_rate\":%ld,\"frame\":[",
        flex_timer,
        bFlexActive ? "true" : "false",
        flex_blk,
        g_sps,
        g_sps2,
        level,
        flex_speed,
        BaudRate);

    for (int i = 0; i < 16; ++i)
    {
        if (i)
        {
            fputc(',', file);
        }
        fprintf(file, "\"0x%08lX\"", (unsigned long)phase_A.frame[i]);
    }

    fputs("]}\n", file);
    fclose(file);
}

void ReportReplayError(const char *detail)
{
    char message[512];
    snprintf(
        message,
        sizeof(message),
        "Preservation replay failed.\n\n%s\n\nLive audio was not started.",
        (detail && detail[0]) ? detail : "Unknown preservation replay error.");
    message[sizeof(message) - 1] = '\0';

    if (ReplayExitRequested())
    {
        OutputDebugStringA(message);
        ExitProcess(2);
    }

    MessageBoxA(
        ghWnd,
        message,
        "PDW preservation replay",
        MB_OK | MB_ICONERROR);
}

bool FeedDecoder(
    const unsigned char *samples,
    size_t sample_count,
    void *context)
{
    ReplaySinkContext *sink = (ReplaySinkContext *)context;

    if (!sink || !samples || sample_count == 0 || sample_count > 0x7fffffffU)
    {
        return sample_count == 0;
    }

    char *legacy_samples = (char *)samples;
    const long legacy_count = (long)sample_count;

    switch (sink->route)
    {
        case PRESERVATION_REPLAY_ROUTE_PAGING:
            Audio_To_Bits(legacy_samples, legacy_count);
            return true;

        case PRESERVATION_REPLAY_ROUTE_ACARS:
            ACARS_To_Bits(legacy_samples, legacy_count);
            return true;

        case PRESERVATION_REPLAY_ROUTE_MOBITEX:
            MOBITEX_To_Bits(legacy_samples, legacy_count);
            return true;

        default:
            return false;
    }
}
}

BOOL Start_Capturing(void)
{
    const char *recording_path = getenv("PDW_PRESERVATION_REPLAY_WAV");

    if (!recording_path || !recording_path[0])
    {
        return LegacyStart_Capturing();
    }

    bCapturing = false;

    char wav_error[256] = {0};
    PreservationWavInfo info = {0};

    const int probe_result = PreservationReplayPcm8Wav(
        recording_path,
        0,
        NULL,
        NULL,
        &info,
        wav_error,
        sizeof(wav_error));

    if (probe_result != PRESERVATION_WAV_OK)
    {
        ReportReplayError(wav_error);
        return FALSE;
    }

    PreservationReplayRoute route = PRESERVATION_REPLAY_ROUTE_NONE;
    const char *capture_path = getenv("PDW_PRESERVATION_CAPTURE");

    const int policy_result = PreservationValidateReplayPolicy(
        Profile.monitor_paging,
        Profile.monitor_acars,
        Profile.monitor_mobitex,
        Profile.monitor_ermes,
        (unsigned int)Profile.audioSampleRate,
        info.sample_rate,
        capture_path,
        &route);

    if (policy_result != PRESERVATION_REPLAY_POLICY_OK)
    {
        ReportReplayError(PreservationReplayPolicyError(policy_result));
        return FALSE;
    }

    Reset_ATB();

    ReplaySinkContext sink;
    sink.route = route;

    memset(wav_error, 0, sizeof(wav_error));

    const int replay_result = PreservationReplayPcm8Wav(
        recording_path,
        8192,
        FeedDecoder,
        &sink,
        &info,
        wav_error,
        sizeof(wav_error));

    if (replay_result != PRESERVATION_WAV_OK)
    {
        ReportReplayError(wav_error);
        return FALSE;
    }

    WriteFlexDiagnosticSnapshot();

    // This mode exists only for unattended preservation runs. By this point
    // replay is complete and every capture write has been closed/flushed, so
    // a direct process exit avoids entering the legacy GUI message loop.
    if (ReplayExitRequested())
    {
        ExitProcess(0);
    }

    return TRUE;
}
