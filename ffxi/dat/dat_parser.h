#pragma once
#include <string>
#include <memory>
#include "mzb.h"
#include "mmb.h"
#include "dxt3.h"
#include "os2.h"
#include "sk2.h"
#include "mo2.h"

class DatParser
{
public:
    DatParser(const std::string& filepath);

    const std::vector<std::unique_ptr<FFXI::MZB>>& getMZBs() const { return mzbs; }
    const std::vector<std::unique_ptr<FFXI::MMB>>& getMMBs() const { return mmbs; }
    const std::vector<std::unique_ptr<FFXI::DXT3>>& getDXT3s() const { return dxt3s; };
    const std::vector<std::unique_ptr<FFXI::MO2>>& getMO2s() const { return mo2s; };
    const std::vector<std::unique_ptr<FFXI::SK2>>& getSK2s() const { return sk2s; };
    const std::vector<std::unique_ptr<FFXI::OS2>>& getOS2s() const { return os2s; };

private:
    std::vector<std::unique_ptr<FFXI::MZB>> mzbs;
    std::vector<std::unique_ptr<FFXI::MMB>> mmbs;
    std::vector<std::unique_ptr<FFXI::DXT3>> dxt3s;
    std::vector<std::unique_ptr<FFXI::MO2>> mo2s;
    std::vector<std::unique_ptr<FFXI::SK2>> sk2s;
    std::vector<std::unique_ptr<FFXI::OS2>> os2s;
};
