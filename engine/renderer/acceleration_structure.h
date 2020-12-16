#pragma once
#include <unordered_map>
#include <glm/glm.hpp>
#include "memory.h"
#include "engine/types.h"
#include "engine/worker_task.h"

namespace lotus
{
    class DeformableEntity;
    class Model;
    class Particle;
    class RendererRaytraceBase;

    class AccelerationStructure
    {
    protected:
        AccelerationStructure(RendererRaytraceBase* _renderer, vk::AccelerationStructureTypeKHR _type) : renderer(_renderer), type(_type) {}

        void CreateAccelerationStructure(const std::vector<vk::AccelerationStructureGeometryKHR>& geometries, std::vector<uint32_t>& max_primitive_counts);
        void BuildAccelerationStructure(vk::CommandBuffer command_buffer, const std::vector<vk::AccelerationStructureGeometryKHR>& geometries,
            const std::vector<vk::AccelerationStructureBuildRangeInfoKHR>& ranges, vk::BuildAccelerationStructureModeKHR mode);
        void Copy(vk::CommandBuffer command_buffer, AccelerationStructure& target);

        const vk::AccelerationStructureTypeKHR type;
        vk::BuildAccelerationStructureFlagsKHR flags;

        RendererRaytraceBase* renderer;
    public:
        void UpdateAccelerationStructure(vk::CommandBuffer command_buffer, const std::vector<vk::AccelerationStructureGeometryKHR>& geometry,
            const std::vector<vk::AccelerationStructureBuildRangeInfoKHR>& ranges);
        vk::UniqueHandle<vk::AccelerationStructureKHR, vk::DispatchLoaderDynamic> acceleration_structure;
        std::unique_ptr<Buffer> scratch_memory;
        std::unique_ptr<Buffer> object_memory;
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
        BottomLevelAccelerationStructure(RendererRaytraceBase* _renderer, vk::CommandBuffer command_buffer, std::vector<vk::AccelerationStructureGeometryKHR>&& geometry,
            std::vector<vk::AccelerationStructureBuildRangeInfoKHR>&& geometry_ranges, std::vector<uint32_t>&& max_primitive_counts, bool updateable, bool compact, Performance performance);
        void Update(vk::CommandBuffer buffer);
        uint32_t instanceid{ 0 };
    private:
        std::vector<vk::AccelerationStructureGeometryKHR> geometries;
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> geometry_ranges;
        std::vector<uint32_t> max_primitive_counts;
    };

    class TopLevelAccelerationStructure : public AccelerationStructure
    {
    public:
        TopLevelAccelerationStructure(RendererRaytraceBase* _renderer, bool updateable);

        uint32_t AddInstance(vk::AccelerationStructureInstanceKHR instance);
        WorkerTask<> Build(Engine*);
        void UpdateInstance(uint32_t instance_id, glm::mat3x4 instance);

    private:
        std::vector<vk::AccelerationStructureInstanceKHR> instances;
        std::unique_ptr<Buffer> instance_memory;
        bool updateable{ false };
        bool dirty{ false };
    };
}
