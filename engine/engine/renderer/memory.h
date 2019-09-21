#pragma once
#include <utility>
#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"

namespace lotus
{
    class MemoryManager;

    class Memory
    {
    public:
        Memory(MemoryManager* _manager, VmaAllocation _allocation, VmaAllocationInfo _alloc_info) :
            allocation(_allocation),
            memory_offset(_alloc_info.offset),
            memory(_alloc_info.deviceMemory),
            manager(_manager) {}
        Memory(const Memory&) = delete;
        Memory(Memory&&) = default;
        Memory& operator=(const Memory&) = delete;
        Memory& operator=(Memory&&) = default;
        virtual ~Memory() = default;
        VmaAllocation allocation;
        vk::DeviceSize memory_offset;
        vk::DeviceMemory memory;
    protected:
        MemoryManager* manager{ nullptr };
    };

    class Buffer : public Memory
    {
    public:
        Buffer(MemoryManager* _manager, vk::Buffer _buffer, VmaAllocation _allocation, VmaAllocationInfo _alloc_info) :
            Memory(_manager, _allocation, _alloc_info),
            buffer(_buffer) {}
        ~Buffer();
        vk::Buffer buffer;
    };

    class Image : public Memory
    {
    public:
        Image(MemoryManager* _manager, vk::Image _image,  VmaAllocation _allocation, VmaAllocationInfo _alloc_info) :
            Memory(_manager, _allocation, _alloc_info),
            image(_image) {}
        ~Image();
        vk::Image image;
    };

    class GenericMemory : public Memory
    {
    public:
        GenericMemory(MemoryManager* _manager, VmaAllocation _allocation, VmaAllocationInfo _alloc_info) :
            Memory(_manager, _allocation, _alloc_info) {}
        ~GenericMemory();
    };

    class MemoryManager
    {
    public:
        MemoryManager(vk::PhysicalDevice _physical_device, vk::Device _device, vk::DispatchLoaderDynamic _dispatch);
        std::unique_ptr<Buffer> GetBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryflags);
        std::unique_ptr<Image> GetImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags memoryflags, uint32_t arrayLayers = 1);
        std::unique_ptr<GenericMemory> GetMemory(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags memoryflags);

        VmaAllocator allocator;

    private:
        vk::Device device;
        vk::PhysicalDevice physical_device;
        vk::DispatchLoaderDynamic dispatch;
    };
}
