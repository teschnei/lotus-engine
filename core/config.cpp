#include "config.h"

const std::string& lotus::Config::getConfigOption(const std::string& option)
{
    if (auto opt_iter = options.find(option); opt_iter != options.end())
    {
        return opt_iter->second;
    }
    return default_options[option];
}
