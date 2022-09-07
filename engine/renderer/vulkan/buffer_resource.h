#pragma once
#include <vector>
#include "vulkan_inc.h"
#include "engine/renderer/memory.h"
#include "engine/shared_linked_list.h"

namespace lotus
{
    template<typename T, uint32_t Binding>
    class BufferResource
    {
    public:
        class View
        {
        public:
            View(BufferResource* _resource, uint32_t _count) : resource(_resource), count(_count), index(_resource->get_index(count)), buffer_view(resource->buffer_mapped + index, count)
            {
            }
            ~View() { resource->free_index(index, count); }
        private:
            BufferResource* resource;
            uint32_t count;
        public:
            const uint32_t index;
            const std::span<T> buffer_view;
        };

        BufferResource(MemoryManager* memory_manager, vk::Device device, vk::DescriptorSet _set)
        {
            free_indices.resize(max_index_size);
            constexpr auto size = this->max_resource_index * sizeof(T);
            this->buffer = memory_manager->GetBuffer(size, vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostVisible);
            this->buffer_mapped = (T*)this->buffer->map(0, size, {});

            vk::DescriptorBufferInfo write_buffer
            {
                .buffer = this->buffer->buffer,
                .offset = 0,
                .range = size
            };
            vk::WriteDescriptorSet write
            {
                .dstSet = _set,
                .dstBinding = Binding,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = Type,
                .pBufferInfo = &write_buffer
            };
            device.updateDescriptorSets(write, nullptr);
        }
        ~BufferResource()
        {
            if (this->buffer_mapped) this->buffer->unmap();
        }

        std::unique_ptr<View> get(uint32_t count)
        {
            return std::make_unique<View>(this, count);
        }

        static constexpr uint32_t Binding = Binding;
        static constexpr vk::DescriptorType Type = vk::DescriptorType::eStorageBuffer;

    private:
        // ???
        static constexpr uint16_t max_resource_index{ 4096 };
        static constexpr uint32_t max_index_size = 16;

        std::unique_ptr<Buffer> buffer;
        T* buffer_mapped{ nullptr };

        uint32_t get_index(uint32_t count)
        {
            if (count > free_indices.size())
            {
                throw std::invalid_argument("max_index_size must be increased for this resource");
            }
            auto index = free_indices[count].get();
            if (!index)
                index = max_index.fetch_add(count);
            return *index;
        }

        void free_index(uint32_t index, uint32_t count)
        {
            free_indices[count].queue(index);
        }

        std::atomic<uint32_t> max_index{ 0 };
        std::vector<SharedLinkedList<uint32_t>> free_indices;
    };
}
