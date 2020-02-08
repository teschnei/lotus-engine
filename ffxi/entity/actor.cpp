#include "actor.h"

#include "engine/core.h"
#include "engine/task/model_init.h"
#include "task/actor_dat_load.h"
#include "dat/os2.h"
#include "dat/sk2.h"

Actor::Actor(lotus::Engine* engine) : RenderableEntity(engine)
{
}

void Actor::Init(const std::shared_ptr<Actor>& sp, const std::string& dat)
{
    engine->worker_pool.addWork(std::make_unique<ActorDatLoad>(sp, dat));
}

FFXIActorLoader::FFXIActorLoader(const std::vector<FFXI::OS2*>& _os2s, FFXI::SK2* _sk2) : ModelLoader(), os2s(_os2s), sk2(_sk2)
{
}

void FFXIActorLoader::LoadModel(std::shared_ptr<lotus::Model>& model)
{
    std::vector<std::vector<uint8_t>> vertices;
    std::vector<std::vector<uint8_t>> indices;

    for (const auto& os2 : os2s)
    {
        for (const auto& os2_mesh : os2->meshes)
        {
            auto mesh = std::make_unique<lotus::Mesh>(); 
            mesh->has_transparency = true;
            mesh->specular_exponent = os2_mesh.specular_exponent;
            mesh->specular_intensity = os2_mesh.specular_intensity;

            std::vector<FFXI::OS2::WeightingVertex> os2_vertices;
            std::vector<uint8_t> vertices_uint8;
            std::vector<uint16_t> mesh_indices;
            std::vector<uint8_t> indices_uint8;
            mesh->texture = lotus::Texture::getTexture(os2_mesh.tex_name);
            int passes = os2->mirror ? 2 : 1;
            for (int i = 0; i < passes; ++i)
            {
                for (auto [index, uv] : os2_mesh.indices)
                {
                    const auto& vert = os2->vertices[index];
                    FFXI::OS2::WeightingVertex vertex;
                    vertex.uv = uv;
                    vertex.pos = vert.first.pos;
                    vertex.norm = vert.first.norm;
                    vertex.weight = vert.first.weight;
                    if (i == 0)
                    {
                        vertex.bone_index = vert.first.bone_index;
                        vertex.mirror_axis = 0;
                    }
                    else
                    {
                        vertex.bone_index = vert.first.bone_index_mirror;
                        vertex.mirror_axis = vert.first.mirror_axis;
                    }
                    os2_vertices.push_back(vertex);
                    vertex.pos = vert.second.pos;
                    vertex.norm = vert.second.norm;
                    vertex.weight = vert.second.weight;
                    if (i == 0)
                    {
                        vertex.bone_index = vert.second.bone_index;
                        vertex.mirror_axis = 0;
                    }
                    else
                    {
                        vertex.bone_index = vert.second.bone_index_mirror;
                        vertex.mirror_axis = vert.second.mirror_axis;
                    }
                    os2_vertices.push_back(vertex);

                    mesh_indices.push_back((uint16_t)mesh_indices.size());
                }
            }
        vertices_uint8.resize(os2_vertices.size() * sizeof(FFXI::OS2::WeightingVertex));
        memcpy(vertices_uint8.data(), os2_vertices.data(), vertices_uint8.size());
        indices_uint8.resize(mesh_indices.size() * sizeof(uint16_t));
        memcpy(indices_uint8.data(), mesh_indices.data(), indices_uint8.size());

        vk::BufferUsageFlags vertex_usage_flags = vk::BufferUsageFlagBits::eTransferDst;
        vk::BufferUsageFlags index_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;

        vertex_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer;
        if (engine->renderer.RTXEnabled())
        {
            index_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer;
        }

        mesh->vertex_buffer = engine->renderer.memory_manager->GetBuffer(vertices_uint8.size(), vertex_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->index_buffer = engine->renderer.memory_manager->GetBuffer(indices_uint8.size(), index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->setIndexCount(mesh_indices.size());
        mesh->setVertexCount(os2_vertices.size());

        vertices.push_back(std::move(vertices_uint8));
        indices.push_back(std::move(indices_uint8));
        model->meshes.push_back(std::move(mesh));
        }
    }
    model->lifetime = lotus::Lifetime::Short;
    model->weighted = true;
    if (model->meshes.size() > 32)
    {
        //RTX shader groups need to be increased if this goes up more, or some kind of refactoring needs to happen to move meshes to something that uses their own resource index
        __debugbreak();
    }
    engine->worker_pool.addWork(std::make_unique<lotus::ModelInitTask>(engine->renderer.getCurrentImage(), model, std::move(vertices), std::move(indices), sizeof(FFXI::OS2::WeightingVertex)));
}
