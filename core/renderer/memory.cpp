#include "memory.h"

namespace lotus
{
    uint32_t MemoryManager::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
        vk::PhysicalDeviceMemoryProperties memProperties = physical_device.getMemoryProperties();

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    vk::DeviceSize MemoryManager::getAllocationSize(vk::DeviceSize required)
    {
        return 1024 * 1024 * 1024;
        if (required < 65536*2)
        {
            return 65536*2;
        }
        for (int i = (sizeof(required) * 8) - 1; i > 0; --i)
        {
            if ((1ull << i & required) == required)
            {
                return required;
            }
            else if ((1ull << i & required) > 0 && i < (sizeof(required)*8) - 1)
            {
                return 1ull << (i + 1);
            }
        }
        throw std::exception("requested memory block too big");
    }

    Memory::~Memory()
    {
        if (manager)
            manager->return_memory(this);
    }

    std::unique_ptr<Buffer> MemoryManager::GetBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags memoryflags)
    {
        vk::BufferCreateInfo buffer_create_info;
        buffer_create_info.size = size;
        buffer_create_info.usage = usage;
        buffer_create_info.sharingMode = vk::SharingMode::eExclusive;

        auto buffer = device.createBufferUnique(buffer_create_info, nullptr, dispatch);

        auto requirements = device.getBufferMemoryRequirements(*buffer, dispatch);
        auto memorytype = findMemoryType(requirements.memoryTypeBits, memoryflags);
       
        auto [memory, offset] = getMemory(requirements.size, requirements.alignment, memorytype);
       
        device.bindBufferMemory(*buffer, memory, offset, dispatch);

        return std::make_unique<Buffer>(this, std::move(buffer), memory, offset, memorytype);
    }

    std::unique_ptr<Image> MemoryManager::GetImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
        vk::MemoryPropertyFlags memoryflags)
    {
        vk::ImageCreateInfo image_info = {};
        image_info.imageType = vk::ImageType::e2D;
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = format;
        image_info.tiling = tiling;
        image_info.initialLayout = vk::ImageLayout::eUndefined;;
        image_info.usage = usage;
        image_info.samples = vk::SampleCountFlagBits::e1;
        image_info.sharingMode = vk::SharingMode::eExclusive;

        auto image = device.createImageUnique(image_info, nullptr, dispatch);

        auto requirements = device.getImageMemoryRequirements(*image, dispatch);
        auto memorytype = findMemoryType(requirements.memoryTypeBits, memoryflags);

        auto [memory, offset] = getMemory(requirements.size, requirements.alignment, memorytype);

        device.bindImageMemory(*image, memory, offset, dispatch);

        return std::make_unique<Image>(this, std::move(image), memory, offset, memorytype);
    }

    void MemoryManager::return_memory(Memory* mem)
    {
        if (auto mem_iter = memory.find(mem->memoryType); mem_iter != memory.end())
        {
            //TODO: condense this into one loop iteration
            mem_iter->second.allocations.erase(std::find_if(mem_iter->second.allocations.begin(), mem_iter->second.allocations.end(), [&mem](const auto& ele) {
                return ele.offset == mem->memory_offset;
                }));
            if (mem_iter->second.allocations.empty())
            {
                mem_iter->second.begin_a = 0;
                mem_iter->second.begin_b = 0;
            }
            //if all remaining are less than the erased one, swap a and b values to restart the circle
            else if (std::all_of(mem_iter->second.allocations.begin(), mem_iter->second.allocations.end(), [&mem](const auto& allocation) {
                return mem->memory_offset > allocation.offset;
                }))
            {
                std::swap(mem_iter->second.begin_a, mem_iter->second.begin_b);
            }
        }
        else
        {
            throw std::exception("could not find memory block for memory");
        }
    }

    std::pair<vk::DeviceMemory, vk::DeviceSize> MemoryManager::getMemory(vk::DeviceSize size, vk::DeviceSize alignment, uint32_t memorytype)
    {
        auto mem = memory.find(static_cast<uint32_t>(memorytype));
        if (mem != memory.end())
        {
            bool bound = false;;
            auto next_alloc = mem->second.allocations.upper_bound({ mem->second.begin_a, 0 });
            size_t space = (next_alloc != mem->second.allocations.end() ? next_alloc->offset : mem->second.size) - mem->second.begin_a;
            void* new_start = (void*)mem->second.begin_a;
            if (std::align(alignment, size, new_start, space) || new_start == nullptr && size < space)
            {
                mem->second.begin_a = (vk::DeviceSize)new_start + size;
                bound = true;
            }
            if (!bound)
            {
                next_alloc = mem->second.allocations.upper_bound({ mem->second.begin_b, 0 });
                space = (next_alloc != mem->second.allocations.end() ? next_alloc->offset : mem->second.size) - mem->second.begin_b;
                new_start = (void*)mem->second.begin_b;
                if (std::align(alignment, size, new_start, space) || new_start == nullptr && size < space)
                {
                    mem->second.begin_b = (vk::DeviceSize)new_start + size;
                    bound = true;
                }
            }
            if (bound)
            {
                mem->second.allocations.insert(memory_allocation{ (vk::DeviceSize)new_start, size });
                return { *mem->second.memory, (vk::DeviceSize)new_start };
            }
            throw std::exception("out of memory in existing buffer");
            //TODO: not enough space in existing buffer - resize?
        }

        vk::MemoryAllocateInfo allocInfo = {};
        allocInfo.allocationSize = getAllocationSize(size);
        allocInfo.memoryTypeIndex = memorytype;

        auto new_mem = device.allocateMemoryUnique(allocInfo, nullptr, dispatch);

        mem = memory.emplace(static_cast<uint32_t>(memorytype), memory_info{ std::move(new_mem), allocInfo.allocationSize, size, 0 }).first;

        mem->second.allocations.insert(memory_allocation{ 0, size });
        return { *mem->second.memory, 0 };
    }
}
