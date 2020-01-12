#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <optional>
#include "engine/entity/camera.h"

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
    static_assert(sizeof(SMZBBlock100) == 0x64);

    class QuadTree
    {
    public:
        QuadTree(glm::vec3 pos1, glm::vec3 pos2) : pos1(pos1), pos2(pos2) {}

        std::vector<uint32_t> find(lotus::Camera::Frustum&) const;
        std::vector<uint32_t> find(glm::vec3 pos) const;

        std::vector<uint32_t> get_nodes() const;

        glm::vec3 pos1;
        glm::vec3 pos2;
        std::vector<uint32_t> nodes;
        std::vector<QuadTree> children;
    };

    class MZB
    {
    public:
        MZB(uint8_t* buffer, size_t max_len);

        static bool DecodeMZB(uint8_t* buffer, size_t max_len);
        std::vector<SMZBBlock100> vecMZB;
        std::optional<QuadTree> quadtree;

    private:
        QuadTree parseQuadTree(uint8_t* buffer, uint32_t offset);
        std::vector<uint32_t> vismap;
        
    };
}
