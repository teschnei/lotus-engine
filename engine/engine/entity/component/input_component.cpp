#include "input_component.h"
#include "../../input.h"

namespace lotus
{
    InputComponent::InputComponent(Entity* entity, Engine* engine, Input* input) : Component(entity, engine), input(input)
    {
        input->RegisterComponent(this);
    }

    InputComponent::~InputComponent()
    {
        input->DeregisterComponent(this);
    }
}
