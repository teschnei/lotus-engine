#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "dat_chunk.h"
#include "d3m.h"

namespace FFXI
{
    class Keyframe : public DatChunk
    {
    public:
        Keyframe(char* name, uint8_t* buffer, size_t len);
        std::vector<std::pair<float, float>> intervals;
    };

    class Generator : public DatChunk
    {
    public:
#pragma pack(push,2)
        struct GeneratorHeader
        {
            uint8_t flags1;
            uint8_t bone_point;
            uint8_t flags2;
            uint8_t flags3;
            uint32_t unknown1[3];
            float unknown2[16];
            uint32_t flags4;
            uint32_t unknown3[4];
            uint16_t unknown4;
            uint16_t interval;
            uint8_t occurences;
            uint8_t flags5;
            uint16_t unknown5;
            uint32_t flags6;
            uint32_t offset1;
            uint32_t offset2;
            uint32_t offset3;
            uint32_t offset4;
        };
#pragma pack(pop)

        Generator(char* name, uint8_t* buffer, size_t len);

        GeneratorHeader* header{ nullptr };
        std::string id;
        uint16_t billboard{ 0 };
        uint16_t pos_flags{ 0 };
        glm::vec3 pos{ 0 };
        uint8_t effect_type{ 0 };
        uint16_t lifetime{ 0 };

        //movement per frame
        glm::vec3 dpos{ 0 };
        glm::vec3 dpos_fluctuation{ 0 };
        glm::vec3 dpos_acceleration{ 0 };
        float dpos_exp{ 1 };

        //movement from/to origin
        float dpos_origin{ 0 };

        //initial rotation
        glm::vec3 rot{ 0 };
        glm::vec3 rot_fluctuation{ 0 };

        //rotation per frame
        glm::vec3 drot{ 0 };
        glm::vec3 drot_fluctuation{ 0 };

        //scale
        glm::vec3 scale{ 0 };
        glm::vec3 scale_fluctuation{ 0 };
        float scale_all_fluctuation{ 0 };

        //scale per frame
        glm::vec3 dscale{ 0 };
        glm::vec3 dscale_fluctuation{ 0 };

        //generation sphere
        float gen_radius_sphere{ 0 };
        float gen_radius_sphere_fluctuation{ 0 };

        uint32_t color{ 0 };

        //generation cylinder
        float gen_radius{ 0 };
        float gen_radius_fluctuation{ 0 };
        glm::vec3 gen_multi{ 0 };
        glm::vec2 gen_axis_rot{ 0 };
        float gen_height{ 0 };
        float gen_height_fluctuation{ 0 };
        uint32_t rotations{ 0 };

        //addition to rot every generation
        glm::vec3 gen_rot_add{ 0 };

        //keyframe animation
        std::string kf_x_pos;
        std::string kf_y_pos;
        std::string kf_z_pos;

        std::string kf_x_rot;
        std::string kf_y_rot;
        std::string kf_z_rot;

        std::string kf_x_scale;
        std::string kf_y_scale;
        std::string kf_z_scale;

        std::string kf_r;
        std::string kf_g;
        std::string kf_b;
        std::string kf_a;

        std::string kf_u;
        std::string kf_v;

        glm::vec2 duv{};

        std::vector<D3M::Vertex> ring_vertices;
        std::vector<uint16_t> ring_indices;
        std::shared_ptr<lotus::Model> ring;
        std::string sub_generator;
        std::string end_generator;
    };
}
