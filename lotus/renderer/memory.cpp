#include "memory.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

namespace lotus
{
void* Memory::map(vk::DeviceSize offset, vk::DeviceSize size, vk::MemoryMapFlags flags)
{
    void* mapped;
    vmaMapMemory(manager->allocator, allocation, &mapped);
    return static_cast<uint8_t*>(mapped) + offset;
}

void Memory::flush(vk::DeviceSize offset, vk::DeviceSize size) { vmaFlushAllocation(manager->allocator, allocation, offset, size); }

void Memory::unmap() { vmaUnmapMemory(manager->allocator, allocation); }

Buffer::~Buffer() { vmaDestroyBuffer(manager->allocator, buffer, allocation); }

Image::~Image() { vmaDestroyImage(manager->allocator, image, allocation); }

GenericMemory::~GenericMemory() { vmaFreeMemory(manager->allocator, allocation); }

MemoryManager::MemoryManager(vk::PhysicalDevice _physical_device, vk::Device _device, vk::Instance _instance)
    : device(_device), physical_device(_physical_device), instance(_instance), allocator(VK_NULL_HANDLE)
{
    VmaAllocatorCreateInfo vma_ci = {};
    vma_ci.device = device;
    vma_ci.physicalDevice = physical_device;
    vma_ci.instance = instance;
    vma_ci.vulkanApiVersion = VK_API_VERSION_1_2;
    vma_ci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    vmaCreateAllocator(&vma_ci, &allocator);
}

MemoryManager::~MemoryManager() { vmaDestroyAllocator(allocator); }

std::unique_ptr<Buffer> MemoryManager::GetBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryflags)
{
    std::scoped_lock lg(allocation_mutex);
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size;
    buffer_create_info.usage = (VkBufferUsageFlags)usage;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vma_ci = {};
    vma_ci.requiredFlags = (VkMemoryPropertyFlags)memoryflags;

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo alloc_info;

    vmaCreateBuffer(allocator, &buffer_create_info, &vma_ci, &buffer, &allocation, &alloc_info);

    return std::make_unique<Buffer>(this, buffer, allocation, alloc_info, size);
}

std::unique_ptr<Buffer> MemoryManager::GetAlignedBuffer(vk::DeviceSize size, vk::DeviceSize alignment, vk::BufferUsageFlags usage,
                                                        vk::MemoryPropertyFlags memoryflags)
{
    std::scoped_lock lg(allocation_mutex);
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size;
    buffer_create_info.usage = (VkBufferUsageFlags)usage;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vma_ci = {};
    vma_ci.requiredFlags = (VkMemoryPropertyFlags)memoryflags;

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo alloc_info;

    vmaCreateBufferWithAlignment(allocator, &buffer_create_info, &vma_ci, alignment, &buffer, &allocation, &alloc_info);

    return std::make_unique<Buffer>(this, buffer, allocation, alloc_info, size);
}

std::unique_ptr<Image> MemoryManager::GetImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                                               vk::MemoryPropertyFlags memoryflags, uint32_t arrayLayers)
{
    std::scoped_lock lg(allocation_mutex);
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = arrayLayers;
    image_info.format = (VkFormat)format;
    image_info.tiling = (VkImageTiling)tiling;
    image_info.initialLayout = (VkImageLayout)vk::ImageLayout::eUndefined;
    image_info.usage = (VkImageUsageFlags)usage;
    image_info.samples = (VkSampleCountFlagBits)vk::SampleCountFlagBits::e1;
    image_info.sharingMode = (VkSharingMode)vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo vma_ci = {};
    vma_ci.requiredFlags = (VkMemoryPropertyFlags)memoryflags;

    VkImage image;
    VmaAllocation allocation;
    VmaAllocationInfo alloc_info;

    vmaCreateImage(allocator, &image_info, &vma_ci, &image, &allocation, &alloc_info);

    return std::make_unique<Image>(this, image, allocation, alloc_info, alloc_info.size);
}

std::unique_ptr<GenericMemory> MemoryManager::GetMemory(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags memoryflags,
                                                        vk::MemoryAllocateFlags allocateflags)
{
    std::scoped_lock lg(allocation_mutex);
    VmaAllocationCreateInfo vma_ci = {};
    vma_ci.requiredFlags = (VkMemoryPropertyFlags)memoryflags;

    VmaAllocation allocation;
    VmaAllocationInfo alloc_info;

    vmaAllocateMemory(allocator, (VkMemoryRequirements*)&requirements, &vma_ci, &allocation, &alloc_info);

    return std::make_unique<GenericMemory>(this, allocation, alloc_info, requirements.size);
}
} // namespace lotus
