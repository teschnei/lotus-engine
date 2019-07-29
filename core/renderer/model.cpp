#include "model.h"


void lotus::Model::setVertexBuffer(uint8_t* buffer, size_t len)
{
    //TODO: make this a task item
    //vk::DeviceSize bufferSize = len;

    //auto stagingBuffer = memoryManager.get_memory(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    //void* data;
    //vkMapMemory(device, stagingBuffer->memory, stagingBuffer->memory_offset, bufferSize, 0, &data);
    //    memcpy(data, entities[0]->model->vertex_buffer.data(), (size_t) bufferSize);
    //vkUnmapMemory(device, stagingBuffer->memory);

    //vertexBuffer = createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //copyBuffer(*stagingBuffer->buffer, *vertexBuffer->buffer, bufferSize);
}
