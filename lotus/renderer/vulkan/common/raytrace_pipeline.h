#pragma once

#include "lotus/renderer/acceleration_structure.h"
#include "lotus/renderer/memory.h"

namespace lotus
{
class Renderer;

enum class RaytracePipelineType
{
    Hybrid,
    Full
};

class RaytracePipeline
{
public:
    RaytracePipeline(Renderer* _renderer, std::string raygen, std::span<vk::DescriptorSetLayoutBinding> input_output_descriptor_layout);

    static constexpr uint32_t shaders_per_group{1};

    void prepareNextFrame();
    Task<> prepareFrame(Engine* engine);

    TopLevelAccelerationStructure* getTLAS(uint32_t image) const;

    vk::UniqueCommandBuffer getCommandBuffer(std::span<vk::WriteDescriptorSet> input_output_descriptors, std::span<vk::ImageMemoryBarrier2KHR> before_barriers,
                                             std::span<vk::ImageMemoryBarrier2KHR> after_barriers);

private:
    Renderer* renderer;

    vk::UniqueDescriptorSetLayout resources_descriptor_layout;
    vk::UniqueDescriptorPool resources_descriptor_pool;
    std::vector<vk::UniqueDescriptorSet> resources_descriptor_sets;

    vk::UniqueDescriptorSetLayout input_output_descriptor_layout;

    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline pipeline;

    std::vector<std::unique_ptr<TopLevelAccelerationStructure>> tlas;
    TopLevelAccelerationStructure::TopLevelAccelerationStructureInstances tlas_instances;

    struct SBT
    {
        std::unique_ptr<Buffer> buffer;
        vk::StridedDeviceAddressRegionKHR raygen;
        vk::StridedDeviceAddressRegionKHR miss;
        vk::StridedDeviceAddressRegionKHR hit;
    } shader_binding_table;

    static vk::UniqueDescriptorSetLayout initializeResourceDescriptorSetLayout(Renderer* renderer);
    static vk::UniqueDescriptorPool initializeResourceDescriptorPool(Renderer* renderer, vk::DescriptorSetLayout layout);
    static std::vector<vk::UniqueDescriptorSet> initializeResourceDescriptorSets(Renderer* renderer, vk::DescriptorSetLayout layout, vk::DescriptorPool pool);
    static vk::UniqueDescriptorSetLayout initializeInputOutputDescriptorSetLayout(Renderer* renderer,
                                                                                  std::span<vk::DescriptorSetLayoutBinding> input_output_descriptor_layout);
    static vk::UniquePipelineLayout initializePipelineLayout(Renderer* renderer, std::span<vk::DescriptorSetLayout> descriptor_layouts);
    static vk::UniquePipeline initializePipeline(Renderer* renderer, vk::PipelineLayout pipeline_layout, std::string raygen);
    static SBT initializeSBT(Renderer* renderer, vk::Pipeline pipeline);
};
} // namespace lotus