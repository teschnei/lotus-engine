#pragma once

#include <map>
#include <filesystem>
#include "dat.h"

namespace FFXI
{
    class DatLoader
    {
    public:
        const Dat& GetDat(std::filesystem::path);
    private:
        std::map<std::filesystem::path, Dat> dat_map;
    };
}