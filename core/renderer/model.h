#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include "memory.h"
#include "../work_item.h"

namespace lotus
{
    class Engine;

    class Model
    {
    public:
        //TODO: figure out how to get engine out of this call
        template<typename ModelLoader, typename... Args>
        static std::pair<std::shared_ptr<Model>, std::unique_ptr<WorkItem>> LoadModel(Engine* engine, const std::string& modelname, Args... args)
        {
            if (auto found = model_map.find(modelname); found != model_map.end())
            {
                return { found->second.lock(), nullptr };
            }
            auto new_model = std::make_shared<Model>();
            ModelLoader loader{args...};
            loader.setEngine(engine);
            auto work_item = loader.LoadModel(new_model);
            return { model_map.emplace(modelname, new_model).first->second.lock(), std::move(work_item)};
        }
        //TODO: get rid of me
        static bool addModel(const std::string& modelname, std::shared_ptr<Model>& model)
        {
            if (auto found = model_map.find(modelname); found != model_map.end())
            {
                return false;
            }
            model_map.emplace(modelname, model);
            return true;
        }
        //TODO: get rid of me
        static std::shared_ptr<Model> getModel(const std::string& modelname)
        {
            if (auto found = model_map.find(modelname); found != model_map.end())
            {
                return found->second.lock();
            }
            return {};
        }
        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;
        Model(Model&&) = default;
        Model& operator=(Model&&) = default;
        virtual ~Model() = default;

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

        int getIndexCount() { return indices; }
        void setIndexCount(int _indices) { indices = _indices; }

        void setVertexBuffer(uint8_t* buffer, size_t len);

        std::unique_ptr<Buffer> vertex_buffer;
        std::unique_ptr<Buffer> index_buffer;

        std::vector<std::shared_ptr<Model>> m_pieces;

        //TODO: move me back to protected
        Model() = default;
    protected:

        inline static std::unordered_map<std::string, std::weak_ptr<Model>> model_map{};

        std::vector<vk::VertexInputBindingDescription> vertex_bindings;
        std::vector<vk::VertexInputAttributeDescription> vertex_attributes;
        int indices{ 0 };
    };

    class ModelLoader
    {
    public:
        ModelLoader() {}
        void setEngine(Engine* _engine) { engine = _engine; }
        virtual std::unique_ptr<WorkItem> LoadModel(std::shared_ptr<Model>&) = 0;
        virtual ~ModelLoader() = default;
    protected:
        Engine* engine {nullptr};
    };
}
