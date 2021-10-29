#pragma once

#include "renderer.h"

namespace lotus
{
    class RendererRaytraceBase : public Renderer
    {
    public:
        RendererRaytraceBase(Engine* engine) : Renderer(engine) {}
    };
}
