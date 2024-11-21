#pragma once
#include "lotus/types.h"
#include "lotus/worker_task.h"
#include "memory.h"
#include <condition_variable>
#include <mutex>
#include <span>

import glm;

namespace lotus
{
class Model;
class Renderer;

class AccelerationStructure
{
protected:
    AccelerationStructure(Renderer* _renderer, vk::AccelerationStructureTypeKHR _type) : renderer(_renderer), type(_type) {}

    void CreateAccelerationStructure(std::span<vk::AccelerationStructureGeometryKHR> geometries, std::span<uint32_t> max_primitive_counts);
    void BuildAccelerationStructure(vk::CommandBuffer command_buffer, std::span<vk::AccelerationStructureGeometryKHR> geometries,
                                    std::span<vk::AccelerationStructureBuildRangeInfoKHR> ranges, vk::BuildAccelerationStructureModeKHR mode);
    void Copy(vk::CommandBuffer command_buffer, AccelerationStructure& target);

    const vk::AccelerationStructureTypeKHR type;
    vk::BuildAccelerationStructureFlagsKHR flags;

    Renderer* renderer;

public:
    void UpdateAccelerationStructure(vk::CommandBuffer command_buffer, std::span<vk::AccelerationStructureGeometryKHR> geometry,
                                     std::span<vk::AccelerationStructureBuildRangeInfoKHR> ranges);
    vk::UniqueHandle<vk::AccelerationStructureKHR, vk::DispatchLoaderDynamic> acceleration_structure;
    std::unique_ptr<Buffer> scratch_memory;
    std::unique_ptr<Buffer> object_memory;
    uint64_t handle{0};
};

class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
    enum class Performance
    {
        FastTrace,
        FastBuild
    };
    BottomLevelAccelerationStructure(Renderer* _renderer, vk::CommandBuffer command_buffer, std::vector<vk::AccelerationStructureGeometryKHR>&& geometry,
                                     std::vector<vk::AccelerationStructureBuildRangeInfoKHR>&& geometry_ranges, std::vector<uint32_t>&& max_primitive_counts,
                                     bool updateable, bool compact, Performance performance);
    void Update(vk::CommandBuffer buffer);
    uint32_t instanceid{0};

private:
    std::vector<vk::AccelerationStructureGeometryKHR> geometries;
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR> geometry_ranges;
    std::vector<uint32_t> max_primitive_counts;
};

class TopLevelAccelerationStructure : public AccelerationStructure
{
public:
    class TopLevelAccelerationStructureInstances
    {
    public:
        TopLevelAccelerationStructureInstances() { instances.resize(size); }
        auto GetData() { return instances.data(); }
        void SetInstance(vk::AccelerationStructureInstanceKHR instance, size_t index);

    private:
        std::vector<vk::AccelerationStructureInstanceKHR> instances;
        size_t size{1024};
        void ReallocateInstances();
        // TODO: there must be a better mechanism for this... latch/barrier would be the best option if they worked
        std::atomic_flag reallocating;
        std::mutex reallocating_mutex;
        std::condition_variable reallocating_cv;
    };
    TopLevelAccelerationStructure(Renderer* _renderer, TopLevelAccelerationStructureInstances& instances, bool updateable);

    uint32_t AddInstance(vk::AccelerationStructureInstanceKHR instance);
    WorkerTask<> Build(Engine*);

private:
    TopLevelAccelerationStructureInstances& instances;
    std::atomic<uint32_t> instance_index{0};
    std::unique_ptr<Buffer> instance_memory;
    bool updateable{false};
};

} // namespace lotus
