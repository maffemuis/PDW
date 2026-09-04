#include "synthetic_injection_policy.h"

#include <iostream>

int main()
{
    if (!pdw::CanInjectSyntheticMessage(false))
    {
        std::cerr << "idle synthetic injection must be allowed" << std::endl;
        return 1;
    }

    if (pdw::CanInjectSyntheticMessage(true))
    {
        std::cerr << "groupcall conversion must reject synthetic injection" << std::endl;
        return 1;
    }

    return 0;
}
