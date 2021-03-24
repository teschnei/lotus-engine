#include "dat_loader.h"

namespace FFXI
{
    const Dat& DatLoader::GetDat(std::filesystem::path path)
    {
        if (auto found = dat_map.find(path); found != dat_map.end())
        {
            return found->second;
        }
        else
        {
            return dat_map.insert({ path, Dat(path) }).first->second;
        }
    }
}