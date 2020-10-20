#include "actor.h"

#include "engine/core.h"
#include "dat/dat_parser.h"
#include "dat/os2.h"
#include "dat/sk2.h"
#include "dat/dxt3.h"
#include "dat/mo2.h"

Actor::Actor(lotus::Engine* engine) : lotus::DeformableEntity(engine)
{
}

lotus::Task<std::shared_ptr<Actor>> Actor::Init(lotus::Engine* engine, const std::filesystem::path& dat)
{
    auto actor = std::make_shared<Actor>(engine);
    co_await actor->Load(dat);
    co_return std::move(actor);
}

lotus::WorkerTask<> Actor::Load(const std::filesystem::path& dat)
{
    FFXI::DatParser parser{ dat, engine->config->renderer.RaytraceEnabled() };

    auto skel = std::make_unique<lotus::Skeleton>();
    FFXI::SK2* pSk2{ nullptr };
    std::vector<FFXI::OS2*> os2s;
    std::vector<lotus::Task<std::shared_ptr<lotus::Texture>>> texture_tasks;

    for (const auto& chunk : parser.root->children)
    {
        if (auto dxt3 = dynamic_cast<FFXI::DXT3*>(chunk.get()))
        {
            if (dxt3->width > 0)
            {
                texture_tasks.push_back(lotus::Texture::LoadTexture<FFXI::DXT3Loader>(engine, dxt3->name, dxt3));
            }
        }
        else if (auto sk2 = dynamic_cast<FFXI::SK2*>(chunk.get()))
        {
            pSk2 = sk2;
            for(const auto& bone : sk2->bones)
            {
                skel->addBone(bone.parent_index, bone.rot, bone.trans);
            }
        }
        else if (auto mo2 = dynamic_cast<FFXI::MO2*>(chunk.get()))
        {
            std::unique_ptr<lotus::Animation> animation = std::make_unique<lotus::Animation>(skel.get());
            animation->name = mo2->name;
            animation->frame_duration = std::chrono::milliseconds(static_cast<int>(1000 * (1.f / 30.f) / mo2->speed));

            for (size_t i = 0; i < mo2->frames; ++i)
            {
                for (size_t bone = 0; bone < skel->bones.size(); ++bone)
                {
                    if (auto transform = mo2->animation_data.find(bone); transform != mo2->animation_data.end())
                    {
                        auto& mo2_transform = transform->second[i];
                        animation->addFrameData(i, bone, { mo2_transform.rot, mo2_transform.trans, mo2_transform.scale });
                    }
                    else
                    {
                        animation->addFrameData(i, bone, { glm::quat{1, 0, 0, 0}, glm::vec3{0}, glm::vec3{1} });
                    }
                }
            }
            skel->animations[animation->name] = std::move(animation);
        }
        else if (auto os2 = dynamic_cast<FFXI::OS2*>(chunk.get()))
        {
            os2s.push_back(os2);
        }
    }

    addSkeleton(std::move(skel), sizeof(FFXI::OS2::Vertex));

    auto [model, model_task] = lotus::Model::LoadModel<FFXIActorLoader>(engine, "iroha_test", os2s, pSk2);
    models.push_back(model);
    auto init_task = InitWork();

    //co_await all tasks
    for (const auto& task : texture_tasks)
    {
        co_await task;
    }
    if (model_task) co_await *model_task;
    co_await init_task;
}

FFXIActorLoader::FFXIActorLoader(const std::vector<FFXI::OS2*>& _os2s, FFXI::SK2* _sk2) : ModelLoader(), os2s(_os2s), sk2(_sk2)
{
}

lotus::Task<> FFXIActorLoader::LoadModel(std::shared_ptr<lotus::Model>& model)
{
    model->light_offset = 0;
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

        vk::BufferUsageFlags vertex_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
        vk::BufferUsageFlags index_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;

        vertex_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer;
        if (engine->config->renderer.RaytraceEnabled())
        {
            vertex_usage_flags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
            index_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        }

        mesh->vertex_buffer = engine->renderer->gpu->memory_manager->GetBuffer(vertices_uint8.size(), vertex_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->index_buffer = engine->renderer->gpu->memory_manager->GetBuffer(indices_uint8.size(), index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->setIndexCount(mesh_indices.size());
        mesh->setVertexCount(os2_vertices.size());
        mesh->setVertexInputAttributeDescription(FFXI::OS2::Vertex::getAttributeDescriptions());
        mesh->setVertexInputBindingDescription(FFXI::OS2::Vertex::getBindingDescriptions());

        vertices.push_back(std::move(vertices_uint8));
        indices.push_back(std::move(indices_uint8));
        model->meshes.push_back(std::move(mesh));
        }
    }
    model->lifetime = lotus::Lifetime::Short;
    model->weighted = true;

    co_await model->InitWork(engine, std::move(vertices), std::move(indices), sizeof(FFXI::OS2::WeightingVertex));
}
