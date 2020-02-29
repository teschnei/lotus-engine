#pragma once

#include <engine/config.h>
#include <string>

class FFXIConfig : public lotus::Config
{
public:
    struct FFXIInfo
    {
        std::string ffxi_install_path;
    } ffxi {};

    FFXIConfig();
};
