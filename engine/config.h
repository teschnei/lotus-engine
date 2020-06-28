#pragma once
#include <engine/types.h>

namespace lotus
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
            RenderMode render_mode{ RenderMode::Hybrid };

            uint32_t screen_width{ 1900 };
            uint32_t screen_height{ 1000 };
            uint32_t borderless{ 0 };

            bool RaytraceEnabled();
            bool RasterizationEnabled();

        } renderer {};
    };
}
