#pragma once

#include <windows.h>

void ConfigurePlainRs232Dcb(DCB *dcb);
BOOL IsPlainRs232Dcb(const DCB *dcb);
