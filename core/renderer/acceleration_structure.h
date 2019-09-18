#pragma once
#include "memory.h"
#include <glm/glm.hpp>
#include <unordered_map>

namespace lotus
{
    class Model;
    class Engine;

    class AccelerationStructure
    {
    protected:
        AccelerationStructure(Engine* _engine) : engine(_engine) {}

        void PopulateAccelerationStructure(uint32_t instanceCount, uint32_t geometryCount, const vk::GeometryNV* pGeometry, bool updateable);
        void PopulateBuffers();
        void BuildAccelerationStructure(vk::CommandBuffer command_buffer, vk::Buffer instance_buffer, vk::DeviceSize instance_offset, bool update);

        Engine* engine;
        vk::AccelerationStructureInfoNV info;
    public:
        void UpdateAccelerationStructure(vk::CommandBuffer command_buffer, vk::Buffer instance_buffer, vk::DeviceSize instance_offset);
        vk::UniqueHandle<vk::AccelerationStructureNV, vk::DispatchLoaderDynamic> acceleration_structure;
        std::unique_ptr<Buffer> scratch_memory;
        std::unique_ptr<Memory> object_memory;
        uint64_t handle {0};
    };

    class BottomLevelAccelerationStructure : public AccelerationStructure
    {
    public:
        BottomLevelAccelerationStructure(Engine* _engine, vk::CommandBuffer command_buffer, const std::vector<vk::GeometryNV>& geometry, bool updateable);
        void Update(vk::CommandBuffer buffer);
        uint16_t resource_index{ 0 };
    };

    struct VkGeometryInstance
    {
        /// Transform matrix, containing only the top 3 rows
        glm::mat3x4 transform;
        /// Instance index
        uint32_t instanceId : 24;
        /// Visibility mask
        uint32_t mask : 8;
        /// Index of the hit group which will be invoked when a ray hits the instance
        uint32_t instanceOffset : 24;
        /// Instance flags, such as culling
        uint32_t flags : 8;
        /// Opaque handle of the bottom-level acceleration structure
        uint64_t accelerationStructureHandle;
    };

    static_assert(sizeof(VkGeometryInstance) == 64, "VkGeometryInstance structure compiles to incorrect size");

    class TopLevelAccelerationStructure : public AccelerationStructure
    {
    public:
        TopLevelAccelerationStructure(Engine* _engine, bool updateable);

        uint32_t AddInstance(VkGeometryInstance instance);
        void Build(vk::CommandBuffer command_buffer);
        void UpdateInstance(uint32_t instance_id, glm::mat3x4 instance);
        void AddBLASResource(Model* model);
        std::vector<vk::DescriptorBufferInfo> descriptor_vertex_info;
        std::vector<vk::DescriptorBufferInfo> descriptor_index_info;
        std::vector<vk::DescriptorImageInfo> descriptor_texture_info;
    private:
        std::vector<VkGeometryInstance> instances;
        std::unique_ptr<Buffer> instance_memory;
        bool updateable{ false };
        bool dirty{ false };
    };
}
