#pragma once
#include <utility>
#include <vulkan/vulkan.hpp>
#include <unordered_map>
#include <set>

namespace lotus
{
    class MemoryManager;

    struct memory_allocation
    {
        vk::DeviceSize offset;
        vk::DeviceSize size;

        bool operator<(const memory_allocation& o) const { return this->offset < o.offset; }
    };

    struct memory_info
    {
        vk::UniqueHandle<vk::DeviceMemory, vk::DispatchLoaderDynamic> memory;
        vk::DeviceSize size;
        vk::DeviceSize begin_a;
        vk::DeviceSize begin_b;
        std::set<memory_allocation> allocations;
    };

    class Memory
    {
    public:
        Memory(MemoryManager* _manager, vk::DeviceMemory _memory, vk::DeviceSize _offset, uint32_t _memoryType) :
            memory(_memory),
            memory_offset(_offset),
            memoryType(_memoryType),
            manager(_manager) {}
        Memory(const Memory&) = delete;
        Memory(Memory&&) = default;
        Memory& operator=(const Memory&) = delete;
        Memory& operator=(Memory&&) = default;
        virtual ~Memory();
        vk::DeviceMemory memory;
        vk::DeviceSize memory_offset;
        uint32_t memoryType;
    private:
        MemoryManager* manager{ nullptr };
    };

    class Buffer : public Memory
    {
    public:
        Buffer(MemoryManager* _manager, vk::UniqueHandle<vk::Buffer, vk::DispatchLoaderDynamic>&& _buffer, vk::DeviceMemory _memory, vk::DeviceSize _offset, uint32_t _memoryType) :
            Memory(_manager, _memory, _offset, _memoryType),
            buffer(std::move(_buffer)) {}
        vk::UniqueHandle<vk::Buffer, vk::DispatchLoaderDynamic> buffer;
    };

    class Image : public Memory
    {
    public:
        Image(MemoryManager* _manager, vk::UniqueHandle<vk::Image, vk::DispatchLoaderDynamic>&& _image, vk::DeviceMemory _memory, vk::DeviceSize _offset, uint32_t _memoryType) :
            Memory(_manager, _memory, _offset, _memoryType),
            image(std::move(_image)) {}
        vk::UniqueHandle<vk::Image, vk::DispatchLoaderDynamic> image;
    };

    class MemoryManager
    {
    public:
        MemoryManager(vk::PhysicalDevice _physical_device, vk::Device _device, vk::DispatchLoaderDynamic _dispatch) : device(_device), physical_device(_physical_device), dispatch(_dispatch) {}
        std::unique_ptr<Buffer> GetBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryflags);
        std::unique_ptr<Image> GetImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags memoryflags, vk::DeviceSize arrayLayers = 1);
        void return_memory(Memory* memory);

    private:
        std::pair<vk::DeviceMemory, vk::DeviceSize> getMemory(vk::DeviceSize size, vk::DeviceSize alignment, uint32_t memorytype);
        uint32_t findMemoryType(uint32_t, vk::MemoryPropertyFlags) const;
        vk::DeviceSize getAllocationSize(vk::DeviceSize required);
        std::unordered_map<uint32_t, memory_info> memory;
        vk::Device device;
        vk::PhysicalDevice physical_device;
        vk::DispatchLoaderDynamic dispatch;
    };
}
