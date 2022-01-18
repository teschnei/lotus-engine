#include "deformable_raster_component.h"
#include "engine/core.h"
#include "engine/renderer/skeleton.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus::Component
{
    DeformableRasterComponent::DeformableRasterComponent(Entity* _entity, Engine* _engine, const DeformedMeshComponent& _mesh_component, const RenderBaseComponent& _base_component) :
         Component(_entity, _engine), mesh_component(_mesh_component), base_component(_base_component)
    {
    }

    WorkerTask<> DeformableRasterComponent::tick(time_point time, duration elapsed)
    {
        uint32_t command_buffer_count = 0;
        if (engine->renderer->rasterizer) command_buffer_count++;
        if (engine->renderer->shadowmap_rasterizer) command_buffer_count++;
        if (command_buffer_count > 0)
        {
            vk::CommandBufferAllocateInfo alloc_info {
                .commandPool = *engine->renderer->graphics_pool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = command_buffer_count
            };

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

        drawModels(command_buffer, false, false);
        drawModels(command_buffer, true, false);

        command_buffer.endRenderingKHR();
        command_buffer.end();
    }

    void DeformableRasterComponent::drawShadowmapsToBuffer(vk::CommandBuffer command_buffer)
    {
        uint32_t image = engine->renderer->getCurrentFrame();

        command_buffer.begin({
            .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue
        });

        auto [model_buffer, model_buffer_offset, model_buffer_range] = base_component.getUniformBuffer(image);

        vk::DescriptorBufferInfo model_buffer_info {
            .buffer = model_buffer,
            .offset = model_buffer_offset,
            .range = model_buffer_range
        };

        vk::DescriptorBufferInfo cascade_buffer_info {
            .buffer = engine->renderer->camera_buffers.cascade_data_ubo->buffer,
            .offset = image * engine->renderer->uniform_buffer_align_up(sizeof(engine->renderer->cascade_data)),
            .range = sizeof(engine->renderer->cascade_data)
        };

        std::array descriptorWrites {
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
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &cascade_buffer_info
            },
        };

        command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, engine->renderer->shadowmap_rasterizer->getPipelineLayout(), 0, descriptorWrites);

        //TODO: i forget if i need this
        command_buffer.setDepthBias(1.25f, 0, 1.75f);

        drawModels(command_buffer, false, true);
        drawModels(command_buffer, true, true);

        command_buffer.end();
    }

    void DeformableRasterComponent::drawModels(vk::CommandBuffer command_buffer, bool transparency, bool shadowmap)
    {
        auto models = mesh_component.getModels();
        for (size_t model_i = 0; model_i < models.size(); ++model_i)
        {
            Model* model = models[model_i].get();
            if (!model->meshes.empty() && model->rendered)
            {
                uint32_t material_index = 0;
                for (size_t mesh_i = 0; mesh_i < model->meshes.size(); ++mesh_i)
                {
                    Mesh* mesh = model->meshes[mesh_i].get();
                    if (mesh->has_transparency == transparency)
                    {
                        const DeformedMeshComponent::ModelTransformedGeometry& transformed_geometry = mesh_component.getModelTransformGeometry(model_i);
                        command_buffer.bindVertexBuffers(0, transformed_geometry.vertex_buffers[mesh_i][engine->renderer->getCurrentFrame()]->buffer, {0});
                        command_buffer.bindVertexBuffers(1, transformed_geometry.vertex_buffers[mesh_i][engine->renderer->getPreviousFrame()]->buffer, {0});
                        material_index = transformed_geometry.resource_index + mesh_i;
                        drawMesh(command_buffer, shadowmap, *mesh, material_index);
                    }
                }
            }
        }
    }

    void DeformableRasterComponent::drawMesh(vk::CommandBuffer command_buffer, bool shadowmap, const Mesh& mesh, uint32_t material_index)
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

        if (!shadowmap)
        {
            pipeline_layout = engine->renderer->rasterizer->getPipelineLayout();
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mesh.pipelines[0]);

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
        }
        else
        {
            pipeline_layout = engine->renderer->shadowmap_rasterizer->getPipelineLayout();
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mesh.pipelines[1]);

            std::array descriptorWrites {
                vk::WriteDescriptorSet {
                    .dstSet = nullptr,
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                    .pImageInfo = &image_info
                }
            };
            command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, descriptorWrites);
        }

        command_buffer.pushConstants<uint32_t>(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, material_index);

        command_buffer.bindIndexBuffer(mesh.index_buffer->buffer, 0, vk::IndexType::eUint16);

        command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0);
    }
}
