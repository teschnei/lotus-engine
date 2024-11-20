#pragma once
#include "types.h"

namespace lotus {
class Config {
public:
  Config();
  struct Renderer {
    enum class RenderMode { Rasterization, Hybrid, Raytrace };
    RenderMode render_mode{RenderMode::Hybrid};

    uint32_t screen_width{1900};
    uint32_t screen_height{1000};
    uint32_t borderless{0};

    bool RaytraceEnabled();
    bool RasterizationEnabled();
    bool RendererShadowmappingEnabled();

  } renderer{};
  struct Audio {
    float master_volume{0.5f};
    float bgm_volume{1.0f};
    float se_volume{1.0f};
  } audio{};
};
} // namespace lotus
