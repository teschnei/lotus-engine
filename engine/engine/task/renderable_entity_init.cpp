#include "renderable_entity_init.h"
#include "engine/worker_thread.h"
#include "engine/core.h"
#include "engine/renderer/renderer.h"
#include "engine/entity/component/animation_component.h"
#include "../../../ffxi/dat/os2.h"

namespace lotus
{
    RenderableEntityInitTask::RenderableEntityInitTask(const std::shared_ptr<RenderableEntity>& _entity) : WorkItem(), entity(_entity)
    {
    }

    void RenderableEntityInitTask::Process(WorkerThread* thread)
    {
        entity->uniform_buffer = thread->engine->renderer.memory_manager->GetBuffer(sizeof(RenderableEntity::UniformBufferObject) * thread->engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        //TODO: need new pipelines for these (also means FFXI::MMB::Vertex bindings don't need to be used
        //TODO: don't forget to check model->weighted for where the right vertex buffer is

        //vk::CommandBufferAllocateInfo alloc_info;
        //alloc_info.level = vk::CommandBufferLevel::eSecondary;
        //alloc_info.commandPool = *thread->command_pool;
        //alloc_info.commandBufferCount = static_cast<uint32_t>(thread->engine->renderer.getImageCount());

        //entity->command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);
        //entity->shadowmap_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);

        //if (thread->engine->renderer.RasterizationEnabled())
        //{
        //    for (int i = 0; i < entity->command_buffers.size(); ++i)
        //    {
        //        auto& command_buffer = entity->command_buffers[i];
        //        vk::CommandBufferInheritanceInfo inheritInfo = {};
        //        inheritInfo.renderPass = *thread->engine->renderer.gbuffer_render_pass;
        //        inheritInfo.framebuffer = *thread->engine->renderer.gbuffer.frame_buffer;

        //        vk::CommandBufferBeginInfo beginInfo = {};
        //        beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
        //        beginInfo.pInheritanceInfo = &inheritInfo;

        //        command_buffer->begin(beginInfo, thread->engine->renderer.dispatch);

        //        command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.main_graphics_pipeline, thread->engine->renderer.dispatch);

        //        vk::DescriptorBufferInfo buffer_info;
        //        buffer_info.buffer = thread->engine->camera.view_proj_ubo->buffer;
        //        buffer_info.offset = i * (sizeof(glm::mat4) * 4);
        //        buffer_info.range = sizeof(glm::mat4) * 4;

        //        std::array<vk::WriteDescriptorSet, 1> descriptorWrites = {};

        //        descriptorWrites[0].dstSet = nullptr;
        //        descriptorWrites[0].dstBinding = 0;
        //        descriptorWrites[0].dstArrayElement = 0;
        //        descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
        //        descriptorWrites[0].descriptorCount = 1;
        //        descriptorWrites[0].pBufferInfo = &buffer_info;

        //        command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.pipeline_layout, 0, descriptorWrites, thread->engine->renderer.dispatch);

        //        drawModel(thread, *command_buffer, false, *thread->engine->renderer.pipeline_layout);

        //        command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.blended_graphics_pipeline, thread->engine->renderer.dispatch);

        //        drawModel(thread, *command_buffer, true, *thread->engine->renderer.pipeline_layout);

        //        command_buffer->end(thread->engine->renderer.dispatch);
        //    }
        //}

        //if (thread->engine->renderer.render_mode == RenderMode::Rasterization)
        //{
        //    for (size_t i = 0; i < entity->shadowmap_buffers.size(); ++i)
        //    {
        //        auto& command_buffer = entity->shadowmap_buffers[i];
        //        vk::CommandBufferInheritanceInfo inheritInfo = {};
        //        inheritInfo.renderPass = *thread->engine->renderer.shadowmap_render_pass;
        //        //used in multiple framebuffers so we can't give the hint
        //        //inheritInfo.framebuffer = *thread->engine->renderer.shadowmap_frame_buffer;

        //        vk::CommandBufferBeginInfo beginInfo = {};
        //        beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
        //        beginInfo.pInheritanceInfo = &inheritInfo;

        //        command_buffer->begin(beginInfo, thread->engine->renderer.dispatch);

        //        command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.shadowmap_pipeline, thread->engine->renderer.dispatch);

        //        vk::DescriptorBufferInfo buffer_info;
        //        buffer_info.buffer = entity->uniform_buffer->buffer;
        //        buffer_info.offset = i * sizeof(RenderableEntity::UniformBufferObject);
        //        buffer_info.range = sizeof(RenderableEntity::UniformBufferObject);

        //        vk::DescriptorBufferInfo cascade_buffer_info;
        //        cascade_buffer_info.buffer = thread->engine->camera.cascade_data_ubo->buffer;
        //        cascade_buffer_info.offset = i * sizeof(thread->engine->camera.cascade_data);
        //        cascade_buffer_info.range = sizeof(thread->engine->camera.cascade_data);

        //        std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};

        //        descriptorWrites[0].dstSet = nullptr;
        //        descriptorWrites[0].dstBinding = 0;
        //        descriptorWrites[0].dstArrayElement = 0;
        //        descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
        //        descriptorWrites[0].descriptorCount = 1;
        //        descriptorWrites[0].pBufferInfo = &buffer_info;

        //        descriptorWrites[1].dstSet = nullptr;
        //        descriptorWrites[1].dstBinding = 2;
        //        descriptorWrites[1].dstArrayElement = 0;
        //        descriptorWrites[1].descriptorType = vk::DescriptorType::eUniformBuffer;
        //        descriptorWrites[1].descriptorCount = 1;
        //        descriptorWrites[1].pBufferInfo = &cascade_buffer_info;

        //        command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.shadowmap_pipeline_layout, 0, descriptorWrites, thread->engine->renderer.dispatch);

        //        command_buffer->setDepthBias(1.25f, 0, 1.75f);

        //        drawModel(thread, *command_buffer, false, *thread->engine->renderer.shadowmap_pipeline_layout);
        //        command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *thread->engine->renderer.blended_shadowmap_pipeline, thread->engine->renderer.dispatch);
        //        drawModel(thread, *command_buffer, true, *thread->engine->renderer.shadowmap_pipeline_layout);

        //        command_buffer->end(thread->engine->renderer.dispatch);
        //    }
        //}

        const auto& animation_component = entity->animation_component;
        if (animation_component)
        {
            vk::CommandBufferAllocateInfo alloc_info;
            alloc_info.level = vk::CommandBufferLevel::ePrimary;
            alloc_info.commandPool = *thread->command_pool;
            alloc_info.commandBufferCount = 1;

            auto command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);
            animation_component->acceleration_structures.push_back({});
            vk::CommandBufferBeginInfo begin_info = {};
            begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
            command_buffer = std::move(command_buffers[0]);

            command_buffer->begin(begin_info, thread->engine->renderer.dispatch);
            for (size_t i = 0; i < entity->models.size(); ++i)
            {
                const auto& model = entity->models[i];
                if (model->weighted)
                {
                    generateVertexBuffers(thread, *command_buffer, *model, animation_component->acceleration_structures.back().vertex_buffers);
                }
            }
            command_buffer->end(thread->engine->renderer.dispatch);
            thread->primary_buffers[thread->engine->renderer.getCurrentImage()].push_back(*command_buffer);
        }
    }

    void RenderableEntityInitTask::drawModel(WorkerThread* thread, vk::CommandBuffer command_buffer, bool transparency, vk::PipelineLayout layout)
    {
        for (const auto& model : entity->models)
        {
            if (!model->meshes.empty())
            {
                for (const auto& mesh : model->meshes)
                {
                    if (mesh->has_transparency == transparency)
                    {
                        drawMesh(thread, command_buffer, *mesh, layout);
                    }
                }
            }
        }
    }

    void RenderableEntityInitTask::drawMesh(WorkerThread* thread, vk::CommandBuffer command_buffer, const Mesh& mesh, vk::PipelineLayout layout)
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

        command_buffer.bindVertexBuffers(0, mesh.vertex_buffer->buffer, {0}, thread->engine->renderer.dispatch);
        command_buffer.bindIndexBuffer(mesh.index_buffer->buffer, {0}, vk::IndexType::eUint16, thread->engine->renderer.dispatch);

        command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0, thread->engine->renderer.dispatch);
    }

    void RenderableEntityInitTask::generateVertexBuffers(WorkerThread* thread, vk::CommandBuffer command_buffer, const Model& model,
        std::vector<std::vector<std::unique_ptr<Buffer>>>& vertex_buffer)
    {
        std::vector<std::vector<vk::GeometryNV>> raytrace_geometry;
        raytrace_geometry.resize(thread->engine->renderer.getImageCount());
        const auto& animation_component = entity->animation_component;
        const auto& skeleton = animation_component->skeleton;
        vertex_buffer.resize(model.meshes.size());
        for (size_t i = 0; i < model.meshes.size(); ++i)
        {
            const auto& mesh = model.meshes[i];

            auto weighted_vertex_buffer = static_cast<FFXI::OS2::WeightingVertex*>(thread->engine->renderer.device->mapMemory(mesh->vertex_buffer->memory, mesh->vertex_buffer->memory_offset, VK_WHOLE_SIZE, {}, thread->engine->renderer.dispatch));
            std::vector<FFXI::OS2::Vertex> vertices;

            for (size_t w = 0; w < mesh->getVertexCount(); w += 2)
            {
                auto vertex1 = weighted_vertex_buffer[w];
                auto vertex2 = weighted_vertex_buffer[w+1];

                FFXI::OS2::Vertex os2_vertex;

                //TODO: move this to compute shader
                if (vertex2.weight == 0.f)
                {
                    Skeleton::Bone& bone = skeleton->bones[vertex1.bone_index];
                    os2_vertex.pos = bone.rot * mirrorVec(vertex1.pos, vertex1.mirror_axis);
                    os2_vertex.pos += bone.trans;
                    os2_vertex.norm = (bone.rot * vertex1.norm);
                    os2_vertex.norm = glm::normalize(os2_vertex.norm);
                }
                else
                {
                    Skeleton::Bone& bone1 = skeleton->bones[vertex1.bone_index];
                    Skeleton::Bone& bone2 = skeleton->bones[vertex2.bone_index];

                    glm::vec3 pos1 = bone1.rot * mirrorVec(vertex1.pos, vertex1.mirror_axis) + (bone1.trans * vertex1.weight);
                    glm::vec3 norm1 = bone1.rot * vertex1.norm * vertex1.weight;
                    norm1 = glm::normalize(norm1);

                    glm::vec3 pos2 = (bone2.rot * mirrorVec(vertex2.pos, vertex2.mirror_axis)) + (bone2.trans * vertex2.weight);
                    glm::vec3 norm2 = (bone2.rot * vertex2.norm) * vertex2.weight;
                    norm2 = glm::normalize(norm2);

                    os2_vertex.pos = pos1 + pos2;
                    os2_vertex.norm = glm::normalize(norm1 + norm2);
                }
                os2_vertex.uv = vertex1.uv;
                vertices.push_back(os2_vertex);
            }
            thread->engine->renderer.device->unmapMemory(mesh->vertex_buffer->memory);

            for (uint32_t image = 0; image < thread->engine->renderer.getImageCount(); ++image)
            {
                vertex_buffer[i].push_back(thread->engine->renderer.memory_manager->GetBuffer(vertices.size() * sizeof(FFXI::OS2::Vertex),
                    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal));

                staging_buffers.push_back(thread->engine->renderer.memory_manager->GetBuffer(vertices.size() * sizeof(FFXI::OS2::Vertex), vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
                uint8_t* staging_buffer_data = static_cast<uint8_t*>(thread->engine->renderer.device->mapMemory(staging_buffers.back()->memory, staging_buffers.back()->memory_offset, vertices.size() * sizeof(FFXI::OS2::Vertex), {}, thread->engine->renderer.dispatch));
                memcpy(staging_buffer_data, vertices.data(), vertices.size() * sizeof(FFXI::OS2::Vertex));
                thread->engine->renderer.device->unmapMemory(staging_buffers.back()->memory);

                vk::BufferMemoryBarrier barrier;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = vertex_buffer[i].back()->buffer;
                barrier.size = VK_WHOLE_SIZE;
                barrier.srcAccessMask = {};
                barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

                command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, barrier, nullptr, thread->engine->renderer.dispatch);

                vk::BufferCopy copy_region;
                copy_region.srcOffset = 0;
                copy_region.size = vertices.size() * sizeof(FFXI::OS2::Vertex);
                command_buffer.copyBuffer(staging_buffers.back()->buffer, vertex_buffer[i].back()->buffer, copy_region, thread->engine->renderer.dispatch);

                if (thread->engine->renderer.RTXEnabled())
                {
                    auto& geo = raytrace_geometry[image].emplace_back();
                    geo.geometryType = vk::GeometryTypeNV::eTriangles;
                    geo.geometry.triangles.vertexData = vertex_buffer[i].back()->buffer;
                    geo.geometry.triangles.vertexOffset = 0;
                    geo.geometry.triangles.vertexCount = vertices.size();
                    geo.geometry.triangles.vertexStride = sizeof(FFXI::OS2::Vertex);
                    geo.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;

                    geo.geometry.triangles.indexData = mesh->index_buffer->buffer;
                    geo.geometry.triangles.indexOffset = 0;
                    geo.geometry.triangles.indexCount = mesh->getIndexCount();
                    geo.geometry.triangles.indexType = vk::IndexType::eUint16;
                    if (!mesh->has_transparency)
                    {
                        geo.flags = vk::GeometryFlagBitsNV::eOpaque;
                    }
                }
            }
        }
        if (thread->engine->renderer.RTXEnabled())
        {
            vk::MemoryBarrier barrier;
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteNV | vk::AccessFlagBits::eAccelerationStructureReadNV;
            command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, {}, barrier, nullptr, nullptr, thread->engine->renderer.dispatch);
            for (size_t i = 0; i < thread->engine->renderer.getImageCount(); ++i)
            {
                animation_component->acceleration_structures.back().bottom_level_as.push_back(std::make_unique<BottomLevelAccelerationStructure>(thread->engine, command_buffer, raytrace_geometry[i], true, model.lifetime == Lifetime::Long, BottomLevelAccelerationStructure::Performance::FastBuild));
            }
        }
    }

    glm::vec3 RenderableEntityInitTask::mirrorVec(glm::vec3 pos, uint8_t mirror_axis)
    {
        glm::vec3 ret = pos;
        if (mirror_axis == 1)
        {
            ret.x = -ret.x;
        }
        if (mirror_axis == 2)
        {
            ret.y = -ret.y;
        }
        if (mirror_axis == 3)
        {
            ret.z = -ret.z;
        }
        return ret;
    }
}
