#pragma once
#include "memory.h"
#include "engine/types.h"
#include <glm/glm.hpp>
#include <unordered_map>

namespace lotus
{
    class DeformableEntity;
    class Model;
    class Particle;
    class Engine;

    class AccelerationStructure
    {
    protected:
        AccelerationStructure(Engine* _engine, vk::AccelerationStructureTypeKHR _type) : engine(_engine), type(_type) {}

        void PopulateAccelerationStructure(const std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR>& geometries);
        void PopulateBuffers();
        void BuildAccelerationStructure(vk::CommandBuffer command_buffer, const std::vector<vk::AccelerationStructureGeometryKHR>& geometries,
            const std::vector<vk::AccelerationStructureBuildOffsetInfoKHR>& offsets, bool update);
        void Copy(vk::CommandBuffer command_buffer, AccelerationStructure& target);

        const vk::AccelerationStructureTypeKHR type;
        vk::BuildAccelerationStructureFlagsKHR flags;

        Engine* engine;
    public:
        void UpdateAccelerationStructure(vk::CommandBuffer command_buffer, const std::vector<vk::AccelerationStructureGeometryKHR>& geometry,
            const std::vector<vk::AccelerationStructureBuildOffsetInfoKHR>& offsets);
        vk::UniqueHandle<vk::AccelerationStructureKHR, vk::DispatchLoaderDynamic> acceleration_structure;
        std::unique_ptr<Buffer> scratch_memory;
        std::unique_ptr<GenericMemory> object_memory;
        uint64_t handle {0};
    };

    class BottomLevelAccelerationStructure : public AccelerationStructure
    {
    public:
        enum class Performance
        {
            FastTrace,
            FastBuild
        };
        BottomLevelAccelerationStructure(Engine* _engine, vk::CommandBuffer command_buffer, std::vector<vk::AccelerationStructureGeometryKHR>&& geometry,
            std::vector<vk::AccelerationStructureBuildOffsetInfoKHR>&& geometry_offsets, std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR>&& geometry_infos,
            bool updateable, bool compact, Performance performance);
        void Update(vk::CommandBuffer buffer);
        uint16_t resource_index{ 0 };
        uint32_t instanceid{ 0 };
    private:
        std::vector<vk::AccelerationStructureGeometryKHR> geometries;
        std::vector<vk::AccelerationStructureBuildOffsetInfoKHR> geometry_offsets;
        std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR> geometry_infos;
    };

    class TopLevelAccelerationStructure : public AccelerationStructure
    {
    public:
        TopLevelAccelerationStructure(Engine* _engine, bool updateable);

        uint32_t AddInstance(vk::AccelerationStructureInstanceKHR instance);
        void Build(vk::CommandBuffer command_buffer);
        void UpdateInstance(uint32_t instance_id, glm::mat3x4 instance);
        void AddBLASResource(Model* model);
        void AddBLASResource(DeformableEntity* entity);
        void AddBLASResource(Particle* entity);
        std::vector<vk::DescriptorBufferInfo> descriptor_vertex_info;
        std::vector<vk::DescriptorBufferInfo> descriptor_index_info;
        std::vector<vk::DescriptorImageInfo> descriptor_texture_info;
    private:
        std::vector<vk::AccelerationStructureInstanceKHR> instances;
        std::unique_ptr<Buffer> instance_memory;
        bool updateable{ false };
        bool dirty{ false };
    };
}
