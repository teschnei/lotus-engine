#pragma once
#include <string>
#include <memory>
#include "mzb.h"
#include "mmb.h"
#include "dxt3.h"

class DatParser
{
public:
    DatParser(const std::string& filepath);

    const std::vector<std::unique_ptr<FFXI::MZB>>& getMZBs() { return mzbs; }
    const std::vector<std::unique_ptr<FFXI::MMB>>& getMMBs() { return mmbs; }
    const std::vector<std::unique_ptr<FFXI::DXT3>>& getDXT3s() { return dxt3s; };

private:
    std::vector<std::unique_ptr<FFXI::MZB>> mzbs;
    std::vector<std::unique_ptr<FFXI::MMB>> mmbs;
    std::vector<std::unique_ptr<FFXI::DXT3>> dxt3s;
};
