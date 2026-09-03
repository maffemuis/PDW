#ifndef PDW_PRESERVATION_DECODER_REPLAY_POLICY_H
#define PDW_PRESERVATION_DECODER_REPLAY_POLICY_H

enum PreservationReplayRoute
{
    PRESERVATION_REPLAY_ROUTE_NONE = 0,
    PRESERVATION_REPLAY_ROUTE_PAGING,
    PRESERVATION_REPLAY_ROUTE_ACARS,
    PRESERVATION_REPLAY_ROUTE_MOBITEX,
    PRESERVATION_REPLAY_ROUTE_ERMES
};

enum PreservationReplayPolicyResult
{
    PRESERVATION_REPLAY_POLICY_OK = 0,
    PRESERVATION_REPLAY_POLICY_NO_MODE,
    PRESERVATION_REPLAY_POLICY_MULTIPLE_MODES,
    PRESERVATION_REPLAY_POLICY_SAMPLE_RATE_MISMATCH,
    PRESERVATION_REPLAY_POLICY_CAPTURE_REQUIRED,
    PRESERVATION_REPLAY_POLICY_ERMES_AUDIO_UNSUPPORTED
};

int PreservationValidateReplayPolicy(
    int monitor_paging,
    int monitor_acars,
    int monitor_mobitex,
    int monitor_ermes,
    unsigned int configured_sample_rate,
    unsigned int recording_sample_rate,
    const char *capture_path,
    PreservationReplayRoute *route);

const char *PreservationReplayPolicyError(int result);
const char *PreservationReplayRouteName(PreservationReplayRoute route);

#endif
