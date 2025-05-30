module;

#include <array>
#include <coroutine>
#include <memory>

export module lotus:entity.component.particle_raster;

import :core.engine;
import :entity.component;
import :entity.component.camera;
import :entity.component.particle;
import :entity.component.render_base;
import :renderer.memory;
import :renderer.mesh;
import :util;
import vulkan_hpp;

export namespace lotus::Component
{
class ParticleRasterComponent : public Component<ParticleRasterComponent, lotus::Component::After<ParticleComponent, RenderBaseComponent>>
{
public:
    explicit ParticleRasterComponent(Entity*, Engine* engine, const ParticleComponent& particle, const RenderBaseComponent& base);

    WorkerTask<> tick(time_point time, duration elapsed);

protected:
    const ParticleComponent& particle_component;
    const RenderBaseComponent& base_component;
    void drawModelsToBuffer(vk::CommandBuffer command_buffer);
    void drawModels(vk::CommandBuffer command_buffer, bool transparency);
    void drawMesh(vk::CommandBuffer command_buffer, const Mesh& mesh, uint32_t material_index, uint32_t pipeline_index);
};

ParticleRasterComponent::ParticleRasterComponent(Entity* _entity, Engine* _engine, const ParticleComponent& _particle_component,
                                                 const RenderBaseComponent& _base_component)
    : Component(_entity, _engine), particle_component(_particle_component), base_component(_base_component)
{
}

WorkerTask<> ParticleRasterComponent::tick(time_point time, duration elapsed)
{
    uint32_t command_buffer_count = 0;
    if (engine->renderer->rasterizer)
        command_buffer_count++;
    if (command_buffer_count > 0)
    {
        vk::CommandBufferAllocateInfo alloc_info{
            .commandPool = *engine->renderer->graphics_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = command_buffer_count};

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

    command_buffer.begin({
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

    engine->renderer->rasterizer->beginTransparencyCommandBufferRendering(command_buffer,
                                                                          vk::RenderingFlagBits::eResuming | vk::RenderingFlagBits::eSuspending);

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

    command_buffer.pushDescriptorSet(vk::PipelineBindPoint::eGraphics, engine->renderer->rasterizer->getPipelineLayout(), 0, descriptorWrites);
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

    drawModels(command_buffer, false);
    drawModels(command_buffer, true);

    command_buffer.endRendering();
    command_buffer.end();
}

void ParticleRasterComponent::drawModels(vk::CommandBuffer command_buffer, bool transparency)
{
    auto [model, info] = particle_component.getModel();
    if (!model->meshes.empty() && model->rendered)
    {
        for (size_t mesh_i = 0; mesh_i < model->meshes.size(); ++mesh_i)
        {
            Mesh* mesh = model->meshes[mesh_i].get();
            if (mesh->has_transparency == transparency)
            {
                auto quad_size = mesh->getVertexStride() * 6;
                auto vertex_buffer_offset = particle_component.current_sprite * quad_size;
                // TODO: this only works for single mesh particles
                command_buffer.bindVertexBuffers(0, model->vertex_buffer->buffer, {vertex_buffer_offset});
                command_buffer.bindIndexBuffer(model->index_buffer->buffer, mesh->index_offset, vk::IndexType::eUint16);
                // TODO: different for particles probably
                drawMesh(command_buffer, *mesh, info->index, particle_component.pipeline_index);
            }
        }
    }
}

void ParticleRasterComponent::drawMesh(vk::CommandBuffer command_buffer, const Mesh& mesh, uint32_t material_index, uint32_t pipeline_index)
{
    vk::PipelineLayout pipeline_layout;

    pipeline_layout = engine->renderer->rasterizer->getPipelineLayout();
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mesh.pipelines[pipeline_index]);

    command_buffer.pushConstants<uint32_t>(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, material_index);

    command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0);
}
} // namespace lotus::Component
