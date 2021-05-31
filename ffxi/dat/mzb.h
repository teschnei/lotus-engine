#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <optional>
#include "dat_chunk.h"
#include "engine/entity/camera.h"
#include "engine/renderer/model.h"

namespace FFXI
{
    struct SMZBBlock100 {
        char id[16];
        float fTransX,fTransY,fTransZ;
        float fRotX,fRotY,fRotZ;
        float fScaleX,fScaleY,fScaleZ;
        float fa,fb,fc,fd;				//0, 10, 100, 1000
        uint32_t  fe,ff,fg,fh,fi,fj,fk,fl;
    };
    static_assert(sizeof(SMZBBlock100) == 0x64);

    struct CollisionMeshData
    {
        std::vector<uint8_t> vertices;
        std::vector<uint8_t> normals;
        std::vector<uint16_t> indices;
        uint16_t flags{};
        uint32_t max_index;
        glm::vec3 bound_min{};
        glm::vec3 bound_max{};
    };

    struct CollisionEntry
    {
        glm::mat4 transform;
        uint32_t mesh_entry;
    };

    class QuadTree
    {
    public:
        QuadTree(glm::vec3 pos1, glm::vec3 pos2) : pos1(pos1), pos2(pos2) {}

        std::vector<uint32_t> find(lotus::Camera::Frustum&) const;

        glm::vec3 pos1;
        glm::vec3 pos2;
        std::vector<uint32_t> nodes;
        std::vector<QuadTree> children;
    private:
        void find_internal(lotus::Camera::Frustum&, std::vector<uint32_t>&) const;
        void get_nodes(std::vector<uint32_t>&) const;

    };

    class MZB : public DatChunk
    {
    public:
        MZB(char* _name, uint8_t* _buffer, size_t _len);

        static bool DecodeMZB(uint8_t* buffer, size_t max_len);
        std::vector<SMZBBlock100> vecMZB;
        std::optional<QuadTree> quadtree;
        std::vector<CollisionMeshData> meshes;
        std::vector<CollisionEntry> mesh_entries;
        std::unordered_map<float, std::vector<CollisionEntry>> water_entries;

        static lotus::Task<> LoadWaterModel(std::shared_ptr<lotus::Model>, lotus::Engine* engine, std::pair<glm::vec3, glm::vec3> bb);
        static lotus::Task<> LoadWaterTexture(std::shared_ptr<lotus::Texture>& texture, lotus::Engine* engine);

    private:
        QuadTree parseQuadTree(uint8_t* buffer, uint32_t offset);
        uint32_t parseMesh(uint8_t* buffer, uint32_t offset);
        void parseGridEntry(uint8_t* buffer, uint32_t offset, int x, int y);
        void parseGridMesh(uint8_t* buffer, int x, int y, uint32_t vis_entry_offset, uint32_t geo_entry_offset);
        std::vector<uint32_t> vismap;
        std::unordered_map<uint32_t, uint32_t> mesh_map;
    };

    class CollisionLoader
    {
    public:
        static lotus::Task<> LoadModel(std::shared_ptr<lotus::Model>, lotus::Engine* engine, std::vector<CollisionMeshData>& meshes, std::vector<CollisionEntry>& entries);
    };
}
