#include "mzb.h"

#include "key_tables.h"
#include <algorithm>
#include "engine/entity/camera.h"
#include "engine/core.h"
#include "entity/landscape_entity.h"
#include "engine/renderer/vulkan/renderer_raytrace_base.h"

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
        uint32_t i1, i2, i3, i4;
    };

    //observed in dat 116
    struct SMZBBlock92b {
        char id[16];
        float fTransX,fTransY,fTransZ;
        float fRotX,fRotY,fRotZ;
        float fScaleX,fScaleY,fScaleZ;
        float fa,fb,fc,fd;				//0, 10, 100, 1000
        uint32_t i1, i2, i3, i4, i5, i6;
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

    MZB::MZB(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
    {
        SMZBHeader* header = (SMZBHeader*)buffer;

        for (size_t i = 0; i < header->totalRecord100; ++i)
        {
            vecMZB.push_back(((SMZBBlock100*)(buffer + sizeof(SMZBHeader)))[i]);
        }

        uint32_t mesh_count = *(uint32_t*)(buffer + header->collisionMeshOffset);
        uint32_t mesh_data = *(uint32_t*)(buffer + header->collisionMeshOffset + 0x04);

        uint32_t offset = mesh_data;
        for (uint32_t i = 0; i < mesh_count; ++i)
        {
            offset = parseMesh(buffer, offset);
        }

        uint32_t grid_offset = *(uint32_t*)(buffer + header->collisionMeshOffset + 0x10);

        for (int y = 0; y < header->gridHeight * 10; ++y)
        {
            for (int x = 0; x < header->gridWidth * 10; ++x)
            {
                uint32_t offsets = (y * header->gridWidth * 10 + x) * 4;
                uint32_t entry_offset = *(uint32_t*)(buffer + grid_offset + offsets);
                if (entry_offset != 0)
                {
                    parseGridEntry(buffer, entry_offset, x, y);
                }
            }
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

    uint32_t MZB::parseMesh(uint8_t* buffer, uint32_t offset)
    {
        uint32_t vertices_offset = *(uint32_t*)(buffer + offset + 0x00);
        uint32_t normals_offset = *(uint32_t*)(buffer + offset + 0x04);
        uint32_t triangles = *(uint32_t*)(buffer + offset + 0x08);
        uint16_t triangle_count = *(uint16_t*)(buffer + offset + 0x0c);
        uint16_t flags = *(uint16_t*)(buffer + offset + 0x0e);

        CollisionMeshData& mesh = meshes.emplace_back();

        mesh.vertices.resize(normals_offset - vertices_offset);
        memcpy(mesh.vertices.data(), buffer + vertices_offset, normals_offset - vertices_offset);

        mesh.normals.resize(triangles - normals_offset);
        memcpy(mesh.normals.data(), buffer + normals_offset, triangles - normals_offset);

        mesh.indices.reserve(triangle_count * 3);
        for (int i = 0; i < triangle_count; ++i)
        {
            mesh.indices.push_back((*(uint16_t*)(buffer + triangles + (i * 4 + 0) * 2)) & 0x3FFF);
            mesh.indices.push_back((*(uint16_t*)(buffer + triangles + (i * 4 + 1) * 2)) & 0x3FFF);
            mesh.indices.push_back((*(uint16_t*)(buffer + triangles + (i * 4 + 2) * 2)) & 0x3FFF);
        }

        mesh_map.insert({ offset, meshes.size() - 1 });

        return triangles + triangle_count * sizeof(uint16_t) * 4;
    }
    
    void MZB::parseGridEntry(uint8_t* buffer, uint32_t offset, int x, int y)
    {
        std::vector<uint32_t> entries;

        while (true)
        {
            uint32_t entry = *(uint32_t*)(buffer + offset);
            if (entry == 0) break;
            entries.push_back(entry);
            offset += 4;
        }

        uint32_t pos = entries.front();
        uint32_t xx = (pos >> 14) & 0x1FF;
        uint32_t yy = (pos >> 23) & 0x1FF;
        uint32_t flags = pos & 0x3FFF;

        for (int i = 1; i < entries.size(); i += 2)
        {
            uint32_t vis_entry_offset = entries[i];
            uint32_t geo_entry_offset = entries[i+1];

            parseGridMesh(buffer, x, y, vis_entry_offset, geo_entry_offset);
        }
    }

    void MZB::parseGridMesh(uint8_t* buffer, int x, int y, uint32_t vis_entry_offset, uint32_t geo_entry_offset)
    {
        glm::mat4 transform_matrix = *(glm::mat4*)(buffer + vis_entry_offset);

        uint32_t mesh_offset = mesh_map[geo_entry_offset];

        mesh_entries.push_back({ glm::transpose(transform_matrix), mesh_offset });
    }

    CollisionLoader::CollisionLoader(std::vector<CollisionMeshData>& meshes, std::vector<CollisionEntry>& entries) : ModelLoader(), meshes(meshes), entries(entries) {}

    lotus::WorkerTask<> CollisionInitWork(lotus::Engine* engine, std::shared_ptr<lotus::Model> model, std::vector<FFXI::CollisionMeshData> mesh_data, std::vector<FFXI::CollisionEntry> entries, uint32_t vertex_stride)
    {
        std::vector<vk::DeviceSize> vertex_offsets;
        std::vector<vk::DeviceSize> index_offsets;

        CollisionMesh* mesh = static_cast<CollisionMesh*>(model->meshes[0].get());

        auto staging_buffer = engine->renderer->gpu->memory_manager->GetBuffer(mesh->vertex_buffer->getSize() + mesh->index_buffer->getSize() + mesh->transform_buffer->getSize(),
            vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        uint8_t* staging_map = static_cast<uint8_t*>(staging_buffer->map(0, VK_WHOLE_SIZE, {}));

        vk::DeviceSize staging_vertex_offset = 0;
        vk::DeviceSize staging_index_offset = 0;

        for (const auto& data : mesh_data)
        {
            memcpy(staging_map + staging_vertex_offset, data.vertices.data(), data.vertices.size());
            vertex_offsets.push_back(staging_vertex_offset);
            staging_vertex_offset += data.vertices.size();
            memcpy(staging_map + mesh->vertex_buffer->getSize() + staging_index_offset, data.indices.data(), data.indices.size() * sizeof(uint16_t));
            index_offsets.push_back(staging_index_offset);
            staging_index_offset += data.indices.size() * sizeof(uint16_t);
        }

        std::vector<vk::AccelerationStructureGeometryKHR> raytrace_geometry;
        std::vector<vk::AccelerationStructureBuildOffsetInfoKHR> raytrace_offset_info;
        std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR> raytrace_create_info;

        if (engine->config->renderer.RaytraceEnabled())
        {
            vk::DeviceSize transform_offset = 0;

            for (const auto& entry : entries)
            {
                glm::mat3x4 transform = entry.transform;
                memcpy(staging_map + mesh->vertex_buffer->getSize() + mesh->index_buffer->getSize() + transform_offset, &transform, sizeof(float) * 12);

                FFXI::CollisionMeshData& data = mesh_data[entry.mesh_entry];

                raytrace_geometry.emplace_back(vk::GeometryTypeKHR::eTriangles, vk::AccelerationStructureGeometryTrianglesDataKHR{
                    vk::Format::eR32G32B32Sfloat,
                    engine->renderer->gpu->device->getBufferAddressKHR(mesh->vertex_buffer->buffer),
                    vertex_stride,
                    vk::IndexType::eUint16,
                    engine->renderer->gpu->device->getBufferAddressKHR(mesh->index_buffer->buffer),
                    engine->renderer->gpu->device->getBufferAddressKHR(mesh->transform_buffer->buffer)
                    }, vk::GeometryFlagBitsKHR::eOpaque);

                raytrace_offset_info.emplace_back(static_cast<uint32_t>(data.indices.size() / 3), index_offsets[entry.mesh_entry], vertex_offsets[entry.mesh_entry] / vertex_stride, transform_offset);

                raytrace_create_info.emplace_back(vk::GeometryTypeKHR::eTriangles, static_cast<uint32_t>(data.indices.size() / 3),
                    vk::IndexType::eUint16, static_cast<uint32_t>(data.vertices.size() / vertex_stride), vk::Format::eR32G32B32Sfloat, true);

                transform_offset += sizeof(float) * 12;
            }
        }

        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *engine->renderer->compute_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

        auto command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info);

        vk::BufferCopy copy_region;
        copy_region.srcOffset = 0;
        copy_region.size = mesh->vertex_buffer->getSize();
        command_buffer->copyBuffer(staging_buffer->buffer, mesh->vertex_buffer->buffer, copy_region);

        copy_region.srcOffset = mesh->vertex_buffer->getSize();
        copy_region.size = mesh->index_buffer->getSize();
        command_buffer->copyBuffer(staging_buffer->buffer, mesh->index_buffer->buffer, copy_region);

        copy_region.srcOffset = mesh->vertex_buffer->getSize() + mesh->index_buffer->getSize();
        copy_region.size = mesh->transform_buffer->getSize();
        command_buffer->copyBuffer(staging_buffer->buffer, mesh->transform_buffer->buffer, copy_region);

        staging_buffer->unmap();

        if (engine->config->renderer.RaytraceEnabled())
        {
            vk::MemoryBarrier barrier;
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eAccelerationStructureReadKHR;
            command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, barrier, nullptr, nullptr);
            model->bottom_level_as = std::make_unique<lotus::BottomLevelAccelerationStructure>(static_cast<lotus::RendererRaytraceBase*>(engine->renderer.get()), *command_buffer, std::move(raytrace_geometry), std::move(raytrace_offset_info),
                std::move(raytrace_create_info), false, true, lotus::BottomLevelAccelerationStructure::Performance::FastTrace);
        }

        command_buffer->end();

        engine->worker_pool->command_buffers.compute.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(staging_buffer), std::move(command_buffer));
        co_return;
    }

    lotus::Task<> CollisionLoader::LoadModel(std::shared_ptr<lotus::Model>& model)
    {
        model->rendered = false;
        auto mesh = std::make_unique<CollisionMesh>();

        auto vertex_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
        auto index_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;
        auto transform_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer;

        if (engine->config->renderer.RaytraceEnabled())
        {
            vertex_usage_flags |= vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer;
            index_usage_flags |= vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer;
            transform_usage_flags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
        }

        auto vertex_buffer_size = 0;
        auto index_buffer_size = 0;
        auto transformation_buffer_size = entries.size() * sizeof(float) * 12;

        for (const auto& mesh : meshes)
        {
            vertex_buffer_size += mesh.vertices.size();
            index_buffer_size += mesh.indices.size() * 2;
        }

        mesh->vertex_buffer = engine->renderer->gpu->memory_manager->GetBuffer(vertex_buffer_size, vertex_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->index_buffer = engine->renderer->gpu->memory_manager->GetBuffer(index_buffer_size, index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->transform_buffer = engine->renderer->gpu->memory_manager->GetBuffer(transformation_buffer_size, transform_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);

        model->meshes.push_back(std::move(mesh));
        model->lifetime = lotus::Lifetime::Long;

        co_await CollisionInitWork(engine, model, std::move(meshes), std::move(entries), sizeof(float) * 3);
    }
}
