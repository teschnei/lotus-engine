#pragma once
#include "engine/entity/component/input_component.h"

class EquipmentTestComponent : public lotus::InputComponent
{
public:
    explicit EquipmentTestComponent(lotus::Entity*, lotus::Engine*, lotus::Input*);
    virtual lotus::Task<> tick(lotus::time_point time, lotus::duration delta) override;
    virtual bool handleInput(const SDL_Event&) override;
protected:
    std::optional<uint16_t> new_modelid;

};
