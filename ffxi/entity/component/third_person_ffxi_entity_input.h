#pragma once
#include "engine/entity/component/third_person_entity_input_component.h"

class ThirdPersonEntityFFXIInputComponent : public lotus::ThirdPersonEntityInputComponent
{
public:
    explicit ThirdPersonEntityFFXIInputComponent(lotus::Entity*, lotus::Engine*, lotus::Input*);
    virtual void tick(lotus::time_point time, lotus::duration delta) override;
protected:
    bool moving_prev {false};
    constexpr static glm::vec3 step_height { 0.f, -0.3f, 0.f };
};
