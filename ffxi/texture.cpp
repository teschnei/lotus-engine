#include "texture.h"
#include "core/renderer/texture.h"
#include "core/task/texture_init.h"
#include "core/core.h"

namespace FFXI
{
#pragma pack(push,1)
    typedef struct
    {
        uint8_t flg;
        char id[16];
        uint32_t dwnazo1;
        long  imgx, imgy;
        uint32_t dwnazo2[6];
        uint32_t widthbyte;
        char ddsType[4];
        unsigned int size;
        unsigned int noBlock;
    } IMGINFOA1;

    typedef struct
    {
        glm::u8  flg;
        char id[16];
        glm::u32 dwnazo1;			//nazo = unknown
        long  imgx, imgy;
        glm::u32 dwnazo2[6];
        glm::u32 widthbyte;
        glm::u32 unk;				//B1-extra unk, 01-no unk
        glm::u32 palet[0x100];
    } IMGINFOB1;
#pragma pack(pop)

    Texture::Texture(uint8_t* buffer)
    {
        IMGINFOA1* infoa1 = reinterpret_cast<IMGINFOA1*>(buffer);

        switch (infoa1->flg)
        {
            case 0xA1:
            {
                name = std::string(infoa1->id, 16);
                width = infoa1->imgx;
                height = infoa1->imgy;
                pixels.resize(infoa1->size);
                format = vk::Format::eBc2UnormBlock;
                memcpy(pixels.data(), buffer + sizeof(IMGINFOA1), infoa1->size);
                break;
            }
            case 0x01:
                __debugbreak();
                break;
            case 0x81:
                __debugbreak();
                break;
            case 0xB1:
            {
                IMGINFOB1* infob1 = reinterpret_cast<IMGINFOB1*>(buffer);
                name = std::string(infob1->id, 16);
                width = infob1->imgx;
                height = infob1->imgy;
                format = vk::Format::eR8G8B8A8Unorm;
                uint32_t size = width * height;
                pixels.resize(size * 4);
                uint8_t* buf_p = buffer + sizeof(IMGINFOB1);
                for (int i = height-1; i>=0; --i)
                {
                    for (int j = 0; j < width; ++j)
                    {
                        memcpy(pixels.data() + ((i * width + j) * 4), &infob1->palet[(size_t)(*buf_p)], 4);
                        ++buf_p;
                    }
                }
                    
                break;
            }
            case 0x05:
                __debugbreak();
                break;
            case 0x91:
                __debugbreak();
                break;
        }
    }

}
