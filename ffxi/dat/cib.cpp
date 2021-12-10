#include "cib.h"

namespace FFXI
{
    Cib::Cib(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
    {
        unknown1 = buffer[0];
        footstep1 = buffer[1];
        footstep2 = buffer[2];
        motion_index = buffer[3];
        motion_option = buffer[4];
        weapon_unknown = buffer[5];
        weapon_unknown2 = buffer[6];
        unknown2 = buffer[7];
        weapon_unknown3 = buffer[8];
        body_armour_unknown = buffer[9];
        scale0 = buffer[10];
        scale1 = buffer[11];
        scale2 = buffer[12];
        scale3 = buffer[13];
        motion_range_index = buffer[14];
    }
}
