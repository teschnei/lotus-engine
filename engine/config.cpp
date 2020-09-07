#include "config.h"

namespace lotus
{
    Config::Config()
    {
        //temporary spot to quickly change rendering mode
        //TODO: parse config from file
        renderer.render_mode = Renderer::RenderMode::Rasterization;
    }
    bool Config::Renderer::RaytraceEnabled()
    {
        return render_mode == RenderMode::Hybrid || render_mode == RenderMode::Raytrace;
    }

    bool Config::Renderer::RasterizationEnabled()
    {
        return render_mode == RenderMode::Rasterization || render_mode == RenderMode::Hybrid;
    }
}
