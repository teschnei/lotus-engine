#pragma once
#include <string>
#include <memory>
#include "engine/types.h"
#include "dat_chunk.h"

namespace FFXI
{
    struct WeatherData
    {
        float unk[3]; //always 0?
        //entity light colors
        uint32_t sunlight_diffuse1;
        uint32_t moonlight_diffuse1;
        uint32_t ambient1;
        uint32_t fog1;
        float max_fog_dist1;
        float min_fog_dist1;
        float brightness1;
        float unk2;
        //landscape light colors
        uint32_t sunlight_diffuse2;
        uint32_t moonlight_diffuse2;
        uint32_t ambient2;
        uint32_t fog2;
        float max_fog_dist2;
        float min_fog_dist2;
        float brightness2;
        float unk3;
        uint32_t fog_color;
        float fog_offset;
        float unk4; //always 0?
        float max_far_clip;
        uint32_t unk5; //some type of flags
        float unk6[3];
        uint32_t skybox_colors[8];
        float skybox_values[8];
        float unk7; //always 0?
    };

    class Weather : public DatChunk
    {
    public:
        Weather(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
        {
            data = reinterpret_cast<WeatherData*>(buffer);
        }
        WeatherData* data;
    };


    class DatParser
    {
    public:
        DatParser(const std::string& filepath, bool rtx);

        std::unique_ptr<DatChunk> root;

    private:
        bool rtx;
        std::vector<uint8_t> buffer;
    };
}
