#include "synthetic_injection_policy.h"

namespace pdw
{
bool CanInjectSyntheticMessage(bool converting_groupcall)
{
    return !converting_groupcall;
}
}
