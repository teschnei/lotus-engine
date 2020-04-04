#pragma once

#include "dat_chunk.h"

namespace FFXI
{
    class Scheduler : public DatChunk
    {
    public:
#pragma pack(push,2)
        struct SchedulerHeader
        {
            uint32_t unk0[4];
            uint32_t unk1;
            uint32_t unk2;
            uint32_t unk3;
            uint32_t unk4;
            uint32_t unk5;
            uint32_t unk6;
            uint32_t unk7;
            uint32_t unk8;
            uint32_t unk9;
            uint32_t unk10;
            uint32_t unk11;
            uint32_t unk12;
            uint32_t unk13;
            uint32_t unk14;
        };
#pragma pack(pop)
        Scheduler(char* name, uint8_t* buffer, size_t len);
        SchedulerHeader* header{ nullptr };
    };
}
