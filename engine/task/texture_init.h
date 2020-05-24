#pragma once

#include "../work_item.h"
#include "../renderer/texture.h"

namespace lotus
{
    class TextureInitTask : public WorkItem
    {
    public:
        TextureInitTask(int image_index, std::shared_ptr<Texture> model, vk::Format format, vk::ImageTiling tiling, std::vector<uint8_t>&& texture_data);
        virtual ~TextureInitTask() override = default;
        virtual void Process(WorkerThread*) override;

    private:
        int image_index;
        std::shared_ptr<Texture> texture;
        vk::Format format;
        vk::ImageTiling tiling;
        std::vector<uint8_t> texture_data;
        std::unique_ptr<Buffer> staging_buffer;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
    };
}
