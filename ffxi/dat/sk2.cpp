#include "sk2.h"

struct Skeleton
{
    uint16_t _pad;
    uint16_t bone_count;
};

FFXI::SK2::SK2(uint8_t* buffer, size_t max_len)
{
    Skeleton* skel = (Skeleton*)buffer;
    Bone* bone_buf = (Bone*)(buffer + sizeof(Skeleton));

    for (int i = 0; i < skel->bone_count; ++i)
    {
        bones.push_back(bone_buf[i]);
    }
}
