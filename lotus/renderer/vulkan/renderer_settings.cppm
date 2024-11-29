module;

#include <cstdint>

export module lotus:renderer.vulkan.settings;

export namespace lotus
{
struct RendererSettings
{
    uint32_t shadowmap_dimension{2048};
};
} // namespace lotus
