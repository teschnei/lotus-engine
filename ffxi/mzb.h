#pragma once
#include <cstdint>
#include <vector>

namespace FFXI
{
    struct SMZBBlock100 {
        char id[16];
        float fTransX,fTransY,fTransZ;
        float fRotX,fRotY,fRotZ;
        float fScaleX,fScaleY,fScaleZ;
        float fa,fb,fc,fd;				//0, 10, 100, 1000
        long  fe,ff,fg,fh,fi,fj,fk,fl;
    };

    class MZB
    {
    public:
        MZB(uint8_t* buffer, size_t max_len);

        static bool DecodeMZB(uint8_t* buffer, size_t max_len);
        std::vector<SMZBBlock100> vecMZB;

    private:
        
    };
}
