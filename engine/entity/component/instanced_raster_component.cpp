#include "instanced_raster_component.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus::Component
{
    InstancedRasterComponent::InstancedRasterComponent(Entity* _entity, Engine* _engine, InstancedModelsComponent& models) :
         Component(_entity, _engine), models_component(models)
    {
    }

    WorkerTask<> InstancedRasterComponent::init()
    {
        uint32_t command_buffer_count = 0;
        if (engine->renderer->rasterizer) command_buffer_count++;
        if (engine->renderer->shadowmap_rasterizer) command_buffer_count++;
        if (command_buffer_count > 0)
        {
            vk::CommandBufferAllocateInfo alloc_info {
                .commandPool = *engine->renderer->graphics_pool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = command_buffer_count * engine->renderer->getFrameCount()
            };

            auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
            size_t buffer_index = 0;

            if (engine->renderer->rasterizer)
            {
                auto prev_image = engine->renderer->getFrameCount() - 1;
                for (size_t i = 0; i < engine->renderer->getFrameCount(); i++)
                {
                    drawModelsToBuffer(*command_buffers[buffer_index], i, prev_image);
                    render_buffers.push_back(std::move(command_buffers[buffer_index]));
                    ++buffer_index;
                    prev_image = i;
                }
            }

            if (engine->renderer->shadowmap_rasterizer)
            {
                for (size_t i = 0; i < engine->renderer->getFrameCount(); i++)
                {
                    drawShadowmapsToBuffer(*command_buffers[buffer_index], i);
                    shadowmap_buffers.push_back(std::move(command_buffers[buffer_index]));
                    ++buffer_index;
                }
            }
        }

        co_return;
    }

    WorkerTask<> InstancedRasterComponent::tick(time_point time, duration elapsed)
    {
        auto image = engine->renderer->getCurrentFrame();
        if (engine->renderer->rasterizer)
        {
            engine->worker_pool->command_buffers.graphics_secondary.queue(*render_buffers[image]);
        }

        if (engine->renderer->shadowmap_rasterizer)
        {
            engine->worker_pool->command_buffers.shadowmap.queue(*shadowmap_buffers[image]);
        }
        co_return;
    }

    void InstancedRasterComponent::drawModelsToBuffer(vk::CommandBuffer command_buffer, uint32_t image, uint32_t prev_image)
    {
        command_buffer.begin({
            .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
        });

        engine->renderer->rasterizer->beginMainCommandBufferRendering(command_buffer, vk::RenderingFlagBitsKHR::eResuming | vk::RenderingFlagBitsKHR::eSuspending);

        std::array camera_buffer_info
        {
            vk::DescriptorBufferInfo  {
                .buffer = engine->renderer->camera_buffers.view_proj_ubo->buffer,
                .offset = image * engine->renderer->uniform_buffer_align_up(sizeof(CameraComponent::CameraData)),
                .range = sizeof(CameraComponent::CameraData)
            },
            vk::DescriptorBufferInfo  {
                .buffer = engine->renderer->camera_buffers.view_proj_ubo->buffer,
                .offset = prev_image * engine->renderer->uniform_buffer_align_up(sizeof(CameraComponent::CameraData)),
                .range = sizeof(CameraComponent::CameraData)
            }
        };

        std::array descriptorWrites {
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = camera_buffer_info.size(),
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = camera_buffer_info.data()
            }
        };

        command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, engine->renderer->rasterizer->getPipelineLayout(), 0, descriptorWrites);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, engine->renderer->rasterizer->getPipelineLayout(), 1, engine->renderer->global_descriptors->getDescriptorSet(), {});

        vk::Viewport viewport {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)engine->renderer->swapchain->extent.width,
            .height = (float)engine->renderer->swapchain->extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vk::Rect2D scissor {
            .offset = vk::Offset2D{0, 0},
            .extent = engine->renderer->swapchain->extent
        };

        command_buffer.setScissor(0, scissor);
        command_buffer.setViewport(0, viewport);

        drawModels(command_buffer, false, false);
        drawModels(command_buffer, true, false);

        command_buffer.endRenderingKHR();
        command_buffer.end();
    }

    void InstancedRasterComponent::drawShadowmapsToBuffer(vk::CommandBuffer command_buffer, uint32_t image)
    {
        vk::CommandBufferInheritanceInfo inheritInfo {
 //           .renderPass = engine->renderer->shadowmap_rasterizer->getRenderPass()
        };

        command_buffer.begin({
            .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue,
            .pInheritanceInfo = &inheritInfo
        });

        vk::DescriptorBufferInfo cascade_buffer_info {
            .buffer = engine->renderer->camera_buffers.cascade_data_ubo->buffer,
            .offset = image * engine->renderer->uniform_buffer_align_up(sizeof(engine->renderer->cascade_data)),
            .range = sizeof(engine->renderer->cascade_data)
        };

        std::array descriptorWrites {
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &cascade_buffer_info
            },
        };

        command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, engine->renderer->shadowmap_rasterizer->getPipelineLayout(), 0, descriptorWrites);

        command_buffer.setDepthBias(1.25f, 0, 1.75f);

        vk::Viewport viewport {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)engine->settings.renderer_settings.shadowmap_dimension,
            .height = (float)engine->settings.renderer_settings.shadowmap_dimension,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vk::Rect2D scissor {
            .offset = vk::Offset2D{0, 0},
            .extent = { .width = engine->settings.renderer_settings.shadowmap_dimension, .height = engine->settings.renderer_settings.shadowmap_dimension }
        };

        command_buffer.setScissor(0, scissor);
        command_buffer.setViewport(0, viewport);

        drawModels(command_buffer, false, true);
        drawModels(command_buffer, true, true);

        command_buffer.end();
    }

    void InstancedRasterComponent::drawModels(vk::CommandBuffer command_buffer, bool transparency, bool shadowmap)
    {
        auto models = models_component.getModels();
        for (size_t model_i = 0; model_i < models.size(); ++model_i)
        {
            Model* model = models[model_i].model.get();
            auto [offset, count] = models_component.getInstanceOffset(model->name);
            if (count > 0 && !model->meshes.empty())
            {
                command_buffer.bindVertexBuffers(1, models_component.getInstanceBuffer(), offset * sizeof(InstancedModelsComponent::InstanceInfo));
                uint32_t material_index = 1;
                for (size_t i = 0; i < model->meshes.size(); ++i)
                {
                    auto& mesh = model->meshes[i];
                    if (mesh->has_transparency == transparency)
                    {
                        drawMesh(command_buffer, shadowmap, *mesh, models[model_i].mesh_infos->index + i, count);
                    }
                }
            }
        }
    }

    void InstancedRasterComponent::drawMesh(vk::CommandBuffer command_buffer, bool shadowmap, const Mesh& mesh, uint32_t material_index, uint32_t count)
    {
        vk::PipelineLayout pipeline_layout;

        if (!shadowmap)
        {
            pipeline_layout = engine->renderer->rasterizer->getPipelineLayout();
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mesh.pipelines[0]);
        }
        else
        {
            pipeline_layout = engine->renderer->shadowmap_rasterizer->getPipelineLayout();
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mesh.pipelines[1]);
        }

        command_buffer.pushConstants<uint32_t>(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, material_index);

        command_buffer.bindVertexBuffers(0, mesh.vertex_buffer->buffer, {0});
        command_buffer.bindIndexBuffer(mesh.index_buffer->buffer, 0, vk::IndexType::eUint16);

        command_buffer.drawIndexed(mesh.getIndexCount(), count, 0, 0, 0);
    }
}
