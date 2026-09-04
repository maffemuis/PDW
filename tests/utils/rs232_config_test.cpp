#include <windows.h>
#include <cassert>
#include <cstring>

#include "rs232_config.h"

int main()
{
    DCB dcb;
    std::memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    // Simulate a virtual COM port inheriting every flow-control mechanism.
    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 7;
    dcb.Parity = EVENPARITY;
    dcb.StopBits = TWOSTOPBITS;
    dcb.fOutxCtsFlow = TRUE;
    dcb.fOutxDsrFlow = TRUE;
    dcb.fDsrSensitivity = TRUE;
    dcb.fOutX = TRUE;
    dcb.fInX = TRUE;
    dcb.fErrorChar = TRUE;
    dcb.fNull = TRUE;
    dcb.fAbortOnError = TRUE;
    dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
    dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;

    assert(!IsPlainRs232Dcb(&dcb));
    ConfigurePlainRs232Dcb(&dcb);
    assert(IsPlainRs232Dcb(&dcb));
    assert(dcb.BaudRate == CBR_19200);
    assert(dcb.ByteSize == 8);
    assert(dcb.Parity == NOPARITY);
    assert(dcb.StopBits == ONESTOPBIT);
    assert(!dcb.fOutxCtsFlow);
    assert(!dcb.fOutxDsrFlow);
    assert(!dcb.fDsrSensitivity);
    assert(!dcb.fOutX);
    assert(!dcb.fInX);
    assert(dcb.fDtrControl == DTR_CONTROL_DISABLE);
    assert(dcb.fRtsControl == RTS_CONTROL_DISABLE);

    // Verification must fail if a driver silently restores any blocked flow flag.
    dcb.fOutxCtsFlow = TRUE;
    assert(!IsPlainRs232Dcb(&dcb));
    dcb.fOutxCtsFlow = FALSE;
    dcb.fInX = TRUE;
    assert(!IsPlainRs232Dcb(&dcb));
    dcb.fInX = FALSE;
    dcb.BaudRate = CBR_9600;
    assert(!IsPlainRs232Dcb(&dcb));

    return 0;
}
