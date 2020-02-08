#pragma once
#include <string>
#include <memory>
#include "engine/types.h"
#include "dat_chunk.h"

namespace FFXI
{

    class DatParser
    {
    public:
        DatParser(const std::string& filepath, bool rtx);

        std::unique_ptr<DatChunk> root;

    private:
        bool rtx;
        std::vector<uint8_t> buffer;
    };
}
