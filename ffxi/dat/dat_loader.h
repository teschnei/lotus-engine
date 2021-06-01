#pragma once

#include <vector>
#include <map>
#include <filesystem>
#include "dat.h"

namespace FFXI
{
    class DatLoader
    {
    public:
        DatLoader(std::filesystem::path install_path);
        const Dat& GetDat(size_t index);
        //const Dat& GetDat(const std::filesystem::path&);
    private:
        std::vector<std::byte> read_file(std::filesystem::path path);
        std::filesystem::path get_dat_path(size_t index);

        std::filesystem::path install_path;
        std::map<std::filesystem::path, Dat> dat_map;
        std::vector<std::vector<std::byte>> vtables;
        std::vector<std::vector<std::byte>> ftables;
    };
}