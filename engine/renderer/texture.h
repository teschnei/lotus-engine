#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <engine/renderer/vulkan/vulkan_inc.h>
#include "memory.h"
#include "engine/worker_task.h"

namespace lotus
{
    class Engine;
    class Texture;

    class TextureLoaderBase
    {
    public:
        void setEngine(Engine* _engine) { engine = _engine; }
    protected:
        Engine* engine {nullptr};
    };

    template<typename T>
    concept TextureLoader = std::derived_from<T, TextureLoaderBase> &&
        requires(T t, Engine* e, std::shared_ptr<Texture>& s)
    {
        {t.LoadTexture(s)} -> std::convertible_to<lotus::Task<>>;
    };

    class Texture
    {
    public:
        //TODO: figure out how to get engine out of this call
        template<TextureLoader Loader, typename... Args>
        [[nodiscard("Work must be queued in order to be processed")]]
        static Task<std::shared_ptr<Texture>> LoadTexture(Engine* engine, std::string texturename, Args... args)
        {
            if (auto found = texture_map.find(texturename); found != texture_map.end())
            {
                co_return found->second.lock();
            }
            auto new_texture = std::shared_ptr<Texture>(new Texture(texturename));
            texture_map.emplace(texturename, new_texture);
            Loader loader{args...};
            loader.setEngine(engine);
            co_await loader.LoadTexture(new_texture);
            co_return new_texture;
        }

        template<typename Func>
        [[nodiscard("Work must be queued in order to be processed")]]
        static Task<std::shared_ptr<Texture>> LoadTexture(std::string texturename, Func func)
        {
            if (auto found = texture_map.find(texturename); found != texture_map.end())
            {
                co_return found->second.lock();
            }
            auto new_texture = std::shared_ptr<Texture>(new Texture(texturename));
            texture_map.emplace(texturename, new_texture);
            co_await func(new_texture);
            co_return new_texture;
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

        WorkerTask<> Init(Engine* engine, std::vector<uint8_t>&& texture_data);

        uint32_t getWidth() const { return width; }
        uint32_t getHeight() const { return height; }
        void setWidth(uint32_t _width) { width = _width; }
        void setHeight(uint32_t _height) { height = _height; }
        std::string getName() { return name; }

        std::unique_ptr<Image> image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> image_view;
        vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> sampler;

    protected:
        Texture(const std::string& _name) : name(_name) {}

        uint32_t width {0};
        uint32_t height {0};
        std::string name;

        inline static std::unordered_map<std::string, std::weak_ptr<Texture>> texture_map{};
    };

}
