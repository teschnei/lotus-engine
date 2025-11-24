module;

#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

export module lotus:renderer.model;

import :renderer.acceleration_structure;
import :renderer.mesh;
import :renderer.memory;
import :renderer.vulkan.common.global_descriptors;
import :util;
import vulkan_hpp;
import glm;

export namespace lotus
{
class Model
{
public:
    // TODO: when to clean up dead weak_ptrs?
    template <typename Loader, typename... Args>
    [[nodiscard("Work must be awaited before being used")]]
    static std::pair<std::shared_ptr<Model>, std::optional<Task<>>> LoadModel(std::string modelname, Loader loader, Args&&... args)
    {
        if (!modelname.empty())
        {
            if (auto found = model_map.find(modelname); found != model_map.end())
            {
                auto ptr = found->second.lock();
                if (ptr)
                    return {ptr, std::optional<Task<>>{}};
                else
                    model_map.erase(found);
            }
        }
        auto new_model = std::shared_ptr<Model>(new Model(modelname));
        auto task = loader(new_model, std::forward<Args>(args)...);
        if (!modelname.empty())
        {
            return {model_map.emplace(modelname, new_model).first->second.lock(), std::move(task)};
        }
        else
        {
            return {new_model, std::move(task)};
        }
    }

    static std::shared_ptr<Model> getModel(const std::string& modelname)
    {
        if (auto found = model_map.find(modelname); found != model_map.end())
        {
            return found->second.lock();
        }
        return {};
    }

    template <typename T> static void forEachModel(T func)
    {
        for (const auto& [name, model] : model_map)
        {
            if (auto ptr = model.lock())
            {
                func(ptr);
            }
        }
    }

    struct TransformEntry
    {
        glm::mat3x4 transform;
        uint32_t mesh_index;
    };

    [[nodiscard]]
    WorkerTask<> InitWork(Engine* engine, const std::vector<std::span<const std::byte>>& vertex_buffers,
                          const std::vector<std::span<const std::byte>>& index_buffers, uint32_t vertex_stride, std::vector<TransformEntry>&& transforms = {});

    [[nodiscard]]
    WorkerTask<> InitWorkAABB(Engine* engine, std::vector<uint8_t>&& vertex_buffer, std::vector<uint16_t>&&, uint32_t vertex_stride, float aabb_dist);

    std::string name;
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::unique_ptr<Buffer> vertex_buffer;
    std::unique_ptr<Buffer> index_buffer;
    std::unique_ptr<Buffer> transform_buffer;
    std::unique_ptr<Buffer> aabbs_buffer;
    bool is_static{false};
    bool weighted{false};
    Lifetime lifetime{Lifetime::Short};
    bool rendered{true};
    // TODO: probably remove this
    uint32_t light_offset{0};
    float animation_frame{0};

    std::unique_ptr<BottomLevelAccelerationStructure> bottom_level_as;

protected:
    explicit Model(const std::string& name);

    inline static std::unordered_map<std::string, std::weak_ptr<Model>> model_map{};
};
} // namespace lotus
