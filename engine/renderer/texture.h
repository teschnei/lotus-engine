#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <engine/renderer/vulkan/vulkan_inc.h>
#include "memory.h"
#include "../work_item.h"

namespace lotus
{
    class Engine;

    class Texture
    {
    public:
        //TODO: figure out how to get engine out of this call
        template<typename TextureLoader, typename... Args>
        [[nodiscard("Work must be queued in order to be processed")]]
        static std::pair<std::shared_ptr<Texture>, std::vector<UniqueWork>> LoadTexture(Engine* engine, const std::string& texturename, Args... args)
        {
            if (auto found = texture_map.find(texturename); found != texture_map.end())
            {
                return { found->second.lock(), std::vector<UniqueWork>() };
            }
            auto new_texture = std::shared_ptr<Texture>(new Texture());
            TextureLoader loader{args...};
            loader.setEngine(engine);
            auto work = loader.LoadTexture(new_texture);
            return {texture_map.emplace(texturename, new_texture).first->second.lock(), std::move(work)};
        }

        static std::shared_ptr<Texture> getTexture(const std::string& texturename)
        {
            if (auto found = texture_map.find(texturename); found != texture_map.end())
            {
                return found->second.lock();
            }
            return texture_map.at("default").lock();
        }

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;
        Texture(Texture&&) = default;
        Texture& operator=(Texture&&) = default;
        virtual ~Texture() = default;

        uint32_t getWidth() const { return width; }
        uint32_t getHeight() const { return height; }
        void setWidth(uint32_t _width) { width = _width; }
        void setHeight(uint32_t _height) { height = _height; }

        std::unique_ptr<Image> image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> image_view;
        vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> sampler;

    protected:
        Texture() = default;

        uint32_t width {0};
        uint32_t height {0};

        inline static std::unordered_map<std::string, std::weak_ptr<Texture>> texture_map{};
    };

    class TextureLoader
    {
    public:
        TextureLoader() {}
        void setEngine(Engine* _engine) { engine = _engine; }
        virtual ~TextureLoader() = default;
        virtual std::vector<UniqueWork> LoadTexture(std::shared_ptr<Texture>&) = 0;
    protected:
        Engine* engine {nullptr};
    };
}
