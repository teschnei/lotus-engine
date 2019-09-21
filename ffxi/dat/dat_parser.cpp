#include "dat_parser.h"
#include <fstream>

#pragma pack(push,1)
typedef struct 
{
  unsigned int  id;
  long   type:7;
  long   next:19;
  long   is_shadow:1;
  long   is_extracted:1;
  long   ver_num:3;
  long   is_virtual:1;
  unsigned int 	parent;
  unsigned int  child;
} DATHEAD;

#pragma pack(pop)
DatParser::DatParser(const std::string& filepath)
{
    std::ifstream dat{filepath, std::ios::ate | std::ios::binary };

    size_t fileSize = (size_t) dat.tellg();
    std::vector<uint8_t> buffer(fileSize);

    dat.seekg(0);
    dat.read((char*)buffer.data(), fileSize);
    dat.close();

    int MZB = 0;
    int MMB = 0;
    int IMG = 0;
    int BONE = 0;
    int ANIM = 0;
    int VERT = 0;

    int offset = 0;
    while(offset < buffer.size())
    {
        DATHEAD* dathead = (DATHEAD*)&buffer[offset];
        int len = (dathead->next & 0x7ffff) * 16;

        switch (dathead->type)
        {
        case 0x1C:
            MZB++;
            if (FFXI::MZB::DecodeMZB(&buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD)))
            {
                mzbs.push_back(std::make_unique<FFXI::MZB>(&buffer[offset + sizeof(DATHEAD)], len - (sizeof(DATHEAD))));
            }
            break;
        case 0x2E:
            MMB++;
            if (FFXI::MMB::DecodeMMB(&buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD)))
            {
                mmbs.push_back(std::make_unique<FFXI::MMB>(&buffer[offset + sizeof(DATHEAD)], len - (sizeof(DATHEAD))));
            }
            break;
        case 0x20:
            IMG++;
            dxt3s.push_back(std::make_unique<FFXI::DXT3>(&buffer[offset + sizeof(DATHEAD)]));
            break;
        case 0x29:
            BONE++;
            break;
        case 0x2A:
            VERT++;
            break;
        case 0x2B:
            ANIM++;
            break;
        default:
            break;
        }
        offset += len;
    }
}
