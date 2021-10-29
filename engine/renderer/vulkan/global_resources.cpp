#include "global_resources.h"

#include "engine/core.h"
#include "engine/renderer/model.h"
#include "engine/entity/deformable_entity.h"
#include "engine/entity/component/animation_component.h"
#include "engine/entity/particle.h"

namespace lotus
{
    GlobalResources::GlobalResources(Engine* _engine, Renderer* renderer) : engine(_engine)
    {
        mesh_info_buffer = renderer->gpu->memory_manager->GetBuffer(max_resource_index * sizeof(MeshInfo) * renderer->getImageCount(), vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostVisible);
        mesh_info_buffer_mapped = (MeshInfo*)mesh_info_buffer->map(0, max_resource_index * sizeof(MeshInfo) * renderer->getImageCount(), {});
    }

    GlobalResources::~GlobalResources()
    {
        if (mesh_info_buffer_mapped)
            mesh_info_buffer->unmap();
    }

    void GlobalResources::BindResources(uint32_t image)
    {
        auto descriptor_vertex_info_count = descriptor_vertex_count.load();
        auto descriptor_vertex_prev_info_count = descriptor_vertex_prev_count.load();
        auto descriptor_index_info_count = descriptor_index_count.load();
        auto descriptor_material_texture_info_count = descriptor_material_texture_count.load();
        auto offset = static_binding_offset.load();

        vk::DescriptorBufferInfo mesh_info;
        mesh_info.buffer = mesh_info_buffer->buffer;
        mesh_info.offset = sizeof(MeshInfo) * max_resource_index * image;
        mesh_info.range = sizeof(MeshInfo) * max_resource_index;

        std::array writes
        {
            vk::WriteDescriptorSet { //vertex
                .dstBinding = 1,
                .dstArrayElement = offset,
                .descriptorCount = descriptor_vertex_info_count,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = descriptor_vertex_info.data()
            },
            vk::WriteDescriptorSet { //vertex prev
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = descriptor_vertex_prev_info_count,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = descriptor_vertex_prev_info.data(),
            },
            vk::WriteDescriptorSet { //index
                .dstBinding = 3,
                .dstArrayElement = offset,
                .descriptorCount = descriptor_index_info_count,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = descriptor_index_info.data(),
            },
            vk::WriteDescriptorSet { //texture
                .dstBinding = 4,
                .dstArrayElement = offset,
                .descriptorCount = descriptor_material_texture_info_count,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = descriptor_texture_info.data(),
            },
            vk::WriteDescriptorSet { //material
                .dstBinding = 5,
                .dstArrayElement = offset,
                .descriptorCount = descriptor_material_texture_info_count,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = descriptor_material_info.data(),
            },
            vk::WriteDescriptorSet { //mesh info
                .dstBinding = 6,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &mesh_info
            },
        };

        engine->renderer->bindResources(image, writes);
        mesh_info_buffer->flush(max_resource_index * sizeof(MeshInfo) * image, max_resource_index * sizeof(MeshInfo));
    }

    void GlobalResources::Reset()
    {
        descriptor_vertex_count = 0;
        descriptor_vertex_prev_count = 0;
        descriptor_index_count = 0;
        descriptor_material_texture_count = 0;
        mesh_info_offset = 0;
    }

    void GlobalResources::AddResources(RenderableEntity* entity)
    {
        /*
        uint32_t image = engine->renderer->getCurrentImage();
        for (auto& model : entity->models)
        {
            if (!model->is_static)
            {
                uint16_t mesh_info_index = mesh_info_offset.fetch_add(model->meshes.size()) + static_binding_offset_data;
                for (size_t i = 0; i < model->meshes.size(); ++i)
                {
                    const auto& mesh = model->meshes[i];
                    uint16_t vertex_index = descriptor_vertex_info.push_back({ mesh->vertex_buffer->buffer, 0, VK_WHOLE_SIZE }) + static_binding_offset_data;
                    uint16_t index_index = descriptor_index_info.push_back({ mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE }) + static_binding_offset_data;
                    descriptor_texture_info.push_back({*mesh->material->texture->sampler, *mesh->material->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal });
                    auto [buffer, offset] = mesh->material->getBuffer();
                    uint16_t material_index = descriptor_material_info.push_back({ buffer, offset, Material::getMaterialBufferSize(engine) }) + static_binding_offset_data;
                    mesh->material->index = material_index;
                    mesh_info_buffer_mapped[image * max_resource_index + mesh_info_index + i] = { vertex_index, index_index, (uint32_t)mesh->getIndexCount(), (uint32_t)mesh->material->index, glm::vec3 { 1.0 }, 0, glm::vec4{ 1.f }, glm::vec2{ 0.f }, model->animation_frame, 0, entity->getPrevModelMatrix() };
                }
                model->resource_index = mesh_info_index;
                mesh_info_offset += model->meshes.size();
            }
        }*/
    }

    void GlobalResources::AddResources(DeformableEntity* entity)
    {
        /*
        uint32_t image = engine->renderer->getCurrentImage();
        uint32_t prev_image = engine->renderer->getPreviousImage();
        for (size_t i = 0; i < entity->models.size(); ++i)
        {
            uint16_t mesh_info_index = mesh_info_offset.fetch_add(entity->models[i]->meshes.size()) + static_binding_offset_data;
            for (size_t j = 0; j < entity->models[i]->meshes.size(); ++j)
            {
                const auto& mesh = entity->models[i]->meshes[j];
                uint16_t vertex_index = descriptor_vertex_info.push_back({ entity->animation_component->transformed_geometries[i].vertex_buffers[j][image]->buffer, 0, VK_WHOLE_SIZE }) + static_binding_offset_data;
                uint16_t vertex_prev_index = descriptor_vertex_prev_info.push_back({ entity->animation_component->transformed_geometries[i].vertex_buffers[j][prev_image]->buffer, 0, VK_WHOLE_SIZE });
                uint16_t index_index = descriptor_index_info.push_back({ mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE }) + static_binding_offset_data;
                descriptor_texture_info.push_back({ *mesh->material->texture->sampler, *mesh->material->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal }) + static_binding_offset_data;
                auto [buffer, offset] = mesh->material->getBuffer();
                uint16_t material_index = descriptor_material_info.push_back({ buffer, offset, Material::getMaterialBufferSize(engine) });
                mesh->material->index = material_index;
                mesh_info_buffer_mapped[image * max_resource_index + mesh_info_index + j] = { vertex_index, index_index, (uint32_t)mesh->getIndexCount(), (uint32_t)mesh->material->index, entity->getScale(), 0, glm::vec4{1.f}, glm::vec2{0.f}, entity->models[i]->animation_frame, vertex_prev_index, entity->getPrevModelMatrix()};
            }
            entity->animation_component->transformed_geometries[i].resource_index = mesh_info_index;
        }
        */
    }

    void GlobalResources::AddResources(Particle* entity)
    {
        /*
        uint32_t image = engine->renderer->getCurrentImage();
        uint16_t mesh_info_index = mesh_info_offset.fetch_add(entity->models[0]->meshes.size()) + static_binding_offset_data;
        auto& model = entity->models[0];
        for (size_t i = 0; i < model->meshes.size(); ++i)
        {
            auto& mesh = model->meshes[i];
            uint16_t vertex_index = descriptor_vertex_info.push_back({ mesh->vertex_buffer->buffer, 0, VK_WHOLE_SIZE }) + static_binding_offset_data;
            uint16_t index_index = descriptor_index_info.push_back({ mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE }) + static_binding_offset_data;
            descriptor_texture_info.push_back({ *mesh->material->texture->sampler, *mesh->material->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal }) + static_binding_offset_data;
            auto [buffer, offset] = mesh->material->getBuffer();
            uint16_t material_index = descriptor_material_info.push_back({ buffer, offset, Material::getMaterialBufferSize(engine) });
            mesh->material->index = material_index;
            mesh_info_buffer_mapped[image * max_resource_index + mesh_info_index + i] = { vertex_index, index_index, (uint32_t)mesh->getIndexCount(), (uint32_t)mesh->material->index, entity->getScale(), entity->billboard, entity->color, entity->uv_offset, model->animation_frame, 0, entity->getPrevModelMatrix() };
        }
        entity->resource_index = mesh_info_index;
        */
    }

    uint16_t GlobalResources::pushVertexInfo(std::span<vk::DescriptorBufferInfo> info)
    {
        auto index = descriptor_vertex_count.fetch_add(info.size());
        std::ranges::copy(info, descriptor_vertex_info.begin() + index);
        return index + static_binding_offset_data;
    }

    uint16_t GlobalResources::pushVertexPrevInfo(std::span<vk::DescriptorBufferInfo> info)
    {
        auto index = descriptor_vertex_prev_count.fetch_add(info.size());
        std::ranges::copy(info, descriptor_vertex_prev_info.begin() + index);
        return index;
    }
    
    uint16_t GlobalResources::pushIndexInfo(std::span<vk::DescriptorBufferInfo> info)
    {
        auto index = descriptor_index_count.fetch_add(info.size());
        std::ranges::copy(info, descriptor_index_info.begin() + index);
        return index + static_binding_offset_data;
    }

    uint16_t GlobalResources::pushMaterialTextureInfo(std::span<vk::DescriptorBufferInfo> info_material, std::span<vk::DescriptorImageInfo> info_texture)
    {
        auto index = descriptor_material_texture_count.fetch_add(std::max(info_material.size(), info_texture.size()));
        std::ranges::copy(info_material, descriptor_material_info.begin() + index);
        std::ranges::copy(info_texture, descriptor_texture_info.begin() + index);
        return index + static_binding_offset_data;
    }

    uint16_t GlobalResources::pushMeshInfo(std::span<MeshInfo> info)
    {
        auto index = mesh_info_offset.fetch_add(info.size());
        std::ranges::copy(info, mesh_info_buffer_mapped + (engine->renderer->getCurrentImage() * max_resource_index) + index + static_binding_offset_data);
        return index + static_binding_offset_data;
    }
}
