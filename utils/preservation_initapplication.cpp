#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "..\Headers\pdw.h"
#include "..\Headers\initapp.h"
#include "..\Headers\sound_in.h"
#include "..\Headers\ermes.h"

BOOL NEAR LegacyInitApplication(HINSTANCE hInstance);

namespace
{
const char *PreservationWavPath()
{
    return getenv("PDW_PRESERVATION_REPLAY_WAV");
}

const char *PreservationErmesSymbolsPath()
{
    return getenv("PDW_PRESERVATION_ERMES_SYMBOLS");
}

bool HasValue(const char *value)
{
    return value && value[0];
}

bool PreservationOneShotReplayRequested()
{
    const char *one_shot = getenv("PDW_PRESERVATION_REPLAY_EXIT");

    return (HasValue(PreservationWavPath()) || HasValue(PreservationErmesSymbolsPath()))
        && one_shot && strcmp(one_shot, "1") == 0;
}

void RefusePreservationReplay(const char *message)
{
    OutputDebugStringA(message ? message : "Preservation replay refused.");
    ExitProcess(2);
}

bool SelectPreservationProtocol()
{
    const char *protocol = getenv("PDW_PRESERVATION_PROTOCOL");

    if (!protocol || !protocol[0] || strcmp(protocol, "paging") == 0)
    {
        Profile.monitor_paging = true;
        Profile.monitor_acars = false;
        Profile.monitor_mobitex = false;
        Profile.monitor_ermes = false;
        return true;
    }

    if (strcmp(protocol, "acars") == 0)
    {
        Profile.monitor_paging = false;
        Profile.monitor_acars = true;
        Profile.monitor_mobitex = false;
        Profile.monitor_ermes = false;
        return true;
    }

    if (strcmp(protocol, "mobitex") == 0)
    {
        Profile.monitor_paging = false;
        Profile.monitor_acars = false;
        Profile.monitor_mobitex = true;
        Profile.monitor_ermes = false;
        return true;
    }

    if (strcmp(protocol, "ermes") == 0)
    {
        Profile.monitor_paging = false;
        Profile.monitor_acars = false;
        Profile.monitor_mobitex = false;
        Profile.monitor_ermes = true;
        return true;
    }

    return false;
}

bool ReplayErmesSymbolsIfRequested()
{
    const char *symbols_path = PreservationErmesSymbolsPath();
    if (!HasValue(symbols_path))
    {
        return false;
    }

    if (!Profile.monitor_ermes)
    {
        RefusePreservationReplay(
            "PDW_PRESERVATION_ERMES_SYMBOLS requires PDW_PRESERVATION_PROTOCOL=ermes.");
        return false;
    }

    if (HasValue(PreservationWavPath()))
    {
        RefusePreservationReplay(
            "ERMES preservation refuses ambiguous WAV + serial-symbol replay sources.");
        return false;
    }

    const char *capture_path = getenv("PDW_PRESERVATION_CAPTURE");
    if (!HasValue(capture_path))
    {
        RefusePreservationReplay(
            "ERMES preservation symbol replay requires PDW_PRESERVATION_CAPTURE.");
        return false;
    }

    FILE *file = fopen(symbols_path, "rb");
    if (!file)
    {
        RefusePreservationReplay("Unable to open ERMES preservation symbol fixture.");
        return false;
    }

    // Match the real serial decoder boundary: pdw_decode_ermes() resolves the
    // modem line state to a four-level symbol (0..3) and calls em.frame().
    // Do not route ERMES through WAV/sound_in.cpp: current PDW has no live
    // ERMES sound-card path and the WAV replay policy intentionally rejects it.
    em.frame(-1);

    size_t symbol_count = 0;
    int symbol = 0;
    while ((symbol = fgetc(file)) != EOF)
    {
        if (symbol < 0 || symbol > 3)
        {
            fclose(file);
            RefusePreservationReplay("ERMES preservation fixture contains a symbol outside 0..3.");
            return false;
        }

        em.frame(symbol);
        ++symbol_count;
    }

    if (ferror(file))
    {
        fclose(file);
        RefusePreservationReplay("Unable to read the complete ERMES preservation symbol fixture.");
        return false;
    }

    fclose(file);

    if (symbol_count == 0)
    {
        RefusePreservationReplay("ERMES preservation symbol fixture is empty.");
        return false;
    }

    return true;
}
}

BOOL NEAR InitApplication(HINSTANCE hInstance)
{
    if (!PreservationOneShotReplayRequested())
    {
        return LegacyInitApplication(hInstance);
    }

    // WinMain has already initialized PDW's deterministic built-in Profile
    // defaults before calling InitApplication(). In one-shot preservation mode
    // replay immediately, before INI loading, window-class registration or any
    // other GUI startup work can block a headless CI runner.
    //
    // The optional preservation protocol selector only changes these headless
    // replay monitor flags. Normal PDW startup never reads or applies it.
    if (!SelectPreservationProtocol())
    {
        RefusePreservationReplay("Unknown PDW_PRESERVATION_PROTOCOL; replay refused.");
        return FALSE;
    }

    // Legacy GUI startup normally initializes the POCSAG/FLEX BCH/ECC lookup
    // tables later during window creation. Headless one-shot replay bypasses
    // that path, so initialize the same decoder state explicitly here.
    setupecc();

    if (ReplayErmesSymbolsIfRequested())
    {
        // Symbol replay is complete and all preservation captures are closed
        // synchronously by PreservationShowMessage(). Never enter GUI startup.
        ExitProcess(0);
        return FALSE;
    }

    // Start_Capturing() terminates with exit 0/2 in one-shot WAV replay mode.
    // If ERMES was selected with a WAV, the existing replay policy rejects it
    // because current live sound-card processing does not feed ERMES_To_Bits.
    Start_Capturing();

    // A one-shot preservation run must never fall through into GUI startup.
    ExitProcess(3);
    return FALSE;
}
