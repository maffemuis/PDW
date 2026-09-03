#include "preservation_decoder_replay_policy.h"

int PreservationValidateReplayPolicy(
    int monitor_paging,
    int monitor_acars,
    int monitor_mobitex,
    int monitor_ermes,
    unsigned int configured_sample_rate,
    unsigned int recording_sample_rate,
    const char *capture_path,
    PreservationReplayRoute *route)
{
    if (route)
    {
        *route = PRESERVATION_REPLAY_ROUTE_NONE;
    }

    const int active_modes =
        (monitor_paging ? 1 : 0)
        + (monitor_acars ? 1 : 0)
        + (monitor_mobitex ? 1 : 0)
        + (monitor_ermes ? 1 : 0);

    if (active_modes == 0)
    {
        return PRESERVATION_REPLAY_POLICY_NO_MODE;
    }

    if (active_modes != 1)
    {
        return PRESERVATION_REPLAY_POLICY_MULTIPLE_MODES;
    }

    if (configured_sample_rate == 0
        || recording_sample_rate == 0
        || configured_sample_rate != recording_sample_rate)
    {
        return PRESERVATION_REPLAY_POLICY_SAMPLE_RATE_MISMATCH;
    }

    if (!capture_path || !capture_path[0])
    {
        return PRESERVATION_REPLAY_POLICY_CAPTURE_REQUIRED;
    }

    if (route)
    {
        if (monitor_paging)
        {
            *route = PRESERVATION_REPLAY_ROUTE_PAGING;
        }
        else if (monitor_acars)
        {
            *route = PRESERVATION_REPLAY_ROUTE_ACARS;
        }
        else if (monitor_mobitex)
        {
            *route = PRESERVATION_REPLAY_ROUTE_MOBITEX;
        }
        else
        {
            *route = PRESERVATION_REPLAY_ROUTE_ERMES;
        }
    }

    return PRESERVATION_REPLAY_POLICY_OK;
}

const char *PreservationReplayPolicyError(int result)
{
    switch (result)
    {
        case PRESERVATION_REPLAY_POLICY_OK:
            return "";
        case PRESERVATION_REPLAY_POLICY_NO_MODE:
            return "preservation replay requires one active decoder mode";
        case PRESERVATION_REPLAY_POLICY_MULTIPLE_MODES:
            return "preservation replay refuses multiple active decoder modes";
        case PRESERVATION_REPLAY_POLICY_SAMPLE_RATE_MISMATCH:
            return "recording sample rate does not match PDW audio sample rate";
        case PRESERVATION_REPLAY_POLICY_CAPTURE_REQUIRED:
            return "PDW_PRESERVATION_CAPTURE must be set for decoder replay";
        default:
            return "unknown preservation replay policy error";
    }
}

const char *PreservationReplayRouteName(PreservationReplayRoute route)
{
    switch (route)
    {
        case PRESERVATION_REPLAY_ROUTE_PAGING:
            return "paging";
        case PRESERVATION_REPLAY_ROUTE_ACARS:
            return "acars";
        case PRESERVATION_REPLAY_ROUTE_MOBITEX:
            return "mobitex";
        case PRESERVATION_REPLAY_ROUTE_ERMES:
            return "ermes";
        default:
            return "none";
    }
}
