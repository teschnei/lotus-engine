#pragma once
#include "engine/entity/component/third_person_entity_input_component.h"

class ThirdPersonEntityFFXIInputComponent : public lotus::ThirdPersonEntityInputComponent
{
public:
    explicit ThirdPersonEntityFFXIInputComponent(lotus::Entity*, lotus::Input*, lotus::Engine*);
    virtual void tick(lotus::time_point time, lotus::duration delta) override;
protected:
    bool moving_prev {false};
    lotus::Engine* engine{ nullptr };
};
