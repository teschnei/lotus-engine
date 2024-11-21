#include "global_descriptors.h"
#include "lotus/renderer/vulkan/gpu.h"
#include "lotus/renderer/vulkan/renderer.h"

namespace lotus
{
GlobalDescriptors::GlobalDescriptors(Renderer* _renderer)
    : renderer(_renderer), layout(initializeResourceDescriptorSetLayout(renderer)), pool(initializeResourceDescriptorPool(renderer, *layout)),
      set(initializeResourceDescriptorSet(renderer, *layout, *pool)), mesh_info(renderer->gpu->memory_manager.get(), *renderer->gpu->device, *set),
      texture(*set)
{
}

vk::UniqueDescriptorSetLayout GlobalDescriptors::initializeResourceDescriptorSetLayout(Renderer* renderer)
{
    std::array descriptors{vk::DescriptorSetLayoutBinding{
                               // mesh info
                               .binding = decltype(mesh_info)::Binding,
                               .descriptorType = decltype(mesh_info)::Type,
                               .descriptorCount = 1,
                               .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR |
                                             vk::ShaderStageFlagBits::eAnyHitKHR | vk::ShaderStageFlagBits::eIntersectionKHR |
                                             vk::ShaderStageFlagBits::eFragment,
                           },
                           vk::DescriptorSetLayoutBinding{
                               // texture
                               .binding = decltype(texture)::Binding,
                               .descriptorType = decltype(texture)::Type,
                               .descriptorCount = max_descriptor_index,
                               .stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR | vk::ShaderStageFlagBits::eFragment,
                           }};

    std::vector<vk::DescriptorBindingFlags> binding_flags{{}, vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind};
    vk::DescriptorSetLayoutBindingFlagsCreateInfo layout_flags{.bindingCount = static_cast<uint32_t>(binding_flags.size()),
                                                               .pBindingFlags = binding_flags.data()};

    return renderer->gpu->device->createDescriptorSetLayoutUnique({.pNext = &layout_flags,
                                                                   .flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
                                                                   .bindingCount = descriptors.size(),
                                                                   .pBindings = descriptors.data()});
}

vk::UniqueDescriptorPool GlobalDescriptors::initializeResourceDescriptorPool(Renderer* renderer, vk::DescriptorSetLayout layout)
{
    std::array pool_sizes{vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1},
                          vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, max_descriptor_index}};

    return renderer->gpu->device->createDescriptorPoolUnique({
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
        .maxSets = 1,
        .poolSizeCount = pool_sizes.size(),
        .pPoolSizes = pool_sizes.data(),
    });
}

vk::UniqueDescriptorSet GlobalDescriptors::initializeResourceDescriptorSet(Renderer* renderer, vk::DescriptorSetLayout layout, vk::DescriptorPool pool)
{
    auto sets = renderer->gpu->device->allocateDescriptorSetsUnique({.descriptorPool = pool, .descriptorSetCount = 1, .pSetLayouts = &layout});
    return std::move(sets[0]);
}

[[nodiscard]]
std::unique_ptr<GlobalDescriptors::TextureDescriptor::Index> GlobalDescriptors::getTextureIndex()
{
    return texture.get();
}

[[nodiscard]]
std::unique_ptr<GlobalDescriptors::MeshInfoBuffer::View> GlobalDescriptors::getMeshInfoBuffer(uint32_t count)
{
    return mesh_info.get(count);
}

void GlobalDescriptors::updateDescriptorSet() { texture.updateDescriptorSet(*renderer->gpu->device); }
} // namespace lotus
