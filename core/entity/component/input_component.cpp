#include "input_component.h"
#include "../../input.h"

namespace lotus
{
    InputComponent::InputComponent(Entity* entity, Input* input) : Component(entity), input(input)
    {
        input->registerComponent(this);
    }

    InputComponent::~InputComponent()
    {
        input->deregisterComponent(this);
    }
}
