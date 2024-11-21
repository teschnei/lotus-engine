#pragma once
#include "lotus/shared_linked_list.h"
#include "vulkan_inc.h"
#include <atomic>
#include <vector>

namespace lotus
{
namespace
{
template <vk::DescriptorType T> struct DescriptorInfoType
{
    using type = vk::DescriptorBufferInfo;
};
template <> struct DescriptorInfoType<vk::DescriptorType::eCombinedImageSampler>
{
    using type = vk::DescriptorImageInfo;
};
} // namespace

template <vk::DescriptorType _Type, uint32_t _Binding> class DescriptorResource
{
public:
    DescriptorResource(vk::DescriptorSet _set) : set(_set) {}

    using DescriptorInfo = typename DescriptorInfoType<_Type>::type;

    class Index
    {
    public:
        Index(DescriptorResource* _resource, uint32_t _index) : resource(_resource), index(_index) {}
        ~Index() { resource->free_index(index); }

        void write(DescriptorInfo info) { resource->addWriteInfo(index, info); }

        const uint32_t index;

    private:
        DescriptorResource* resource;
    };

    auto get()
    {
        auto index = free_indices.get();
        if (!index)
            index = max_index.fetch_add(1);
        return std::make_unique<Index>(this, *index);
    }

    void updateDescriptorSet(vk::Device device)
    {
        auto w = writes.getAll();
        auto wi = std::move(write_info);

        if (!w.empty())
            device.updateDescriptorSets(w, nullptr);
    }

    static constexpr uint32_t Binding = _Binding;
    static constexpr vk::DescriptorType Type = _Type;

private:
    vk::DescriptorSet set;
    SharedLinkedList<vk::WriteDescriptorSet> writes;
    SharedLinkedList<DescriptorInfo> write_info;
    std::atomic<uint32_t> max_index{0};
    SharedLinkedList<uint32_t> free_indices;

    void addWriteInfo(uint32_t index, DescriptorInfo _info)
    {
        auto& info = write_info.queue(_info);

        vk::WriteDescriptorSet write{.dstSet = set, .dstBinding = Binding, .dstArrayElement = index, .descriptorCount = 1, .descriptorType = Type};
        if constexpr (std::same_as<DescriptorInfo, vk::DescriptorBufferInfo>)
        {
            write.pBufferInfo = &info;
        }
        else if constexpr (std::same_as<DescriptorInfo, vk::DescriptorImageInfo>)
        {
            write.pImageInfo = &info;
        }
        writes.queue(write);
    }

    void free_index(uint32_t index) { free_indices.queue(index); }
};
} // namespace lotus
