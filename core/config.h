#pragma once
#include <string>
#include <unordered_map>

namespace lotus
{
    class Config
    {
    public:
        const std::string& getConfigOption(const std::string&);
    private:
        std::unordered_map<std::string, std::string> options;
        std::unordered_map<std::string, std::string> default_options;
    };
}
