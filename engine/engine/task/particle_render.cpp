#include "particle_render.h"

#include "engine/core.h"
#include "engine/worker_thread.h"

namespace lotus
{
    ParticleRenderTask::ParticleRenderTask(std::shared_ptr<Particle>& _particle, float _priority) : WorkItem(), particle(_particle)
    {
        priority = _priority;
    }

    void ParticleRenderTask::Process(WorkerThread* thread)
    {
        auto i = thread->engine->renderer.getCurrentImage();

        RenderableEntity::UniformBufferObject ubo = {};
        ubo.model = particle->getModelMatrix();
        ubo.modelIT = glm::transpose(glm::inverse(glm::mat3(ubo.model)));

        auto& uniform_buffer = particle->uniform_buffer;
        auto data = uniform_buffer->map(0, sizeof(ubo) * thread->engine->renderer.getImageCount(), {});
            memcpy(static_cast<uint8_t*>(data)+(i*sizeof(ubo)), &ubo, sizeof(ubo));
        uniform_buffer->unmap();

        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::eSecondary;
        alloc_info.commandPool = *thread->graphics_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);
        command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferInheritanceInfo inherit_info = {};
        inherit_info.renderPass = *thread->engine->renderer.gbuffer_render_pass;
        inherit_info.framebuffer = *thread->engine->renderer.gbuffer.frame_buffer;
        inherit_info.subpass = 1;

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit |vk::CommandBufferUsageFlagBits::eRenderPassContinue;
        begin_info.pInheritanceInfo = &inherit_info;

        command_buffer->begin(begin_info, thread->engine->renderer.dispatch);

        if (thread->engine->renderer.RasterizationEnabled())
        {
            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.particle_pipeline_group.graphics_pipeline, thread->engine->renderer.dispatch);

            vk::DescriptorBufferInfo camera_buffer_info;
            camera_buffer_info.buffer = thread->engine->camera->view_proj_ubo->buffer;
            camera_buffer_info.offset = i * sizeof(Camera::CameraData);
            camera_buffer_info.range = sizeof(Camera::CameraData);

            vk::DescriptorBufferInfo model_buffer_info;
            model_buffer_info.buffer = particle->uniform_buffer->buffer;
            model_buffer_info.offset = i * (sizeof(RenderableEntity::UniformBufferObject));
            model_buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

            vk::DescriptorBufferInfo mesh_info;
            mesh_info.buffer = thread->engine->renderer.mesh_info_buffer->buffer;
            mesh_info.offset = sizeof(Renderer::MeshInfo) * Renderer::max_acceleration_binding_index * i;
            mesh_info.range = sizeof(Renderer::MeshInfo) * Renderer::max_acceleration_binding_index;

            std::array<vk::WriteDescriptorSet, 3> descriptorWrites = {};

            descriptorWrites[0].dstSet = nullptr;
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &camera_buffer_info;

            descriptorWrites[1].dstSet = nullptr;
            descriptorWrites[1].dstBinding = 2;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = &model_buffer_info;

            descriptorWrites[2].dstSet = nullptr;
            descriptorWrites[2].dstBinding = 3;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pBufferInfo = &mesh_info;

            command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.pipeline_layout, 0, descriptorWrites, thread->engine->renderer.dispatch);

            drawModel(thread, *command_buffer, false, *thread->engine->renderer.pipeline_layout, i);

            command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.particle_pipeline_group.blended_graphics_pipeline, thread->engine->renderer.dispatch);

            drawModel(thread, *command_buffer, true, *thread->engine->renderer.pipeline_layout, i);
        }

        command_buffer->end(thread->engine->renderer.dispatch);
        graphics.particle = *command_buffer;
    }

    void ParticleRenderTask::drawModel(WorkerThread* thread, vk::CommandBuffer command_buffer, bool transparency, vk::PipelineLayout layout, size_t image)
    {
        uint32_t material_index = particle->resource_index;
        for (size_t model_i = 0; model_i < particle->models.size(); ++model_i)
        {
            Model* model = particle->models[model_i].get();
            if (!model->meshes.empty())
            {
                //TODO: material_index can only work with one model (or one mesh)
                if (model->meshes.size() > 1)
                    __debugbreak();
                for (size_t mesh_i = 0; mesh_i < model->meshes.size(); ++mesh_i)
                {
                    Mesh* mesh = model->meshes[mesh_i].get();
                    if (mesh->has_transparency == transparency)
                    {
                        command_buffer.bindVertexBuffers(0, mesh->vertex_buffer->buffer, { 0 }, thread->engine->renderer.dispatch);
                        drawMesh(thread, command_buffer, *mesh, layout, material_index + mesh_i);
                    }
                }
            }
        }
    }

    void ParticleRenderTask::drawMesh(WorkerThread* thread, vk::CommandBuffer command_buffer, const Mesh& mesh, vk::PipelineLayout layout, uint32_t material_index)
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

        command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, layout, 0, descriptorWrites, thread->engine->renderer.dispatch);

        command_buffer.pushConstants<uint32_t>(layout, vk::ShaderStageFlagBits::eFragment, 0, material_index);

        command_buffer.bindIndexBuffer(mesh.index_buffer->buffer, 0, vk::IndexType::eUint16, thread->engine->renderer.dispatch);

        command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0, thread->engine->renderer.dispatch);
    }
}
