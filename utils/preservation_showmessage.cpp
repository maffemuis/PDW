#ifndef STRICT
#define STRICT 1
#endif

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#define PDW_SHOWMESSAGE_IMPLEMENTATION 1
#include "../Headers/pdw.h"
#include "../Headers/misc.h"

#include "legacy_decoded_message.h"
#include "preservation_capture.h"

namespace
{
bool PreservationOneShotReplayRequested()
{
    const char *recording = getenv("PDW_PRESERVATION_REPLAY_WAV");
    const char *one_shot = getenv("PDW_PRESERVATION_REPLAY_EXIT");

    return recording && recording[0]
        && one_shot && strcmp(one_shot, "1") == 0;
}
}

void PreservationShowMessage(void)
{
    const pdw::DecodedMessage decoded = pdw::SnapshotLegacyDecodedMessage();

    PreservationCaptureMessage(
        decoded.address.c_str(),
        decoded.received_time.c_str(),
        decoded.received_date.c_str(),
        decoded.mode.c_str(),
        decoded.type.c_str(),
        decoded.bitrate.c_str(),
        decoded.text.c_str(),
        decoded.auxiliary.c_str(),
        MAX_STR_LEN);

    if (PreservationOneShotReplayRequested())
    {
        // Golden preservation deliberately stops at the decoder-to-ShowMessage
        // boundary. Do not enter legacy GUI/filter/logging side effects on a
        // headless runner; just prepare the message accumulator for another
        // decoded message in the same recording.
        iMessageIndex = 0;
        message_buffer[0] = '\0';
        mobitex_buffer[0] = '\0';
        return;
    }

    ShowMessage();
}
