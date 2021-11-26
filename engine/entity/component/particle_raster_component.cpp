#include "particle_raster_component.h"
#include "engine/core.h"
#include "engine/renderer/skeleton.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus::Component
{
    ParticleRasterComponent::ParticleRasterComponent(Entity* _entity, Engine* _engine, const ParticleComponent& _particle_component, const RenderBaseComponent& _base_component) :
         Component(_entity, _engine), particle_component(_particle_component), base_component(_base_component)
    {
    }

    WorkerTask<> ParticleRasterComponent::tick(time_point time, duration elapsed)
    {
        uint32_t command_buffer_count = 0;
        if (engine->renderer->rasterizer) command_buffer_count++;
        if (command_buffer_count > 0)
        {
            vk::CommandBufferAllocateInfo alloc_info {
                .commandPool = *engine->renderer->graphics_pool,
                .level = vk::CommandBufferLevel::eSecondary,
                .commandBufferCount = command_buffer_count
            };

            auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

            if (engine->renderer->rasterizer)
            {
                drawModelsToBuffer(*command_buffers[0]);
                engine->worker_pool->command_buffers.particle.queue(*command_buffers[0]);
            }

            engine->worker_pool->gpuResource(std::move(command_buffers));
        }
        co_return;
    }

    void ParticleRasterComponent::drawModelsToBuffer(vk::CommandBuffer command_buffer)
    {
        uint32_t image = engine->renderer->getCurrentFrame();

        vk::CommandBufferInheritanceInfo inheritInfo {
            .renderPass = engine->renderer->rasterizer->getRenderPass(),
            .subpass = 1,
            .framebuffer = *engine->renderer->rasterizer->getGBuffer().frame_buffer
        };

        command_buffer.begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit | vk::CommandBufferUsageFlagBits::eRenderPassContinue,
            .pInheritanceInfo = &inheritInfo
        });

        std::array camera_buffer_info
        {
            vk::DescriptorBufferInfo  {
                .buffer = engine->renderer->camera_buffers.view_proj_ubo->buffer,
                .offset = image * engine->renderer->uniform_buffer_align_up(sizeof(CameraComponent::CameraData)),
                .range = sizeof(CameraComponent::CameraData)
            },
            vk::DescriptorBufferInfo  {
                .buffer = engine->renderer->camera_buffers.view_proj_ubo->buffer,
                .offset = engine->renderer->getPreviousFrame() * engine->renderer->uniform_buffer_align_up(sizeof(CameraComponent::CameraData)),
                .range = sizeof(CameraComponent::CameraData)
            }
        };

        auto [model_buffer, model_buffer_offset, model_buffer_range] = base_component.getUniformBuffer(image);

        vk::DescriptorBufferInfo model_buffer_info {
            .buffer = model_buffer,
            .offset = model_buffer_offset,
            .range = model_buffer_range
        };

        vk::DescriptorBufferInfo mesh_info {
            .buffer = engine->renderer->resources->mesh_info_buffer->buffer,
            .offset = sizeof(GlobalResources::MeshInfo) * GlobalResources::max_resource_index * image,
            .range = sizeof(GlobalResources::MeshInfo) * GlobalResources::max_resource_index,
        };

        std::array descriptorWrites {
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = camera_buffer_info.size(),
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = camera_buffer_info.data()
            },
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &model_buffer_info
            },
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &mesh_info
            }
        };

        command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, engine->renderer->rasterizer->getPipelineLayout(), 0, descriptorWrites);

        drawModels(command_buffer, false);
        drawModels(command_buffer, true);

        command_buffer.end();
    }

    void ParticleRasterComponent::drawModels(vk::CommandBuffer command_buffer, bool transparency)
    {
        auto models = particle_component.getModels();
        for (size_t model_i = 0; model_i < models.size(); ++model_i)
        {
            Model* model = models[model_i].get();
            if (!model->meshes.empty() && model->rendered)
            {
                for (size_t mesh_i = 0; mesh_i < model->meshes.size(); ++mesh_i)
                {
                    Mesh* mesh = model->meshes[mesh_i].get();
                    if (mesh->has_transparency == transparency)
                    {
                        auto quad_size = mesh->getVertexStride() * 6;
                        auto vertex_buffer_offset = particle_component.current_sprite * quad_size;
                        command_buffer.bindVertexBuffers(0, mesh->vertex_buffer->buffer, {vertex_buffer_offset});
                        //TODO: different for particles probably
                        drawMesh(command_buffer, *mesh, particle_component.resource_index + mesh_i, particle_component.pipeline_index);
                    }
                }
            }
        }
    }

    void ParticleRasterComponent::drawMesh(vk::CommandBuffer command_buffer, const Mesh& mesh, uint32_t material_index, uint32_t pipeline_index)
    {
        vk::DescriptorBufferInfo material_info;

        vk::DescriptorImageInfo image_info;
        image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        //TODO: debug texture? probably AYAYA
        if (mesh.material)
        {
            auto [buffer, offset] = mesh.material->getBuffer();
            material_info.buffer = buffer;
            material_info.offset = offset;
            material_info.range = Material::getMaterialBufferSize(engine);
            if (mesh.material->texture)
            {
                image_info.imageView = *mesh.material->texture->image_view;
                image_info.sampler = *mesh.material->texture->sampler;
            }
        }

        vk::PipelineLayout pipeline_layout;

        pipeline_layout = engine->renderer->rasterizer->getPipelineLayout();
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mesh.pipelines[pipeline_index]);

        std::array descriptorWrites {
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &image_info
            },
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 4,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &material_info
            }
        };
        command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, descriptorWrites);

        command_buffer.pushConstants<uint32_t>(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, material_index);

        command_buffer.bindIndexBuffer(mesh.index_buffer->buffer, 0, vk::IndexType::eUint16);

        command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0);
    }
}
