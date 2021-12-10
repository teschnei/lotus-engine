#pragma once

#include "dat_chunk.h"

namespace FFXI
{
    class Cib : public DatChunk
    {
    public:
        Cib(char* name, uint8_t* buffer, size_t len);

        uint8_t unknown1; //from skeleton
        uint8_t footstep1; //FootMat?
        uint8_t footstep2; //FootSize?
        uint8_t motion_index;
        uint8_t motion_option;
        uint8_t weapon_unknown; //Shield?
        uint8_t weapon_unknown2; //Constrain?
        uint8_t unknown2; //probably always empty, maps to XiActorSkeleton's weapon_unknown2 for offhand
        uint8_t weapon_unknown3; //never seen this populated, but decompiling says it's from a weapon
        uint8_t body_armour_unknown; //Waist?
        uint8_t scale0;
        uint8_t scale1;
        uint8_t scale2;
        uint8_t scale3;
        uint8_t motion_range_index;
    };
}
