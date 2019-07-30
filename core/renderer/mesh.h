#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include "memory.h"
#include "core/work_item.h"
#include "texture.h"

namespace lotus
{
    class Engine;

    class Mesh
    {
    public:
        //TODO: figure out how to get engine out of this call
        template<typename MeshLoader, typename... Args>
        static std::pair<std::shared_ptr<Mesh>, std::unique_ptr<WorkItem>> LoadMesh(Engine* engine, const std::string& modelname, Args... args)
        {
            if (auto found = model_map.find(modelname); found != model_map.end())
            {
                return { found->second.lock(), nullptr };
            }
            auto new_model = std::make_shared<Mesh>();
            MeshLoader loader{args...};
            loader.setEngine(engine);
            auto work_item = loader.LoadMesh(new_model);
            return { model_map.emplace(modelname, new_model).first->second.lock(), std::move(work_item)};
        }
        //TODO: get rid of me
        static bool addMesh(const std::string& modelname, std::shared_ptr<Mesh>& model)
        {
            if (auto found = model_map.find(modelname); found != model_map.end())
            {
                return false;
            }
            model_map.emplace(modelname, model);
            return true;
        }
        //TODO: get rid of me
        static std::shared_ptr<Mesh> getMesh(const std::string& modelname)
        {
            if (auto found = model_map.find(modelname); found != model_map.end())
            {
                return found->second.lock();
            }
            return {};
        }
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh(Mesh&&) = default;
        Mesh& operator=(Mesh&&) = default;
        virtual ~Mesh() = default;

        std::vector<vk::VertexInputBindingDescription>& getVertexInputBindingDescription()
        {
            return vertex_bindings;
        }

        void setVertexInputBindingDescription(std::vector<vk::VertexInputBindingDescription>&& desc)
        {
            vertex_bindings = desc;
        }

        std::vector<vk::VertexInputAttributeDescription>& getVertexInputAttributeDescription()
        {
            return vertex_attributes;
        }

        void setVertexInputAttributeDescription(std::vector<vk::VertexInputAttributeDescription>&& attrs)
        {
            vertex_attributes = std::move(attrs);
        }

        int getIndexCount() const { return indices; }
        void setIndexCount(int _indices) { indices = _indices; }

        void setVertexBuffer(uint8_t* buffer, size_t len);

        std::unique_ptr<Buffer> vertex_buffer;
        std::unique_ptr<Buffer> index_buffer;

        std::shared_ptr<Texture> texture;

        //TODO: move me back to protected
        Mesh() = default;
    protected:

        inline static std::unordered_map<std::string, std::weak_ptr<Mesh>> model_map{};

        std::vector<vk::VertexInputBindingDescription> vertex_bindings;
        std::vector<vk::VertexInputAttributeDescription> vertex_attributes;
        int indices{ 0 };
    };

    class MeshLoader
    {
    public:
        MeshLoader() {}
        void setEngine(Engine* _engine) { engine = _engine; }
        virtual std::unique_ptr<WorkItem> LoadMesh(std::shared_ptr<Mesh>&) = 0;
        virtual ~MeshLoader() = default;
    protected:
        Engine* engine {nullptr};
    };
}
