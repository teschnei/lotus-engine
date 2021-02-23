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
        mesh_info_buffer = renderer->gpu->memory_manager->GetBuffer(max_resource_index * sizeof(MeshInfo) * renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible);
        mesh_info_buffer_mapped = (MeshInfo*)mesh_info_buffer->map(0, max_resource_index * sizeof(MeshInfo) * renderer->getImageCount(), {});
    }

    GlobalResources::~GlobalResources()
    {
        if (mesh_info_buffer_mapped)
            mesh_info_buffer->unmap();
    }

    void GlobalResources::BindResources(uint32_t image)
    {
        auto offset = static_binding_offset.load(std::memory_order::relaxed);
        vk::WriteDescriptorSet write_info_vertex;
        write_info_vertex.descriptorType = vk::DescriptorType::eStorageBuffer;
        write_info_vertex.dstArrayElement = offset;
        write_info_vertex.dstBinding = 1;
        write_info_vertex.descriptorCount = static_cast<uint32_t>(descriptor_vertex_info.size());
        write_info_vertex.pBufferInfo = descriptor_vertex_info.data();

        vk::WriteDescriptorSet write_info_index;
        write_info_index.descriptorType = vk::DescriptorType::eStorageBuffer;
        write_info_index.dstArrayElement = offset;
        write_info_index.dstBinding = 2;
        write_info_index.descriptorCount = static_cast<uint32_t>(descriptor_index_info.size());
        write_info_index.pBufferInfo = descriptor_index_info.data();

        vk::WriteDescriptorSet write_info_texture;
        write_info_texture.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write_info_texture.dstArrayElement = offset;
        write_info_texture.dstBinding = 3;
        write_info_texture.descriptorCount = static_cast<uint32_t>(descriptor_texture_info.size());
        write_info_texture.pImageInfo = descriptor_texture_info.data();

        vk::WriteDescriptorSet write_info_material;
        write_info_material.descriptorType = vk::DescriptorType::eUniformBuffer;
        write_info_material.dstArrayElement = offset;
        write_info_material.dstBinding = 4;
        write_info_material.descriptorCount = static_cast<uint32_t>(descriptor_material_info.size());
        write_info_material.pBufferInfo = descriptor_material_info.data();

        vk::DescriptorBufferInfo mesh_info;
        mesh_info.buffer = mesh_info_buffer->buffer;
        mesh_info.offset = sizeof(MeshInfo) * max_resource_index * image;
        mesh_info.range = sizeof(MeshInfo) * max_resource_index;

        vk::WriteDescriptorSet write_info_mesh_info;
        write_info_mesh_info.descriptorType = vk::DescriptorType::eUniformBuffer;
        write_info_mesh_info.dstArrayElement = 0;
        write_info_mesh_info.dstBinding = 5;
        write_info_mesh_info.descriptorCount = 1;
        write_info_mesh_info.pBufferInfo = &mesh_info;

        engine->renderer->bindResources(image, write_info_vertex, write_info_index, write_info_material, write_info_texture, write_info_mesh_info);
        mesh_info_buffer->flush(max_resource_index * sizeof(MeshInfo) * image, max_resource_index * sizeof(MeshInfo));
    }

    void GlobalResources::Reset()
    {
        descriptor_vertex_info.clear();
        descriptor_index_info.clear();
        descriptor_material_info.clear();
        descriptor_texture_info.clear();
        mesh_info_offset = 0;
    }

    void GlobalResources::AddResources(Model* model)
    {
        uint32_t image = engine->renderer->getCurrentImage();
        uint16_t vertex_index = static_cast<uint16_t>(descriptor_vertex_info.size()) + static_binding_offset_data;
        uint16_t index_index = static_cast<uint16_t>(descriptor_index_info.size()) + static_binding_offset_data;
        uint16_t material_index = static_cast<uint16_t>(descriptor_texture_info.size()) + static_binding_offset_data;
        uint16_t mesh_info_index = mesh_info_offset + static_binding_offset_data;
        for (size_t i = 0; i < model->meshes.size(); ++i)
        {
            auto& mesh = model->meshes[i];
            descriptor_vertex_info.emplace_back(mesh->vertex_buffer->buffer, 0, VK_WHOLE_SIZE);
            descriptor_index_info.emplace_back(mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE);
            descriptor_texture_info.emplace_back(*mesh->material->texture->sampler, *mesh->material->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
            auto [buffer, offset] = mesh->material->getBuffer();
            descriptor_material_info.emplace_back(buffer, offset, Material::getMaterialBufferSize(engine));
            mesh->material->index = material_index + i;
            mesh_info_buffer_mapped[image * max_resource_index + mesh_info_index + i] = { vertex_index + (uint32_t)i, index_index + (uint32_t)i, (uint32_t)mesh->getIndexCount(), (uint32_t)mesh->material->index, glm::vec3{1.0}, 0, glm::vec4{1.f} };
        }
        model->resource_index = mesh_info_index;
        mesh_info_offset += model->meshes.size();
    }

    void GlobalResources::AddResources(DeformableEntity* entity)
    {
        uint32_t image = engine->renderer->getCurrentImage();
        for (size_t i = 0; i < entity->models.size(); ++i)
        {
            uint16_t vertex_index = static_cast<uint16_t>(descriptor_vertex_info.size()) + static_binding_offset_data;
            uint16_t index_index = static_cast<uint16_t>(descriptor_index_info.size()) + static_binding_offset_data;
            uint16_t material_index = static_cast<uint16_t>(descriptor_texture_info.size()) + static_binding_offset_data;
            uint16_t mesh_info_index = mesh_info_offset + static_binding_offset_data;
            for (size_t j = 0; j < entity->models[i]->meshes.size(); ++j)
            {
                const auto& mesh = entity->models[i]->meshes[j];
                descriptor_vertex_info.emplace_back(entity->animation_component->transformed_geometries[i].vertex_buffers[j][image]->buffer, 0, VK_WHOLE_SIZE);
                descriptor_index_info.emplace_back(mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE);
                descriptor_texture_info.emplace_back(*mesh->material->texture->sampler, *mesh->material->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
                auto [buffer, offset] = mesh->material->getBuffer();
                descriptor_material_info.emplace_back(buffer, offset, Material::getMaterialBufferSize(engine));
                mesh->material->index = material_index + j;
                mesh_info_buffer_mapped[image * max_resource_index + mesh_info_index + j] = { vertex_index + (uint32_t)j, index_index + (uint32_t)j, (uint32_t)mesh->getIndexCount(), (uint32_t)mesh->material->index, entity->getScale(), 0, glm::vec4{1.f} };
            }
            entity->animation_component->transformed_geometries[i].resource_index = mesh_info_index;
            mesh_info_offset += entity->models[i]->meshes.size();
        }
    }

    void GlobalResources::AddResources(Particle* entity)
    {
        uint32_t image = engine->renderer->getCurrentImage();
        uint16_t vertex_index = static_cast<uint16_t>(descriptor_vertex_info.size()) + static_binding_offset_data;
        uint16_t index_index = static_cast<uint16_t>(descriptor_index_info.size()) + static_binding_offset_data;
        uint16_t material_index = static_cast<uint16_t>(descriptor_texture_info.size()) + static_binding_offset_data;
        uint16_t mesh_info_index = mesh_info_offset + static_binding_offset_data;
        auto& model = entity->models[0];
        for (size_t i = 0; i < model->meshes.size(); ++i)
        {
            auto& mesh = model->meshes[i];
            descriptor_vertex_info.emplace_back(mesh->vertex_buffer->buffer, 0, VK_WHOLE_SIZE);
            descriptor_index_info.emplace_back(mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE);
            descriptor_texture_info.emplace_back(*mesh->material->texture->sampler, *mesh->material->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
            auto [buffer, offset] = mesh->material->getBuffer();
            descriptor_material_info.emplace_back(buffer, offset, Material::getMaterialBufferSize(engine));
            mesh->material->index = material_index + i;
            mesh_info_buffer_mapped[image * max_resource_index + mesh_info_index + i] = { vertex_index + (uint32_t)i, index_index + (uint32_t)i, (uint32_t)mesh->getIndexCount(), (uint32_t)mesh->material->index, entity->getScale(), entity->billboard, entity->color };
        }
        uint32_t index = mesh_info_index;
        mesh_info_offset += model->meshes.size();
        entity->resource_index = index;
    }
}
