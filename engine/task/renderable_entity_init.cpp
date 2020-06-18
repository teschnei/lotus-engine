#include "renderable_entity_init.h"
#include "engine/worker_thread.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/deformable_entity.h"
#include "engine/entity/component/animation_component.h"

namespace lotus
{
    RenderableEntityInitTask::RenderableEntityInitTask(const std::shared_ptr<RenderableEntity>& _entity) : WorkItem(), entity(_entity)
    {
    }

    void RenderableEntityInitTask::Process(WorkerThread* thread)
    {
        if (auto deformable = dynamic_cast<DeformableEntity*>(entity.get()))
        {
            const auto& animation_component = deformable->animation_component;
            if (animation_component)
            {
                vk::CommandBufferAllocateInfo alloc_info;
                alloc_info.level = vk::CommandBufferLevel::ePrimary;
                alloc_info.commandPool = *thread->graphics_pool;
                alloc_info.commandBufferCount = 1;

                auto command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info);
                animation_component->transformed_geometries.push_back({});
                vk::CommandBufferBeginInfo begin_info = {};
                begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
                command_buffer = std::move(command_buffers[0]);

                command_buffer->begin(begin_info);
                for (size_t i = 0; i < entity->models.size(); ++i)
                {
                    const auto& model = entity->models[i];
                    if (model->weighted)
                    {
                        generateVertexBuffers(thread, *command_buffer, deformable, *model, animation_component->transformed_geometries.back().vertex_buffers);
                    }
                }
                command_buffer->end();
                graphics.primary = *command_buffer;
            }
        }

        entity->uniform_buffer = thread->engine->renderer.memory_manager->GetBuffer(thread->engine->renderer.uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * thread->engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        entity->mesh_index_buffer = thread->engine->renderer.memory_manager->GetBuffer(thread->engine->renderer.uniform_buffer_align_up(sizeof(uint32_t)) * thread->engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        entity->uniform_buffer_mapped = static_cast<uint8_t*>(entity->uniform_buffer->map(0, thread->engine->renderer.uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * thread->engine->renderer.getImageCount(), {}));
        entity->mesh_index_buffer_mapped = static_cast<uint8_t*>(entity->mesh_index_buffer->map(0, thread->engine->renderer.uniform_buffer_align_up(sizeof(uint32_t)) * thread->engine->renderer.getImageCount(), {}));
        createStaticCommandBuffers(thread);
    }

    void RenderableEntityInitTask::createStaticCommandBuffers(WorkerThread* thread)
    {
        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::eSecondary;
        alloc_info.commandPool = *thread->graphics_pool;
        alloc_info.commandBufferCount = static_cast<uint32_t>(thread->engine->renderer.getImageCount());

        entity->command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info);
        entity->shadowmap_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info);

        auto deformable = dynamic_cast<DeformableEntity*>(entity.get());

        if (thread->engine->renderer.RasterizationEnabled())
        {
            for (size_t i = 0; i < entity->command_buffers.size(); ++i)
            {
                auto& command_buffer = entity->command_buffers[i];
                vk::CommandBufferInheritanceInfo inheritInfo = {};
                inheritInfo.renderPass = *thread->engine->renderer.gbuffer_render_pass;
                inheritInfo.framebuffer = *thread->engine->renderer.gbuffer.frame_buffer;

                vk::CommandBufferBeginInfo beginInfo = {};
                beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
                beginInfo.pInheritanceInfo = &inheritInfo;

                command_buffer->begin(beginInfo);

                command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.main_pipeline_group.graphics_pipeline);

                vk::DescriptorBufferInfo camera_buffer_info;
                camera_buffer_info.buffer = thread->engine->renderer.camera_buffers.view_proj_ubo->buffer;
                camera_buffer_info.offset = i * thread->engine->renderer.uniform_buffer_align_up(sizeof(Camera::CameraData));
                camera_buffer_info.range = sizeof(Camera::CameraData);

                vk::DescriptorBufferInfo model_buffer_info;
                model_buffer_info.buffer = entity->uniform_buffer->buffer;
                model_buffer_info.offset = i * thread->engine->renderer.uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject));
                model_buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

                vk::DescriptorBufferInfo mesh_info;
                mesh_info.buffer = thread->engine->renderer.mesh_info_buffer->buffer;
                mesh_info.offset = sizeof(Renderer::MeshInfo) * Renderer::max_acceleration_binding_index * i;
                mesh_info.range = sizeof(Renderer::MeshInfo) * Renderer::max_acceleration_binding_index;

                vk::DescriptorBufferInfo material_index_info;
                material_index_info.buffer = entity->mesh_index_buffer->buffer;
                material_index_info.offset = i * thread->engine->renderer.uniform_buffer_align_up(sizeof(uint32_t));
                material_index_info.range = sizeof(uint32_t);

                std::array<vk::WriteDescriptorSet, 4> descriptorWrites = {};

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

                descriptorWrites[3].dstSet = nullptr;
                descriptorWrites[3].dstBinding = 4;
                descriptorWrites[3].dstArrayElement = 0;
                descriptorWrites[3].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[3].descriptorCount = 1;
                descriptorWrites[3].pBufferInfo = &material_index_info;

                command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.pipeline_layout, 0, descriptorWrites);

                drawModel(thread, *command_buffer, deformable, false, *thread->engine->renderer.pipeline_layout, i);

                command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.main_pipeline_group.blended_graphics_pipeline);

                drawModel(thread, *command_buffer, deformable, true, *thread->engine->renderer.pipeline_layout, i);

                command_buffer->end();
            }
        }

        if (thread->engine->renderer.render_mode == RenderMode::Rasterization)
        {
            for (size_t i = 0; i < entity->shadowmap_buffers.size(); ++i)
            {
                auto& command_buffer = entity->shadowmap_buffers[i];
                vk::CommandBufferInheritanceInfo inheritInfo = {};
                inheritInfo.renderPass = *thread->engine->renderer.shadowmap_render_pass;
                //used in multiple framebuffers so we can't give the hint
                //inheritInfo.framebuffer = *thread->engine->renderer.shadowmap_frame_buffer;

                vk::CommandBufferBeginInfo beginInfo = {};
                beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
                beginInfo.pInheritanceInfo = &inheritInfo;

                command_buffer->begin(beginInfo);

                vk::DescriptorBufferInfo buffer_info;
                buffer_info.buffer = entity->uniform_buffer->buffer;
                buffer_info.offset = i * sizeof(RenderableEntity::UniformBufferObject);
                buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

                vk::DescriptorBufferInfo cascade_buffer_info;
                cascade_buffer_info.buffer = thread->engine->renderer.camera_buffers.cascade_data_ubo->buffer;
                cascade_buffer_info.offset = i * thread->engine->renderer.uniform_buffer_align_up(sizeof(thread->engine->camera->cascade_data));
                cascade_buffer_info.range = sizeof(thread->engine->camera->cascade_data);

                std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};

                descriptorWrites[0].dstSet = nullptr;
                descriptorWrites[0].dstBinding = 2;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pBufferInfo = &buffer_info;

                descriptorWrites[1].dstSet = nullptr;
                descriptorWrites[1].dstBinding = 3;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pBufferInfo = &cascade_buffer_info;

                command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.shadowmap_pipeline_layout, 0, descriptorWrites);

                command_buffer->setDepthBias(1.25f, 0, 1.75f);

                command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.main_pipeline_group.shadowmap_pipeline);
                drawModel(thread, *command_buffer, deformable, false, *thread->engine->renderer.shadowmap_pipeline_layout, i);
                command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.main_pipeline_group.blended_shadowmap_pipeline);
                drawModel(thread, *command_buffer, deformable, true, *thread->engine->renderer.shadowmap_pipeline_layout, i);

                command_buffer->end();
            }
        }
    }

    void RenderableEntityInitTask::drawModel(WorkerThread* thread, vk::CommandBuffer command_buffer, DeformableEntity* deformable, bool transparency, vk::PipelineLayout layout, size_t image)
    {
        for (size_t model_i = 0; model_i < entity->models.size(); ++model_i)
        {
            Model* model = entity->models[model_i].get();
            if (!model->meshes.empty())
            {
                uint32_t material_index = 0;
                for (size_t mesh_i = 0; mesh_i < model->meshes.size(); ++mesh_i)
                {
                    Mesh* mesh = model->meshes[mesh_i].get();
                    if (mesh->has_transparency == transparency)
                    {
                        if (deformable)
                        {
                            command_buffer.bindVertexBuffers(0, deformable->animation_component->transformed_geometries[model_i].vertex_buffers[mesh_i][image]->buffer, {0});
                        }
                        else
                        {
                            command_buffer.bindVertexBuffers(0, mesh->vertex_buffer->buffer, {0});
                        }
                        if (model->bottom_level_as)
                        {
                            material_index = model->bottom_level_as->resource_index + mesh_i;
                        }
                        drawMesh(thread, command_buffer, *mesh, layout, material_index);
                    }
                }
            }
        }
    }

    void RenderableEntityInitTask::drawMesh(WorkerThread* thread, vk::CommandBuffer command_buffer, const Mesh& mesh, vk::PipelineLayout layout, uint32_t material_index)
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

        command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, layout, 0, descriptorWrites);

        command_buffer.pushConstants<uint32_t>(layout, vk::ShaderStageFlagBits::eFragment, 0, material_index);

        command_buffer.bindIndexBuffer(mesh.index_buffer->buffer, 0, vk::IndexType::eUint16);

        command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0);
    }

    void RenderableEntityInitTask::generateVertexBuffers(WorkerThread* thread, vk::CommandBuffer command_buffer, DeformableEntity* entity, const Model& model,
        std::vector<std::vector<std::unique_ptr<Buffer>>>& vertex_buffer)
    {
        std::vector<std::vector<vk::AccelerationStructureGeometryKHR>> raytrace_geometry;
        std::vector<std::vector<vk::AccelerationStructureBuildOffsetInfoKHR>> raytrace_offset_info;
        std::vector<std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR>> raytrace_create_info;

        raytrace_geometry.resize(thread->engine->renderer.getImageCount());
        raytrace_offset_info.resize(thread->engine->renderer.getImageCount());
        raytrace_create_info.resize(thread->engine->renderer.getImageCount());
        const auto& animation_component = entity->animation_component;
        vertex_buffer.resize(model.meshes.size());
        for (size_t i = 0; i < model.meshes.size(); ++i)
        {
            const auto& mesh = model.meshes[i];

            for (uint32_t image = 0; image < thread->engine->renderer.getImageCount(); ++image)
            {
                size_t vertex_size = mesh->getVertexInputBindingDescription()[0].stride;
                vertex_buffer[i].push_back(thread->engine->renderer.memory_manager->GetBuffer(mesh->getVertexCount() * vertex_size,
                    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal));

                if (thread->engine->renderer.RaytraceEnabled())
                {
                    raytrace_geometry[image].emplace_back(vk::GeometryTypeKHR::eTriangles, vk::AccelerationStructureGeometryTrianglesDataKHR{
                        vk::Format::eR32G32B32Sfloat,
                        thread->engine->renderer.device->getBufferAddressKHR(vertex_buffer[i].back()->buffer),
                        vertex_size,
                        vk::IndexType::eUint16,
                        thread->engine->renderer.device->getBufferAddressKHR(mesh->index_buffer->buffer) 
                        }, mesh->has_transparency ? vk::GeometryFlagsKHR{} : vk::GeometryFlagBitsKHR::eOpaque);

                    raytrace_offset_info[image].emplace_back(mesh->getIndexCount() / 3, 0, 0);

                    raytrace_create_info[image].emplace_back(vk::GeometryTypeKHR::eTriangles, static_cast<uint32_t>(mesh->getIndexCount() / 3),
                        vk::IndexType::eUint16, mesh->getVertexCount(), vk::Format::eR32G32B32Sfloat, false);
                }
            }
        }

        if (thread->engine->renderer.RaytraceEnabled())
        {
            //transform skeleton with default animation before building AS to improve the bounding box accuracy
            //make sure all vertex and index buffers are finished transferring
            vk::MemoryBarrier transfer_barrier;
            transfer_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            transfer_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, transfer_barrier, nullptr, nullptr);

            auto component = entity->animation_component;
            auto& skeleton = component->skeleton;
            command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *thread->engine->renderer.animation_pipeline);

            for (uint32_t image_index = 0; image_index < thread->engine->renderer.getImageCount(); ++image_index)
            {
                vk::DescriptorBufferInfo skeleton_buffer_info;
                skeleton_buffer_info.buffer = entity->animation_component->skeleton_bone_buffer->buffer;
                skeleton_buffer_info.offset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * image_index;
                skeleton_buffer_info.range = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size();

                vk::WriteDescriptorSet skeleton_descriptor_set = {};

                skeleton_descriptor_set.dstSet = nullptr;
                skeleton_descriptor_set.dstBinding = 1;
                skeleton_descriptor_set.dstArrayElement = 0;
                skeleton_descriptor_set.descriptorType = vk::DescriptorType::eStorageBuffer;
                skeleton_descriptor_set.descriptorCount = 1;
                skeleton_descriptor_set.pBufferInfo = &skeleton_buffer_info;

                command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *thread->engine->renderer.animation_pipeline_layout, 0, skeleton_descriptor_set);

                for (size_t i = 0; i < entity->models.size(); ++i)
                {
                    for (size_t j = 0; j < entity->models[i]->meshes.size(); ++j)
                    {
                        auto& mesh = entity->models[i]->meshes[j];
                        auto& vertex_buffer = component->transformed_geometries[i].vertex_buffers[j][image_index];

                        vk::DescriptorBufferInfo vertex_weights_buffer_info;
                        vertex_weights_buffer_info.buffer = mesh->vertex_buffer->buffer;
                        vertex_weights_buffer_info.offset = 0;
                        vertex_weights_buffer_info.range = VK_WHOLE_SIZE;

                        vk::DescriptorBufferInfo vertex_output_buffer_info;
                        vertex_output_buffer_info.buffer = vertex_buffer->buffer;
                        vertex_output_buffer_info.offset = 0;
                        vertex_output_buffer_info.range = VK_WHOLE_SIZE;

                        vk::WriteDescriptorSet weight_descriptor_set{};
                        weight_descriptor_set.dstSet = nullptr;
                        weight_descriptor_set.dstBinding = 0;
                        weight_descriptor_set.dstArrayElement = 0;
                        weight_descriptor_set.descriptorType = vk::DescriptorType::eStorageBuffer;
                        weight_descriptor_set.descriptorCount = 1;
                        weight_descriptor_set.pBufferInfo = &vertex_weights_buffer_info;

                        vk::WriteDescriptorSet output_descriptor_set{};
                        output_descriptor_set.dstSet = nullptr;
                        output_descriptor_set.dstBinding = 2;
                        output_descriptor_set.dstArrayElement = 0;
                        output_descriptor_set.descriptorType = vk::DescriptorType::eStorageBuffer;
                        output_descriptor_set.descriptorCount = 1;
                        output_descriptor_set.pBufferInfo = &vertex_output_buffer_info;

                        command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *thread->engine->renderer.animation_pipeline_layout, 0, { weight_descriptor_set, output_descriptor_set });

                        command_buffer.dispatch(mesh->getVertexCount(), 1, 1);

                        if (thread->engine->renderer.RaytraceEnabled())
                        {
                            vk::BufferMemoryBarrier barrier;
                            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                            barrier.buffer = vertex_buffer->buffer;
                            barrier.size = VK_WHOLE_SIZE;
                            barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
                            barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR;

                            command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, nullptr, barrier, nullptr);
                        }
                    }
                }
            }
            for (size_t i = 0; i < thread->engine->renderer.getImageCount(); ++i)
            {
                animation_component->transformed_geometries.back().bottom_level_as.push_back(std::make_unique<BottomLevelAccelerationStructure>(thread->engine, command_buffer, std::move(raytrace_geometry[i]),
                    std::move(raytrace_offset_info[i]), std::move(raytrace_create_info[i]), true, model.lifetime == Lifetime::Long, BottomLevelAccelerationStructure::Performance::FastBuild));
            }
        }
    }

    RenderableEntityReInitTask::RenderableEntityReInitTask(const std::shared_ptr<RenderableEntity>& entity) : RenderableEntityInitTask(entity)
    {
    }

    void RenderableEntityReInitTask::Process(WorkerThread* thread)
    {
        createStaticCommandBuffers(thread);
    }
}
