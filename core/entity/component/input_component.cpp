#include "input_component.h"
#include "../../input.h"

namespace lotus
{
    InputComponent::InputComponent(Entity* entity, Input* input) : Component(entity), input(input)
    {
        input->RegisterComponent(this);
    }

    InputComponent::~InputComponent()
    {
        input->DeregisterComponent(this);
    }
}
