#include "rs232_config.h"

void ConfigurePlainRs232Dcb(DCB *dcb)
{
    if(!dcb) return;

    dcb->BaudRate = CBR_19200;
    dcb->ByteSize = 8;
    dcb->Parity = NOPARITY;
    dcb->StopBits = ONESTOPBIT;
    dcb->fBinary = TRUE;
    dcb->fParity = FALSE;

    // Plain USB/virtual-COM discriminator input must never inherit flow control
    // from a previous application or driver configuration.
    dcb->fOutxCtsFlow = FALSE;
    dcb->fOutxDsrFlow = FALSE;
    dcb->fDsrSensitivity = FALSE;
    dcb->fTXContinueOnXoff = TRUE;
    dcb->fOutX = FALSE;
    dcb->fInX = FALSE;
    dcb->fErrorChar = FALSE;
    dcb->fNull = FALSE;
    dcb->fAbortOnError = FALSE;
    dcb->fDtrControl = DTR_CONTROL_DISABLE;
    dcb->fRtsControl = RTS_CONTROL_DISABLE;
}

BOOL IsPlainRs232Dcb(const DCB *dcb)
{
    if(!dcb) return FALSE;

    return dcb->BaudRate == CBR_19200 &&
           dcb->ByteSize == 8 &&
           dcb->Parity == NOPARITY &&
           dcb->StopBits == ONESTOPBIT &&
           dcb->fBinary &&
           !dcb->fParity &&
           !dcb->fOutxCtsFlow &&
           !dcb->fOutxDsrFlow &&
           !dcb->fDsrSensitivity &&
           !dcb->fOutX &&
           !dcb->fInX &&
           !dcb->fErrorChar &&
           !dcb->fNull &&
           !dcb->fAbortOnError &&
           dcb->fDtrControl == DTR_CONTROL_DISABLE &&
           dcb->fRtsControl == RTS_CONTROL_DISABLE;
}
