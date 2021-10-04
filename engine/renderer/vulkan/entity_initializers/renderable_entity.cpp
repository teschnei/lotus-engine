#include "renderable_entity.h"

#include "engine/core.h"
#include "engine/entity/renderable_entity.h"
#include "engine/entity/deformable_entity.h"
#include "engine/entity/component/animation_component.h"
#include "engine/renderer/vulkan/raytrace/renderer_raytrace.h"
#include "engine/renderer/vulkan/raster/renderer_rasterization.h"
#include "engine/renderer/vulkan/hybrid/renderer_hybrid.h"

namespace lotus
{
    RenderableEntityInitializer::RenderableEntityInitializer(Entity* _entity) :
        EntityInitializer(_entity)
    {

    }

    void RenderableEntityInitializer::initEntity(RendererRaytrace* renderer, Engine* engine)
    {
        auto entity = static_cast<RenderableEntity*>(this->entity);
        if (auto deformable = dynamic_cast<DeformableEntity*>(entity))
        {
            const auto& animation_component = deformable->animation_component;
            if (animation_component)
            {
                vk::CommandBufferAllocateInfo alloc_info;
                alloc_info.level = vk::CommandBufferLevel::ePrimary;
                alloc_info.commandPool = *renderer->graphics_pool;
                alloc_info.commandBufferCount = 1;

                auto command_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
                command_buffer = std::move(command_buffers[0]);

                vk::CommandBufferBeginInfo begin_info = {};
                begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

                command_buffer->begin(begin_info);
                for (size_t i = 0; i < entity->models.size(); ++i)
                {
                    animation_component->transformed_geometries.push_back({});
                    const auto& model = entity->models[i];
                    if (model->weighted)
                    {
                        initModelWork(renderer, *command_buffer, deformable, *model, animation_component->transformed_geometries.back());
                    }
                }
                command_buffer->end();
                engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
            }
        }

        if (!entity->uniform_buffer)
        {
            entity->uniform_buffer = renderer->gpu->memory_manager->GetBuffer(renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            entity->uniform_buffer_mapped = static_cast<uint8_t*>(entity->uniform_buffer->map(0, renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * renderer->getImageCount(), {}));
        }
    }

    void RenderableEntityInitializer::initModel(RendererRaytrace* renderer, Engine* engine, Model& model, ModelTransformedGeometry& model_transform)
    {
        auto entity = static_cast<RenderableEntity*>(this->entity);
        if (auto deformable = dynamic_cast<DeformableEntity*>(entity))
        {
            const auto& animation_component = deformable->animation_component;
            if (animation_component)
            {
                vk::CommandBufferAllocateInfo alloc_info;
                alloc_info.level = vk::CommandBufferLevel::ePrimary;
                alloc_info.commandPool = *renderer->graphics_pool;
                alloc_info.commandBufferCount = 1;

                auto command_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
                command_buffer = std::move(command_buffers[0]);

                vk::CommandBufferBeginInfo begin_info = {};
                begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

                command_buffer->begin(begin_info);
                if (model.weighted)
                {
                    initModelWork(renderer, *command_buffer, deformable, model, model_transform);
                }
                command_buffer->end();
                engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
            }
        }
    }

    void RenderableEntityInitializer::initModelWork(RendererRaytrace* renderer, vk::CommandBuffer command_buffer, DeformableEntity* entity, const Model& model,
        ModelTransformedGeometry& model_transform)
    {
        std::vector<std::vector<vk::AccelerationStructureGeometryKHR>> raytrace_geometry;
        std::vector<std::vector<vk::AccelerationStructureBuildRangeInfoKHR>> raytrace_offset_info;
        std::vector<std::vector<uint32_t>> max_primitives;

        raytrace_geometry.resize(renderer->getImageCount());
        raytrace_offset_info.resize(renderer->getImageCount());
        max_primitives.resize(renderer->getImageCount());
        model_transform.vertex_buffers.resize(model.meshes.size());
        for (size_t i = 0; i < model.meshes.size(); ++i)
        {
            const auto& mesh = model.meshes[i];

            for (uint32_t image = 0; image < renderer->getImageCount(); ++image)
            {
                size_t vertex_size = mesh->getVertexInputBindingDescription()[0].stride;
                model_transform.vertex_buffers[i].push_back(renderer->gpu->memory_manager->GetBuffer(mesh->getVertexCount() * vertex_size,
                    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                    vk::MemoryPropertyFlagBits::eDeviceLocal));

                raytrace_geometry[image].push_back(vk::AccelerationStructureGeometryKHR{
                    .geometryType = vk::GeometryTypeKHR::eTriangles,
                    .geometry = { .triangles = vk::AccelerationStructureGeometryTrianglesDataKHR {
                        .vertexFormat = vk::Format::eR32G32B32Sfloat,
                        .vertexData = renderer->gpu->device->getBufferAddress({.buffer = model_transform.vertex_buffers[i].back()->buffer}),
                        .vertexStride = vertex_size,
                        .maxVertex = mesh->getMaxIndex(),
                        .indexType = vk::IndexType::eUint16,
                        .indexData = renderer->gpu->device->getBufferAddress({.buffer = mesh->index_buffer->buffer})
                    }},
                    .flags = mesh->has_transparency ? vk::GeometryFlagsKHR{} : vk::GeometryFlagBitsKHR::eOpaque
                });

                raytrace_offset_info[image].push_back({
                    .primitiveCount = static_cast<uint32_t>(mesh->getIndexCount() / 3)
                });

                max_primitives[image].emplace_back(static_cast<uint32_t>(mesh->getIndexCount() / 3));
            }
        }

        //transform skeleton with default animation before building AS to improve the bounding box accuracy
        //make sure all vertex and index buffers are finished transferring
        vk::MemoryBarrier2KHR barrier
        {
            .srcStageMask = vk::PipelineStageFlagBits2KHR::eTransfer,
            .srcAccessMask = vk::AccessFlagBits2KHR::eTransferWrite,
            .dstStageMask =  vk::PipelineStageFlagBits2KHR::eComputeShader,
            .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead
        };

        command_buffer.pipelineBarrier2KHR({
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &barrier
        });

        auto component = entity->animation_component;
        auto& skeleton = component->skeleton;
        command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *renderer->animation_pipeline);

        for (uint32_t image_index = 0; image_index < renderer->getImageCount(); ++image_index)
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

            command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *renderer->animation_pipeline_layout, 0, skeleton_descriptor_set);

            for (size_t j = 0; j < model.meshes.size(); ++j)
            {
                auto& mesh = model.meshes[j];
                auto& vertex_buffer = model_transform.vertex_buffers[j][image_index];

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

                command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *renderer->animation_pipeline_layout, 0, { weight_descriptor_set, output_descriptor_set });

                command_buffer.dispatch(mesh->getVertexCount(), 1, 1);

                vk::BufferMemoryBarrier2KHR barrier
                {
                    .srcStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                    .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                    .dstStageMask = vk::PipelineStageFlagBits2KHR::eAccelerationStructureBuild,
                    .dstAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureRead,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer = vertex_buffer->buffer,
                    .size = VK_WHOLE_SIZE
                };

                command_buffer.pipelineBarrier2KHR({
                    .bufferMemoryBarrierCount = 1,
                    .pBufferMemoryBarriers = &barrier
                });
            }
        }
        for (size_t i = 0; i < renderer->getImageCount(); ++i)
        {
            if (std::ranges::any_of(raytrace_geometry, [](auto geo) { return !geo.empty(); }))
            {
                model_transform.bottom_level_as.push_back(std::make_unique<BottomLevelAccelerationStructure>(renderer, command_buffer, std::move(raytrace_geometry[i]),
                    std::move(raytrace_offset_info[i]), std::move(max_primitives[i]), true, model.lifetime == Lifetime::Long, BottomLevelAccelerationStructure::Performance::FastBuild));
            }
            else
            {
                model_transform.bottom_level_as.push_back({});
            }
        }
    }

    void drawModel(Engine* engine, RenderableEntity* entity, vk::CommandBuffer command_buffer, DeformableEntity* deformable, bool transparency, bool shadowmap, vk::PipelineLayout layout, size_t image);
    void drawMesh(Engine* engine, vk::CommandBuffer command_buffer, const Mesh& mesh, vk::PipelineLayout layout, uint32_t material_index, bool shadowmap);

    void RenderableEntityInitializer::initEntity(RendererRasterization* renderer, Engine* engine)
    {
        auto entity = static_cast<RenderableEntity*>(this->entity);
        if (auto deformable = dynamic_cast<DeformableEntity*>(entity))
        {
            const auto& animation_component = deformable->animation_component;
            if (animation_component)
            {
                vk::CommandBufferAllocateInfo alloc_info;
                alloc_info.level = vk::CommandBufferLevel::ePrimary;
                alloc_info.commandPool = *renderer->graphics_pool;
                alloc_info.commandBufferCount = 1;

                auto command_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
                command_buffer = std::move(command_buffers[0]);

                vk::CommandBufferBeginInfo begin_info = {};
                begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

                command_buffer->begin(begin_info);
                for (size_t i = 0; i < entity->models.size(); ++i)
                {
                    animation_component->transformed_geometries.push_back({});
                    const auto& model = entity->models[i];
                    if (model->weighted)
                    {
                        initModelWork(renderer, *command_buffer, deformable, *model, animation_component->transformed_geometries.back());
                    }
                }
                command_buffer->end();
                engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
            }
        }

        entity->uniform_buffer = renderer->gpu->memory_manager->GetBuffer(renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        entity->uniform_buffer_mapped = static_cast<uint8_t*>(entity->uniform_buffer->map(0, renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * renderer->getImageCount(), {}));
    }

    void RenderableEntityInitializer::initModel(RendererRasterization* renderer, Engine* engine, Model& model, ModelTransformedGeometry& model_transform)
    {
        auto entity = static_cast<RenderableEntity*>(this->entity);
        if (auto deformable = dynamic_cast<DeformableEntity*>(entity))
        {
            const auto& animation_component = deformable->animation_component;
            if (animation_component)
            {
                vk::CommandBufferAllocateInfo alloc_info;
                alloc_info.level = vk::CommandBufferLevel::ePrimary;
                alloc_info.commandPool = *renderer->graphics_pool;
                alloc_info.commandBufferCount = 1;

                auto command_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
                command_buffer = std::move(command_buffers[0]);

                vk::CommandBufferBeginInfo begin_info = {};
                begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

                command_buffer->begin(begin_info);
                if (model.weighted)
                {
                    initModelWork(renderer, *command_buffer, deformable, model, model_transform);
                }
                command_buffer->end();
                engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
            }
        }
    }

    void RenderableEntityInitializer::drawEntity(RendererRasterization* renderer, Engine* engine, vk::CommandBuffer buffer, uint32_t image)
    {
        auto entity = static_cast<RenderableEntity*>(this->entity);
        auto deformable = dynamic_cast<DeformableEntity*>(entity);

        vk::CommandBufferInheritanceInfo inheritInfo = {};
        inheritInfo.renderPass = *renderer->gbuffer_render_pass;
        inheritInfo.framebuffer = *renderer->gbuffer.frame_buffer;

        vk::CommandBufferBeginInfo beginInfo = {};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
        beginInfo.pInheritanceInfo = &inheritInfo;

        buffer.begin(beginInfo);

        vk::DescriptorBufferInfo camera_buffer_info;
        camera_buffer_info.buffer = renderer->camera_buffers.view_proj_ubo->buffer;
        camera_buffer_info.offset = image * renderer->uniform_buffer_align_up(sizeof(Camera::CameraData));
        camera_buffer_info.range = sizeof(Camera::CameraData);

        vk::DescriptorBufferInfo model_buffer_info;
        model_buffer_info.buffer = entity->uniform_buffer->buffer;
        model_buffer_info.offset = image * renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject));
        model_buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

        vk::DescriptorBufferInfo mesh_info;
        mesh_info.buffer = renderer->resources->mesh_info_buffer->buffer;
        mesh_info.offset = sizeof(GlobalResources::MeshInfo) * GlobalResources::max_resource_index * image;
        mesh_info.range = sizeof(GlobalResources::MeshInfo) * GlobalResources::max_resource_index;

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
        descriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &mesh_info;

        buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *renderer->pipeline_layout, 0, descriptorWrites);

        drawModel(engine, entity, buffer, deformable, false, false, *renderer->pipeline_layout, image);
        drawModel(engine, entity, buffer, deformable, true, false, *renderer->pipeline_layout, image);

        buffer.end();
    }

    void RenderableEntityInitializer::drawEntityShadowmap(RendererRasterization* renderer, Engine* engine, vk::CommandBuffer buffer, uint32_t image)
    {
        auto entity = static_cast<RenderableEntity*>(this->entity);
        auto deformable = dynamic_cast<DeformableEntity*>(entity);

        vk::CommandBufferInheritanceInfo inheritInfo = {};
        inheritInfo.renderPass = *renderer->shadowmap_render_pass;

        vk::CommandBufferBeginInfo beginInfo = {};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
        beginInfo.pInheritanceInfo = &inheritInfo;

        buffer.begin(beginInfo);

        vk::DescriptorBufferInfo buffer_info;
        buffer_info.buffer = entity->uniform_buffer->buffer;
        buffer_info.offset = image * renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject));
        buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

        vk::DescriptorBufferInfo cascade_buffer_info;
        cascade_buffer_info.buffer = renderer->camera_buffers.cascade_data_ubo->buffer;
        cascade_buffer_info.offset = image * renderer->uniform_buffer_align_up(sizeof(renderer->cascade_data));
        cascade_buffer_info.range = sizeof(renderer->cascade_data);

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

        buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *renderer->shadowmap_pipeline_layout, 0, descriptorWrites);

        buffer.setDepthBias(1.25f, 0, 1.75f);

        drawModel(engine, entity, buffer, deformable, false, true, *renderer->shadowmap_pipeline_layout, image);
        drawModel(engine, entity, buffer, deformable, true, true, *renderer->shadowmap_pipeline_layout, image);

        buffer.end();
    }

    void drawModel(Engine* engine, RenderableEntity* entity, vk::CommandBuffer command_buffer, DeformableEntity* deformable, bool transparency, bool shadowmap, vk::PipelineLayout layout, size_t image)
    {
        for (size_t model_i = 0; model_i < entity->models.size(); ++model_i)
        {
            Model* model = entity->models[model_i].get();
            if (!model->meshes.empty() && model->rendered)
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
                            command_buffer.bindVertexBuffers(1, deformable->animation_component->transformed_geometries[model_i].vertex_buffers[mesh_i][engine->renderer->getPreviousImage()]->buffer, {0});
                            material_index = deformable->animation_component->transformed_geometries[model_i].resource_index + mesh_i;
                        }
                        else
                        {
                            command_buffer.bindVertexBuffers(0, mesh->vertex_buffer->buffer, {0});
                            command_buffer.bindVertexBuffers(1, mesh->vertex_buffer->buffer, {0});
                            material_index = model->resource_index + mesh_i;
                        }
                        drawMesh(engine, command_buffer, *mesh, layout, material_index, shadowmap);
                    }
                }
            }
        }
    }

    void drawMesh(Engine* engine, vk::CommandBuffer command_buffer, const Mesh& mesh, vk::PipelineLayout layout, uint32_t material_index, bool shadowmap)
    {
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, shadowmap ? mesh.pipeline_shadow : mesh.pipeline);
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

        std::vector<vk::WriteDescriptorSet> descriptorWrites{ 1 };

        descriptorWrites[0].dstSet = nullptr;
        descriptorWrites[0].dstBinding = 1;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &image_info;

        if (!shadowmap)
        {
            descriptorWrites.push_back({});
            descriptorWrites[1].dstSet = nullptr;
            descriptorWrites[1].dstBinding = 4;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = &material_info;
        }

        command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, layout, 0, descriptorWrites);

        command_buffer.pushConstants<uint32_t>(layout, vk::ShaderStageFlagBits::eFragment, 0, material_index);

        command_buffer.bindIndexBuffer(mesh.index_buffer->buffer, 0, vk::IndexType::eUint16);

        command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0);
    }

    void RenderableEntityInitializer::initModelWork(RendererRasterization* renderer, vk::CommandBuffer command_buffer, DeformableEntity* entity, const Model& model,
        ModelTransformedGeometry& model_transform)
    {
        model_transform.vertex_buffers.resize(model.meshes.size());
        for (size_t i = 0; i < model.meshes.size(); ++i)
        {
            const auto& mesh = model.meshes[i];

            for (uint32_t image = 0; image < renderer->getImageCount(); ++image)
            {
                size_t vertex_size = mesh->getVertexInputBindingDescription()[0].stride;
                model_transform.vertex_buffers[i].push_back(renderer->gpu->memory_manager->GetBuffer(mesh->getVertexCount() * vertex_size,
                    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal));
            }
        }
    }

    void RenderableEntityInitializer::initEntity(RendererHybrid* renderer, Engine* engine)
    {
        auto entity = static_cast<RenderableEntity*>(this->entity);
        if (auto deformable = dynamic_cast<DeformableEntity*>(entity))
        {
            const auto& animation_component = deformable->animation_component;
            if (animation_component)
            {
                vk::CommandBufferAllocateInfo alloc_info;
                alloc_info.level = vk::CommandBufferLevel::ePrimary;
                alloc_info.commandPool = *renderer->graphics_pool;
                alloc_info.commandBufferCount = 1;

                auto command_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
                vk::CommandBufferBeginInfo begin_info = {};
                begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
                command_buffer = std::move(command_buffers[0]);

                command_buffer->begin(begin_info);
                for (size_t i = 0; i < entity->models.size(); ++i)
                {
                    animation_component->transformed_geometries.push_back({});
                    const auto& model = entity->models[i];
                    if (model->weighted)
                    {
                        initModelWork(renderer, *command_buffer, deformable, *model, animation_component->transformed_geometries.back());
                    }
                }
                command_buffer->end();
                engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
            }
        }

        entity->uniform_buffer = renderer->gpu->memory_manager->GetBuffer(renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        entity->uniform_buffer_mapped = static_cast<uint8_t*>(entity->uniform_buffer->map(0, renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject)) * renderer->getImageCount(), {}));
    }

    void RenderableEntityInitializer::initModel(RendererHybrid* renderer, Engine* engine, Model& model, ModelTransformedGeometry& model_transform)
    {
        auto entity = static_cast<RenderableEntity*>(this->entity);
        if (auto deformable = dynamic_cast<DeformableEntity*>(entity))
        {
            const auto& animation_component = deformable->animation_component;
            if (animation_component)
            {
                vk::CommandBufferAllocateInfo alloc_info;
                alloc_info.level = vk::CommandBufferLevel::ePrimary;
                alloc_info.commandPool = *renderer->graphics_pool;
                alloc_info.commandBufferCount = 1;

                auto command_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
                command_buffer = std::move(command_buffers[0]);

                vk::CommandBufferBeginInfo begin_info = {};
                begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

                command_buffer->begin(begin_info);
                if (model.weighted)
                {
                    initModelWork(renderer, *command_buffer, deformable, model, model_transform);
                }
                command_buffer->end();
                engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
            }
        }
    }

    void RenderableEntityInitializer::drawEntity(RendererHybrid* renderer, Engine* engine, vk::CommandBuffer buffer, uint32_t image)
    {
        auto entity = static_cast<RenderableEntity*>(this->entity);
        auto deformable = dynamic_cast<DeformableEntity*>(entity);

        vk::CommandBufferInheritanceInfo inheritInfo = {};
        inheritInfo.renderPass = *renderer->gbuffer_render_pass;
        inheritInfo.framebuffer = *renderer->gbuffer.frame_buffer;

        vk::CommandBufferBeginInfo beginInfo = {};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
        beginInfo.pInheritanceInfo = &inheritInfo;

        buffer.begin(beginInfo);

        vk::DescriptorBufferInfo camera_buffer_info;
        camera_buffer_info.buffer = renderer->camera_buffers.view_proj_ubo->buffer;
        camera_buffer_info.offset = image * renderer->uniform_buffer_align_up(sizeof(Camera::CameraData));
        camera_buffer_info.range = sizeof(Camera::CameraData);

        vk::DescriptorBufferInfo model_buffer_info;
        model_buffer_info.buffer = entity->uniform_buffer->buffer;
        model_buffer_info.offset = image * renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject));
        model_buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

        vk::DescriptorBufferInfo mesh_info;
        mesh_info.buffer = renderer->resources->mesh_info_buffer->buffer;
        mesh_info.offset = sizeof(GlobalResources::MeshInfo) * GlobalResources::max_resource_index * image;
        mesh_info.range = sizeof(GlobalResources::MeshInfo) * GlobalResources::max_resource_index;

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
        descriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &mesh_info;

        buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *renderer->pipeline_layout, 0, descriptorWrites);

        drawModel(engine, entity, buffer, deformable, false, false, *renderer->pipeline_layout, image);
        drawModel(engine, entity, buffer, deformable, true, false, *renderer->pipeline_layout, image);

        buffer.end();
    }

    void RenderableEntityInitializer::initModelWork(RendererHybrid* renderer, vk::CommandBuffer command_buffer, DeformableEntity* entity, const Model& model,
        ModelTransformedGeometry& model_transform)
    {
        std::vector<std::vector<vk::AccelerationStructureGeometryKHR>> raytrace_geometry;
        std::vector<std::vector<vk::AccelerationStructureBuildRangeInfoKHR>> raytrace_offset_info;
        std::vector<std::vector<uint32_t>> max_primitives;

        raytrace_geometry.resize(renderer->getImageCount());
        raytrace_offset_info.resize(renderer->getImageCount());
        max_primitives.resize(renderer->getImageCount());
        const auto& animation_component = entity->animation_component;
        model_transform.vertex_buffers.resize(model.meshes.size());
        for (size_t i = 0; i < model.meshes.size(); ++i)
        {
            const auto& mesh = model.meshes[i];

            for (uint32_t image = 0; image < renderer->getImageCount(); ++image)
            {
                size_t vertex_size = mesh->getVertexInputBindingDescription()[0].stride;
                model_transform.vertex_buffers[i].push_back(renderer->gpu->memory_manager->GetBuffer(mesh->getVertexCount() * vertex_size,
                    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, vk::MemoryPropertyFlagBits::eDeviceLocal));

                raytrace_geometry[image].push_back(vk::AccelerationStructureGeometryKHR{
                    .geometryType = vk::GeometryTypeKHR::eTriangles,
                    .geometry = { .triangles = vk::AccelerationStructureGeometryTrianglesDataKHR {
                        .vertexFormat = vk::Format::eR32G32B32Sfloat,
                        .vertexData = renderer->gpu->device->getBufferAddress({.buffer = model_transform.vertex_buffers[i].back()->buffer}),
                        .vertexStride = vertex_size,
                        .maxVertex = mesh->getMaxIndex(),
                        .indexType = vk::IndexType::eUint16,
                        .indexData = renderer->gpu->device->getBufferAddress({.buffer = mesh->index_buffer->buffer})
                    }},
                    .flags = mesh->has_transparency ? vk::GeometryFlagsKHR{} : vk::GeometryFlagBitsKHR::eOpaque
                });

                raytrace_offset_info[image].push_back({
                    .primitiveCount = static_cast<uint32_t>(mesh->getIndexCount() / 3)
                });

                max_primitives[image].emplace_back(static_cast<uint32_t>(mesh->getIndexCount() / 3));
            }
        }

        //transform skeleton with default animation before building AS to improve the bounding box accuracy
        //make sure all vertex and index buffers are finished transferring
        vk::MemoryBarrier2KHR barrier
        {
            .srcStageMask = vk::PipelineStageFlagBits2KHR::eTransfer,
            .srcAccessMask = vk::AccessFlagBits2KHR::eTransferWrite,
            .dstStageMask =  vk::PipelineStageFlagBits2KHR::eComputeShader,
            .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead
        };

        command_buffer.pipelineBarrier2KHR({
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &barrier
        });

        auto component = entity->animation_component;
        auto& skeleton = component->skeleton;
        command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *renderer->animation_pipeline);

        for (uint32_t image_index = 0; image_index < renderer->getImageCount(); ++image_index)
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

            command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *renderer->animation_pipeline_layout, 0, skeleton_descriptor_set);

            for (size_t j = 0; j < model.meshes.size(); ++j)
            {
                auto& mesh = model.meshes[j];
                auto& vertex_buffer = model_transform.vertex_buffers[j][image_index];

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

                command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *renderer->animation_pipeline_layout, 0, { weight_descriptor_set, output_descriptor_set });

                command_buffer.dispatch(mesh->getVertexCount(), 1, 1);

                vk::BufferMemoryBarrier2KHR barrier
                {
                    .srcStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                    .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                    .dstStageMask = vk::PipelineStageFlagBits2KHR::eAccelerationStructureBuild,
                    .dstAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureRead,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer = vertex_buffer->buffer,
                    .size = VK_WHOLE_SIZE
                };

                command_buffer.pipelineBarrier2KHR({
                    .bufferMemoryBarrierCount = 1,
                    .pBufferMemoryBarriers = &barrier
                });
            }
        }
        for (size_t i = 0; i < renderer->getImageCount(); ++i)
        {
            if (std::ranges::any_of(raytrace_geometry, [](auto geo) { return !geo.empty(); }))
            {
                model_transform.bottom_level_as.push_back(std::make_unique<BottomLevelAccelerationStructure>(renderer, command_buffer, std::move(raytrace_geometry[i]),
                    std::move(raytrace_offset_info[i]), std::move(max_primitives[i]), true, model.lifetime == Lifetime::Long, BottomLevelAccelerationStructure::Performance::FastBuild));
            }
            else
            {
                model_transform.bottom_level_as.push_back({});
            }
        }
    }
}
