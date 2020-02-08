#include "sk2.h"

struct Skeleton
{
    uint16_t _pad;
    uint16_t bone_count;
};

FFXI::SK2::SK2(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
{
    Skeleton* skel = (Skeleton*)buffer;
    Bone* bone_buf = (Bone*)(buffer + sizeof(Skeleton));

    for (int i = 0; i < skel->bone_count; ++i)
    {
        bones.push_back(bone_buf[i]);
    }
}
