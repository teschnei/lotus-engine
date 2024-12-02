module;

#include <array>
#include <coroutine>
#include <memory>

export module lotus:entity.component.deformable_raster;

import :core.engine;
import :entity.component;
import :entity.component.camera;
import :entity.component.deformed_mesh;
import :entity.component.render_base;
import :renderer.memory;
import :renderer.mesh;
import :renderer.model;
import :renderer.skeleton;
import :util;
import vulkan_hpp;

export namespace lotus::Component
{
class DeformableRasterComponent : public Component<DeformableRasterComponent, After<DeformedMeshComponent, RenderBaseComponent>>
{
public:
    explicit DeformableRasterComponent(Entity*, Engine* engine, const DeformedMeshComponent& animation, const RenderBaseComponent& physics);

    WorkerTask<> tick(time_point time, duration elapsed);

protected:
    const DeformedMeshComponent& mesh_component;
    const RenderBaseComponent& base_component;
    void drawModelsToBuffer(vk::CommandBuffer command_buffer);
    void drawShadowmapsToBuffer(vk::CommandBuffer command_buffer);
    void drawModels(vk::CommandBuffer command_buffer, bool transparency, bool shadowmap);
    void drawMesh(vk::CommandBuffer command_buffer, bool shadowmap, const Model& model, const Mesh& mesh, uint32_t material_index);
};

DeformableRasterComponent::DeformableRasterComponent(Entity* _entity, Engine* _engine, const DeformedMeshComponent& _mesh_component,
                                                     const RenderBaseComponent& _base_component)
    : Component(_entity, _engine), mesh_component(_mesh_component), base_component(_base_component)
{
}

WorkerTask<> DeformableRasterComponent::tick(time_point time, duration elapsed)
{
    uint32_t command_buffer_count = 0;
    if (engine->renderer->rasterizer)
        command_buffer_count++;
    if (engine->renderer->shadowmap_rasterizer)
        command_buffer_count++;
    if (command_buffer_count > 0)
    {
        vk::CommandBufferAllocateInfo alloc_info{
            .commandPool = *engine->renderer->graphics_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = command_buffer_count};

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

        if (engine->renderer->rasterizer)
        {
            drawModelsToBuffer(*command_buffers[0]);
            engine->worker_pool->command_buffers.graphics_secondary.queue(*command_buffers[0]);
        }

        if (engine->renderer->shadowmap_rasterizer)
        {
            drawShadowmapsToBuffer(*command_buffers[1]);
            engine->worker_pool->command_buffers.shadowmap.queue(*command_buffers[1]);
        }

        engine->worker_pool->gpuResource(std::move(command_buffers));
    }
    co_return;
}

void DeformableRasterComponent::drawModelsToBuffer(vk::CommandBuffer command_buffer)
{
    uint32_t image = engine->renderer->getCurrentFrame();

    command_buffer.begin({
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

    engine->renderer->rasterizer->beginMainCommandBufferRendering(command_buffer, vk::RenderingFlagBitsKHR::eResuming | vk::RenderingFlagBitsKHR::eSuspending);

    std::array camera_buffer_info{vk::DescriptorBufferInfo{.buffer = engine->renderer->camera_buffers.view_proj_ubo->buffer,
                                                           .offset = image * engine->renderer->uniform_buffer_align_up(sizeof(CameraComponent::CameraData)),
                                                           .range = sizeof(CameraComponent::CameraData)},
                                  vk::DescriptorBufferInfo{.buffer = engine->renderer->camera_buffers.view_proj_ubo->buffer,
                                                           .offset = engine->renderer->getPreviousFrame() *
                                                                     engine->renderer->uniform_buffer_align_up(sizeof(CameraComponent::CameraData)),
                                                           .range = sizeof(CameraComponent::CameraData)}};

    auto [model_buffer, model_buffer_offset, model_buffer_range] = base_component.getUniformBuffer(image);

    vk::DescriptorBufferInfo model_buffer_info{.buffer = model_buffer, .offset = model_buffer_offset, .range = model_buffer_range};

    std::array descriptorWrites{vk::WriteDescriptorSet{.dstSet = nullptr,
                                                       .dstBinding = 0,
                                                       .dstArrayElement = 0,
                                                       .descriptorCount = camera_buffer_info.size(),
                                                       .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                       .pBufferInfo = camera_buffer_info.data()},
                                vk::WriteDescriptorSet{.dstSet = nullptr,
                                                       .dstBinding = 1,
                                                       .dstArrayElement = 0,
                                                       .descriptorCount = 1,
                                                       .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                       .pBufferInfo = &model_buffer_info}};

    command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, engine->renderer->rasterizer->getPipelineLayout(), 0, descriptorWrites);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, engine->renderer->rasterizer->getPipelineLayout(), 1,
                                      engine->renderer->global_descriptors->getDescriptorSet(), {});

    vk::Viewport viewport{.x = 0.0f,
                          .y = 0.0f,
                          .width = (float)engine->renderer->swapchain->extent.width,
                          .height = (float)engine->renderer->swapchain->extent.height,
                          .minDepth = 0.0f,
                          .maxDepth = 1.0f};

    vk::Rect2D scissor{.offset = vk::Offset2D{0, 0}, .extent = engine->renderer->swapchain->extent};

    command_buffer.setScissor(0, scissor);
    command_buffer.setViewport(0, viewport);

    drawModels(command_buffer, false, false);
    drawModels(command_buffer, true, false);

    command_buffer.endRenderingKHR();
    command_buffer.end();
}

void DeformableRasterComponent::drawShadowmapsToBuffer(vk::CommandBuffer command_buffer)
{
    uint32_t image = engine->renderer->getCurrentFrame();

    command_buffer.begin({.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue});

    auto [model_buffer, model_buffer_offset, model_buffer_range] = base_component.getUniformBuffer(image);

    vk::DescriptorBufferInfo model_buffer_info{.buffer = model_buffer, .offset = model_buffer_offset, .range = model_buffer_range};

    vk::DescriptorBufferInfo cascade_buffer_info{.buffer = engine->renderer->camera_buffers.cascade_data_ubo->buffer,
                                                 .offset = image * engine->renderer->uniform_buffer_align_up(sizeof(engine->renderer->cascade_data)),
                                                 .range = sizeof(engine->renderer->cascade_data)};

    std::array descriptorWrites{
        vk::WriteDescriptorSet{.dstSet = nullptr,
                               .dstBinding = 2,
                               .dstArrayElement = 0,
                               .descriptorCount = 1,
                               .descriptorType = vk::DescriptorType::eUniformBuffer,
                               .pBufferInfo = &model_buffer_info},
        vk::WriteDescriptorSet{.dstSet = nullptr,
                               .dstBinding = 3,
                               .dstArrayElement = 0,
                               .descriptorCount = 1,
                               .descriptorType = vk::DescriptorType::eUniformBuffer,
                               .pBufferInfo = &cascade_buffer_info},
    };

    command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, engine->renderer->shadowmap_rasterizer->getPipelineLayout(), 0, descriptorWrites);

    command_buffer.setDepthBias(1.25f, 0, 1.75f);

    vk::Viewport viewport{.x = 0.0f,
                          .y = 0.0f,
                          .width = (float)engine->settings.renderer_settings.shadowmap_dimension,
                          .height = (float)engine->settings.renderer_settings.shadowmap_dimension,
                          .minDepth = 0.0f,
                          .maxDepth = 1.0f};

    vk::Rect2D scissor{
        .offset = vk::Offset2D{0, 0},
        .extent = {.width = engine->settings.renderer_settings.shadowmap_dimension, .height = engine->settings.renderer_settings.shadowmap_dimension}};

    command_buffer.setScissor(0, scissor);
    command_buffer.setViewport(0, viewport);

    drawModels(command_buffer, false, true);
    drawModels(command_buffer, true, true);

    command_buffer.end();
}

void DeformableRasterComponent::drawModels(vk::CommandBuffer command_buffer, bool transparency, bool shadowmap)
{
    auto models = mesh_component.getModels();
    for (size_t model_i = 0; model_i < models.size(); ++model_i)
    {
        const DeformedMeshComponent::ModelInfo& info = models[model_i];
        Model* model = info.model.get();
        if (!model->meshes.empty() && model->rendered)
        {
            uint32_t material_index = 0;
            for (size_t mesh_i = 0; mesh_i < model->meshes.size(); ++mesh_i)
            {
                Mesh* mesh = model->meshes[mesh_i].get();
                if (mesh->has_transparency == transparency)
                {
                    command_buffer.bindVertexBuffers(0, info.vertex_buffers[engine->renderer->getCurrentFrame()]->buffer, {mesh->vertex_offset});
                    command_buffer.bindVertexBuffers(1, info.vertex_buffers[engine->renderer->getPreviousFrame()]->buffer, {mesh->vertex_offset});
                    material_index = info.mesh_infos[engine->renderer->getCurrentFrame()]->index + mesh_i;
                    drawMesh(command_buffer, shadowmap, *model, *mesh, material_index);
                }
            }
        }
    }
}

void DeformableRasterComponent::drawMesh(vk::CommandBuffer command_buffer, bool shadowmap, const Model& model, const Mesh& mesh, uint32_t material_index)
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

    command_buffer.bindIndexBuffer(model.index_buffer->buffer, mesh.index_offset, vk::IndexType::eUint16);

    command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0);
}
} // namespace lotus::Component
