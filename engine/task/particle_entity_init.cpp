#include "particle_entity_init.h"
#include "engine/worker_thread.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/particle.h"

namespace lotus
{
    ParticleEntityInitTask::ParticleEntityInitTask(const std::shared_ptr<Particle>& _entity) : WorkItem(), entity(_entity)
    {
    }

    void ParticleEntityInitTask::Process(WorkerThread* thread)
    {
        entity->uniform_buffer = thread->engine->renderer->gpu->memory_manager->GetBuffer(thread->engine->renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * thread->engine->renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        entity->mesh_index_buffer = thread->engine->renderer->gpu->memory_manager->GetBuffer(thread->engine->renderer->uniform_buffer_align_up(sizeof(uint32_t)) * thread->engine->renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        entity->uniform_buffer_mapped = static_cast<uint8_t*>(entity->uniform_buffer->map(0, thread->engine->renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * thread->engine->renderer->getImageCount(), {}));
        entity->mesh_index_buffer_mapped = static_cast<uint8_t*>(entity->mesh_index_buffer->map(0, thread->engine->renderer->uniform_buffer_align_up(sizeof(uint32_t)) * thread->engine->renderer->getImageCount(), {}));

        createStaticCommandBuffers(thread);
    }

    void ParticleEntityInitTask::createStaticCommandBuffers(WorkerThread* thread)
    {
    }

    void ParticleEntityInitTask::drawModel(WorkerThread* thread, vk::CommandBuffer buffer, bool transparency, vk::PipelineLayout layout, size_t image)
    {
        for (size_t model_i = 0; model_i < entity->models.size(); ++model_i)
        {
            Model* model = entity->models[model_i].get();
            if (!model->meshes.empty())
            {
                //TODO: material_index can only work with one model (or one mesh)
                if (model->meshes.size() > 1)
                    DEBUG_BREAK();
                for (size_t mesh_i = 0; mesh_i < model->meshes.size(); ++mesh_i)
                {
                    Mesh* mesh = model->meshes[mesh_i].get();
                    if (mesh->has_transparency == transparency)
                    {
                        buffer.bindVertexBuffers(0, mesh->vertex_buffer->buffer, { 0 });
                        drawMesh(thread, buffer, *mesh, layout, mesh_i);
                    }
                }
            }
        }
    }

    void ParticleEntityInitTask::drawMesh(WorkerThread* thread, vk::CommandBuffer buffer, const Mesh& mesh, vk::PipelineLayout layout, uint32_t mesh_index)
    {
        vk::DescriptorImageInfo image_info;
        image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        //TODO: debug texture? probably AYAYA
        if (mesh.texture)
        {
            image_info.imageView = *mesh.texture->image_view;
            image_info.sampler = *mesh.texture->sampler;
        }

        std::array<vk::WriteDescriptorSet, 1> descriptorWrites = {};

        descriptorWrites[0].dstSet = nullptr;
        descriptorWrites[0].dstBinding = 1;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &image_info;

        buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, layout, 0, descriptorWrites);

        buffer.pushConstants<uint32_t>(layout, vk::ShaderStageFlagBits::eFragment, 0, mesh_index);

        buffer.bindIndexBuffer(mesh.index_buffer->buffer, 0, vk::IndexType::eUint16);

        buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0);
    }

    ParticleEntityReInitTask::ParticleEntityReInitTask(const std::shared_ptr<Particle>& entity) : ParticleEntityInitTask(entity)
    {
    }

    void ParticleEntityReInitTask::Process(WorkerThread* thread)
    {
        createStaticCommandBuffers(thread);
    }
}
