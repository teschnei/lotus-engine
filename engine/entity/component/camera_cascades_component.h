#pragma once
#include "component.h"

namespace lotus
{
    class CameraCascadesComponent : public Component
    {
    public:
        explicit CameraCascadesComponent(Entity*, Engine*);
        virtual Task<> render(Engine* engine, std::shared_ptr<Entity> sp) override;
    };
}
