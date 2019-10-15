#pragma once

#include "engine/types.h"
#include <string>

namespace FFXI
{
    class MO2
    {
    public:
        MO2(uint8_t* buffer, size_t max_len, char name[4]);

        std::string name;
    };
}
