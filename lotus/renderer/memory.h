#pragma once
#include "vk_mem_alloc.h"
#include <memory>
#include <mutex>
#include <utility>

import vulkan_hpp;

namespace lotus
{
class MemoryManager;

class Memory
{
public:
    Memory(MemoryManager* _manager, VmaAllocation _allocation, VmaAllocationInfo _alloc_info, vk::DeviceSize _size)
        : allocation(_allocation), memory_offset(_alloc_info.offset), memory(_alloc_info.deviceMemory), size(_size), manager(_manager)
    {
    }
    Memory(const Memory&) = delete;
    Memory(Memory&&) = default;
    Memory& operator=(const Memory&) = delete;
    Memory& operator=(Memory&&) = default;
    virtual ~Memory() = default;
    void* map(vk::DeviceSize offset, vk::DeviceSize size, vk::MemoryMapFlags flags);
    void unmap();
    void flush(vk::DeviceSize offset, vk::DeviceSize size);
    vk::DeviceSize getSize() { return size; }
    VmaAllocation allocation;

protected:
    vk::DeviceSize memory_offset;
    vk::DeviceMemory memory;
    vk::DeviceSize size;
    MemoryManager* manager{nullptr};
};

class Buffer : public Memory
{
public:
    Buffer(MemoryManager* _manager, vk::Buffer _buffer, VmaAllocation _allocation, VmaAllocationInfo _alloc_info, vk::DeviceSize _size)
        : Memory(_manager, _allocation, _alloc_info, _size), buffer(_buffer)
    {
    }
    ~Buffer();
    vk::Buffer buffer;
};

class Image : public Memory
{
public:
    Image(MemoryManager* _manager, vk::Image _image, VmaAllocation _allocation, VmaAllocationInfo _alloc_info, vk::DeviceSize _size)
        : Memory(_manager, _allocation, _alloc_info, _size), image(_image)
    {
    }
    ~Image();
    vk::Image image;
};

class GenericMemory : public Memory
{
public:
    GenericMemory(MemoryManager* _manager, VmaAllocation _allocation, VmaAllocationInfo _alloc_info, vk::DeviceSize _size)
        : Memory(_manager, _allocation, _alloc_info, _size)
    {
    }
    ~GenericMemory();

    vk::DeviceMemory get_memory() const { return memory; }
    vk::DeviceSize get_memory_offset() const { return memory_offset; }
};

class MemoryManager
{
public:
    MemoryManager(vk::PhysicalDevice _physical_device, vk::Device _device, vk::Instance _instance);
    ~MemoryManager();
    std::unique_ptr<Buffer> GetBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryflags);
    std::unique_ptr<Buffer> GetAlignedBuffer(vk::DeviceSize size, vk::DeviceSize alignment, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryflags);
    std::unique_ptr<Image> GetImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                                    vk::MemoryPropertyFlags memoryflags, uint32_t arrayLayers = 1);
    std::unique_ptr<GenericMemory> GetMemory(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags memoryflags,
                                             vk::MemoryAllocateFlags allocateflags = vk::MemoryAllocateFlagBits{});

    VmaAllocator allocator;

private:
    vk::Device device;
    vk::PhysicalDevice physical_device;
    vk::Instance instance;
    std::mutex allocation_mutex;

    friend class Memory;
};
} // namespace lotus
