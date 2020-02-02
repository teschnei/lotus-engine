#include "mzb.h"

#include "key_tables.h"
#include <algorithm>
#include "engine/entity/camera.h"

namespace FFXI
{
    struct SMZBHeader {
        char id[4];
        uint32_t totalRecord100 : 24;
        uint32_t R100Flag : 8;
        uint32_t collisionMeshOffset;
        uint8_t gridWidth;
        uint8_t gridHeight;
        uint8_t bucketWidth;
        uint8_t bucketHeight;
        uint32_t quadtreeOffset;
        uint32_t objectOffsetEnd;
        uint32_t shortnamesOffset;
        int32_t unk5;
    };
    static_assert(sizeof(SMZBHeader) == 0x20);

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

    enum class FrustumResult
    {
        Inside,
        Outside,
        Intersect
    };

    FrustumResult isInFrustum(lotus::Camera::Frustum& frustum, glm::vec3 pos_min, glm::vec3 pos_max)
    {
        FrustumResult result = FrustumResult::Inside;
        for (const auto& plane : {frustum.left, frustum.right, frustum.top, frustum.bottom, frustum.near, frustum.far})
        {
            //p: AABB corner furthest in the direction of plane normal, n: AABB corner furthest in the direction opposite of plane normal
            glm::vec3 p = pos_min;
            glm::vec3 n = pos_max;

            if (plane.x >= 0)
            {
                p.x = pos_max.x;
                n.x = pos_min.x;
            }
            if (plane.y >= 0)
            {
                p.y = pos_max.y;
                n.y = pos_min.y;
            }
            if (plane.z >= 0)
            {
                p.z = pos_max.z;
                n.z = pos_min.z;
            }

            if (glm::dot(p, glm::vec3(plane)) + plane.w < 0.f)
            {
                return FrustumResult::Outside;
            }
            if (glm::dot(n, glm::vec3(plane)) + plane.w < 0.f)
            {
                result = FrustumResult::Intersect;
            }
        }
        return result;
    }

    void QuadTree::find_internal(lotus::Camera::Frustum& frustum, std::vector<uint32_t>& results) const
    {
        auto result = isInFrustum(frustum, pos1, pos2);
        if (result == FrustumResult::Outside)
            return;
        else if (result == FrustumResult::Inside)
        {
            get_nodes(results);
        }
        else if (result == FrustumResult::Intersect)
        {
            results.insert(results.end(), nodes.begin(), nodes.end());
            for (const auto& child : children)
            {
                child.find_internal(frustum, results);
            }
        }
    }

    std::vector<uint32_t> QuadTree::find(lotus::Camera::Frustum& frustum) const
    {
        std::vector<uint32_t> ret;
        find_internal(frustum, ret);
        return ret;
    }

    void QuadTree::get_nodes(std::vector<uint32_t>& results) const
    {
        results.insert(results.end(), nodes.begin(), nodes.end());
        for (const auto& child : children)
        {
            child.get_nodes(results);
        }
    }

    MZB::MZB(uint8_t* buffer, size_t max_len)
    {
        SMZBHeader* header = (SMZBHeader*)buffer;

        for (size_t i = 0; i < header->totalRecord100; ++i)
        {
            vecMZB.push_back(((SMZBBlock100*)(buffer + sizeof(SMZBHeader)))[i]);
        }

        uint32_t maplist_offset = *(uint32_t*)(buffer + header->collisionMeshOffset + 0x14);
        uint32_t maplist_count = *(uint32_t*)(buffer + header->collisionMeshOffset + 0x18);

        for (uint32_t i = 0; i < maplist_count; ++i)
        {
            uint8_t* maplist_base = buffer + maplist_offset + 0x0C * i;
            uint32_t mapid_encoded = *(uint32_t*)(maplist_base + 0x29 * sizeof(float));
            uint32_t objvis_offset = *(uint32_t*)(maplist_base + 0x2a * sizeof(float));
            uint32_t objvis_count = *(uint32_t*)(maplist_base + 0x2b * sizeof(float));

            float x = *(float*)(maplist_base + 0x2c * sizeof(float));
            float y = *(float*)(maplist_base + 0x2c * sizeof(float));

            uint32_t mapid = ((mapid_encoded >> 3) & 0x7) | (((mapid_encoded >> 26) & 0x3) << 3);
            vismap.push_back(mapid);
        }

        quadtree = parseQuadTree(buffer, header->quadtreeOffset);
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

    QuadTree MZB::parseQuadTree(uint8_t* buffer, uint32_t offset)
    {
        uint8_t* quad_base = buffer + offset;
        glm::vec3 pos1 = ((glm::vec3*)quad_base)[0];
        glm::vec3 pos2 = ((glm::vec3*)quad_base)[0];

        for (int i = 1; i < 8; ++i)
        {
            glm::vec3 bb = ((glm::vec3*)quad_base)[i];
            pos1 = glm::min(pos1, bb);
            pos2 = glm::max(pos2, bb);
        }

        QuadTree quadtree{ pos1, pos2 };

        uint32_t visibility_list_offset = *(uint32_t*)(quad_base + sizeof(glm::vec3) * 8);
        uint32_t visibility_list_count = *(uint32_t*)(quad_base + sizeof(glm::vec3) * 8 + sizeof(uint32_t));

        for (size_t i = 0; i < visibility_list_count; ++i)
        {
            uint32_t node = *(uint32_t*)(buffer + visibility_list_offset + sizeof(uint32_t) * i);
            quadtree.nodes.push_back(node);
        }

        for (int i = 0; i < 6; ++i)
        {
            uint32_t child_offset = *(uint32_t*)(quad_base + sizeof(glm::vec3) * 8 + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) * i);
            if (child_offset != 0)
            {
                quadtree.children.push_back(parseQuadTree(buffer, child_offset));
            }
        }

        return quadtree;
    }
}
