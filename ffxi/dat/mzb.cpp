#include "mzb.h"

#include "key_tables.h"

namespace FFXI
{
    struct SMZBHeader {
        char id[4];
        unsigned int totalRecord100 : 24;
        unsigned int R100Flag : 8;
        unsigned int offsetHeader2;
        unsigned int d1 : 8;
        unsigned int d2 : 8;
        unsigned int d3 : 8;
        unsigned int d4 : 8;
        int offsetlooseTree;
        unsigned int offsetEndRecord100;
        unsigned int offsetEndlooseTree;
        int unk5;
    };

    struct SMZBBlock84 {
        char id[16];
        float fTransX,fTransY,fTransZ;
        float fRotX,fRotY,fRotZ;
        float fScaleX,fScaleY,fScaleZ;
        float fa,fb,fc,fd;				//0, 10, 100, 1000
        unsigned int i1, i2, i3, i4;
    };

    //observed in dat 116
    struct SMZBBlock92b {
        char id[16];
        float fTransX,fTransY,fTransZ;
        float fRotX,fRotY,fRotZ;
        float fScaleX,fScaleY,fScaleZ;
        float fa,fb,fc,fd;				//0, 10, 100, 1000
        unsigned int i1, i2, i3, i4, i5, i6;
    };

    MZB::MZB(uint8_t* buffer, size_t max_len)
    {
        SMZBHeader* header = (SMZBHeader*)buffer;

        for (size_t i = 0; i < header->totalRecord100; ++i)
        {
            vecMZB.push_back(((SMZBBlock100*)(buffer + 32))[i]);
        }

        //bool haveLooseTree = header->offsetlooseTree > header->offsetEndRecord100 && header->offsetEndlooseTree > header->offsetlooseTree;
        //bool havePVS = false;
        //int sizePVS = 0;

        //if (haveLooseTree)
        //{
        //    if (header->offsetlooseTree - header->offsetEndRecord100 > 16)
        //    {
        //        havePVS = true;
        //    }
        //}
        //else
        //{
        //    if (header->offsetlooseTree > 0)
        //    {
        //        sizePVS = header->offsetlooseTree;
        //        havePVS = true;
        //    }
        //}

        //if (havePVS)
        //{
        //    
        //}

    }

    bool MZB::DecodeMZB(uint8_t* buffer, size_t max_len)
    {
        if (buffer[3] >= 0x1B)
        {
            uint32_t len = *(uint32_t*)buffer & 0x00FFFFFF;
            if (len > max_len) return false;

            uint32_t key = key_table[buffer[7] ^ 0xFF];
            int key_count = 0;
            uint32_t pos = 8;
            while (pos < len)
            {
                uint32_t xor_length = ((key >> 4) & 7) + 16;

                if ((key & 1) && (pos + xor_length < len))
                {
                    for (uint32_t i = 0; i < xor_length; ++i)
                    {
                        buffer[pos + i] ^= 0xFF;
                    }
                }
                key += ++key_count;
                pos += xor_length;
            }

            uint32_t node_count = *(uint32_t*)(buffer + 4) & 0x00FFFFFF;

            SMZBBlock100* node = (SMZBBlock100*)(buffer + 32);

            for (uint32_t i = 0; i < node_count; ++i)
            {
                for (size_t j = 0; j < 16; ++j)
                {
                    node->id[j] ^= 0x55;
                }
                ++node;
            }
        }
        return true;
    }
    
}
