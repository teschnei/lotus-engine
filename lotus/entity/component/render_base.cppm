module;

#include <coroutine>
#include <memory>

export module lotus:entity.component.render_base;

import :core.engine;
import :entity.component;
import :entity.component.camera;
import :renderer.memory;
import :renderer.vulkan.renderer;
import :util;
import glm;
import vulkan_hpp;

export namespace lotus::Component
{
class RenderBaseComponent : public Component<RenderBaseComponent>
{
public:
    explicit RenderBaseComponent(Entity*, Engine* engine);
    ~RenderBaseComponent();

    Task<> tick(time_point time, duration elapsed);
    Task<> init();

    std::tuple<vk::Buffer, size_t, size_t> getUniformBuffer(uint32_t image) const;

    glm::mat4 getModelMatrix() const { return model; }
    glm::mat4 getModelMatrixT() const { return modelT; }
    glm::mat4 getModelMatrixIT() const { return modelIT; }
    glm::mat4 getPrevModelMatrix() const { return model_prev; }

    glm::vec3 getPos() const { return pos; }
    glm::quat getRot() const { return rot; }
    glm::vec3 getScale() const { return scale; }

    void setPos(glm::vec3 pos);
    void setRot(glm::quat rot);
    void setScale(glm::vec3 scale);

    class Billboard
    {
    public:
        static constexpr uint8_t None = 0;
        static constexpr uint8_t X = 1;
        static constexpr uint8_t Y = 2;
        static constexpr uint8_t Z = 4;
        static constexpr uint8_t All = 7;
    };

    void setBillboard(uint8_t b) { billboard = b; }
    uint8_t getBillboard() const { return billboard; }

protected:
    struct UniformBufferObject
    {
        glm::mat4 model;
        glm::mat4 modelIT;
        glm::mat4 model_prev;
    };

    std::unique_ptr<Buffer> uniform_buffer;
    uint8_t* uniform_buffer_mapped{nullptr};

    bool should_update_matrix{true};

    glm::vec3 pos{0.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
    glm::vec3 scale{1.f, 1.f, 1.f};

    uint8_t billboard{0};

    glm::mat4 model{};
    glm::mat4 modelT{};
    glm::mat4 modelIT{};
    glm::mat4 model_prev{};
};

RenderBaseComponent::RenderBaseComponent(Entity* _entity, Engine* _engine) : Component(_entity, _engine) {}

RenderBaseComponent::~RenderBaseComponent() { uniform_buffer->unmap(); }

Task<> RenderBaseComponent::init()
{
    uniform_buffer = engine->renderer->gpu->memory_manager->GetBuffer(
        engine->renderer->uniform_buffer_align_up(sizeof(UniformBufferObject)) * engine->renderer->getFrameCount(), vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    uniform_buffer_mapped = static_cast<uint8_t*>(
        uniform_buffer->map(0, engine->renderer->uniform_buffer_align_up(sizeof(UniformBufferObject)) * engine->renderer->getFrameCount(), {}));
    co_return;
}

Task<> RenderBaseComponent::tick(time_point time, duration elapsed)
{
    model_prev = model;
    if (should_update_matrix)
    {
        if (billboard != Billboard::None)
        {
            auto rot_mat = glm::transpose(glm::mat4_cast(rot));
            auto camera_mat = glm::mat4(glm::transpose(glm::mat3(engine->camera->getViewMatrix())));
            if (billboard == Billboard::Y)
            {
                camera_mat[1] = glm::vec4(0, 1, 0, 0);
                camera_mat[2].y = 0;
            }
            model = glm::translate(glm::mat4{1.f}, pos) * camera_mat * rot_mat * glm::scale(glm::mat4{1.f}, scale);
        }
        else
        {
            model = glm::translate(glm::mat4{1.f}, pos) * glm::transpose(glm::mat4_cast(rot)) * glm::scale(glm::mat4{1.f}, scale);
        }
        modelT = glm::transpose(model);
        modelIT = glm::mat3(glm::transpose(glm::inverse(model)));
        should_update_matrix = false;
    }
    UniformBufferObject* ubo = reinterpret_cast<UniformBufferObject*>(
        uniform_buffer_mapped + engine->renderer->getCurrentFrame() * engine->renderer->uniform_buffer_align_up(sizeof(UniformBufferObject)));
    ubo->model = model;
    ubo->modelIT = modelIT;
    ubo->model_prev = model_prev;
    co_return;
}

std::tuple<vk::Buffer, size_t, size_t> RenderBaseComponent::getUniformBuffer(uint32_t image_index) const
{
    return {uniform_buffer->buffer, image_index * engine->renderer->uniform_buffer_align_up(sizeof(UniformBufferObject)), sizeof(UniformBufferObject)};
}

void RenderBaseComponent::setPos(glm::vec3 _pos)
{
    pos = _pos;
    should_update_matrix = true;
}

void RenderBaseComponent::setRot(glm::quat _rot)
{
    rot = _rot;
    should_update_matrix = true;
}

void RenderBaseComponent::setScale(glm::vec3 _scale)
{
    scale = _scale;
    should_update_matrix = true;
}
} // namespace lotus::Component
