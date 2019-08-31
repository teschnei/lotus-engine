#pragma once

#include "core/renderer/mesh.h"
#include "acceleration_structure.h"

namespace lotus
{
    class Model
    {
    public:
        //TODO: figure out how to get engine out of this call
        template<typename ModelLoader, typename... Args>
        static std::shared_ptr<Model> LoadModel(Engine* engine, const std::string& modelname, Args... args)
        {
            if (auto found = model_map.find(modelname); found != model_map.end())
            {
                return found->second.lock();
            }
            auto new_model = std::shared_ptr<Model>(new Model(modelname));
            ModelLoader loader{args...};
            loader.setEngine(engine);
            loader.LoadModel(new_model);
            return model_map.emplace(modelname, new_model).first->second.lock();
        }

        static std::shared_ptr<Model> getModel(const std::string& modelname)
        {
            if (auto found = model_map.find(modelname); found != model_map.end())
            {
                return found->second.lock();
            }
            return {};
        }

        std::string name;
        std::vector<std::unique_ptr<Mesh>> meshes;

        std::unique_ptr<BottomLevelAccelerationStructure> bottom_level_as;

    protected:
        explicit Model(const std::string& name);

        inline static std::unordered_map<std::string, std::weak_ptr<Model>> model_map{};
    };

    class ModelLoader
    {
    public:
        ModelLoader() {}
        void setEngine(Engine* _engine) { engine = _engine; }
        virtual ~ModelLoader() = default;
        virtual void LoadModel(std::shared_ptr<Model>&) = 0;
    protected:

        Engine* engine {nullptr};
    };
}
