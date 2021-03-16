#include "sk2.h"
#include <algorithm>
#include <span>

struct Skeleton
{
    uint16_t _pad;
    uint16_t bone_count;
};

FFXI::SK2::SK2(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
{
    Skeleton* skel = (Skeleton*)buffer;

    Bone* bone_buf = (Bone*)(buffer + sizeof(Skeleton));
    bones.resize(skel->bone_count);
    std::ranges::copy(std::span(bone_buf, bone_buf + skel->bone_count), bones.begin());

    GeneratorPoint* points = (GeneratorPoint*)(buffer + sizeof(Skeleton) + (sizeof(Bone) * skel->bone_count) + 4);
    std::ranges::copy(std::span(points, points + GeneratorPointMax), generator_points.begin());
}
