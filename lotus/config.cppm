module;

#include <cstdint>

export module lotus:core.config;

export namespace lotus
{
class Config
{
public:
    Config();
    struct Renderer
    {
        enum class RenderMode
        {
            Rasterization,
            Hybrid,
            Raytrace
        };
        RenderMode render_mode{RenderMode::Hybrid};

        uint32_t screen_width{1900};
        uint32_t screen_height{1000};
        uint32_t borderless{0};

        bool RaytraceEnabled();
        bool RasterizationEnabled();
        bool RendererShadowmappingEnabled();

    } renderer{};
    struct Audio
    {
        float master_volume{0.5f};
        float bgm_volume{1.0f};
        float se_volume{1.0f};
    } audio{};
};

Config::Config()
{
    // temporary spot to quickly change rendering mode
    // TODO: parse config from file
    renderer.render_mode = Renderer::RenderMode::Raytrace;
}
bool Config::Renderer::RaytraceEnabled() { return render_mode == RenderMode::Hybrid || render_mode == RenderMode::Raytrace; }

bool Config::Renderer::RasterizationEnabled() { return render_mode == RenderMode::Rasterization || render_mode == RenderMode::Hybrid; }

bool Config::Renderer::RendererShadowmappingEnabled() { return render_mode == RenderMode::Rasterization; }
} // namespace lotus
