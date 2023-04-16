#include "dat_loader.h"
#include <fstream>
#include <fmt/core.h>
#include <algorithm>

namespace FFXI
{
    DatLoader::DatLoader(std::filesystem::path install_path) : install_path(install_path)
    {
        vtables.push_back(read_file(install_path / "VTABLE.DAT"));
        ftables.push_back(read_file(install_path / "FTABLE.DAT"));

        std::vector<std::filesystem::path> v_paths;
        std::vector<std::filesystem::path> f_paths;

        for (const auto& dir : std::filesystem::directory_iterator(install_path))
        {
            //a terrible hack that should probably get fixed in the standard
            if (auto dir_ext = (dir.path() / "").parent_path().filename(); dir.is_directory() && dir_ext.string().starts_with("ROM"))
            {
                for (const auto& rom_file : std::filesystem::directory_iterator(dir))
                {
                    if (rom_file.is_regular_file())
                    {
                        if (auto p = rom_file.path().stem().string(); p.starts_with("VTABLE"))
                        {
                            //make sure that the VTABLE is from the same ROM - an old update can have VTABLE3 in ROM2
                            if (dir_ext.string().back() == p.back())
                                v_paths.push_back(rom_file);
                        }
                        else if (p.starts_with("FTABLE"))
                        {
                            if (dir_ext.string().back() == p.back())
                                f_paths.push_back(rom_file);
                        }
                    }
                }
            }
        }
        std::ranges::sort(v_paths);
        std::ranges::sort(f_paths);
        for (const auto& dir : v_paths)
        {
            vtables.push_back(read_file(dir));
        }
        for (const auto& dir : f_paths)
        {
            ftables.push_back(read_file(dir));
        }
    }

    const Dat& DatLoader::GetDat(size_t index)
    {
        auto path = install_path / get_dat_path(index);
        if (auto found = dat_map.find(path); found != dat_map.end())
        {
            return found->second;
        }
        else
        {
            return dat_map.insert({ path, Dat(path) }).first->second;
        }
    }

    const Dat& DatLoader::GetDat(const std::filesystem::path& dat_path)
    {
        auto path = install_path / dat_path;
        if (auto found = dat_map.find(path); found != dat_map.end())
        {
            return found->second;
        }
        else
        {
            return dat_map.insert({ path, Dat(path) }).first->second;
        }
    }

    std::vector<std::byte> DatLoader::read_file(std::filesystem::path path)
    {
        std::vector<std::byte> buffer;
        std::ifstream file{ path, std::ios::ate | std::ios::binary };

        if (!file.good())
            throw std::runtime_error("file not found: " + path.string());

        size_t file_size = (size_t)file.tellg();
        buffer.resize(file_size);

        file.seekg(0);
        file.read((char*)buffer.data(), file_size);
        file.close();
        return buffer;
    }

    std::filesystem::path DatLoader::get_dat_path(size_t index)
    {
        size_t ftable_index = 0;
        for (const auto& vtable : vtables)
        {
            if (auto ft = std::to_integer<int>(vtable[index]); ft != 0)
            {
                ftable_index = ft;
                break;
            }
        }
        if (ftable_index < ftables.size() && ftable_index != 0)
        {
            auto ft_val = *reinterpret_cast<uint16_t*>(ftables[ftable_index - 1].data() + (index * 2));
            if (ftable_index == 1)
            {
                return fmt::format("ROM/{}/{}.DAT", ft_val >> 7, ft_val & 0x7F);
            }
            else
            {
                return fmt::format("ROM{}/{}/{}.DAT", ftable_index, ft_val >> 7, ft_val & 0x7F);
            }
        }
        return {};
    }
}
