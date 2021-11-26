#include "generator_light_component.h"

#include "engine/core.h"
#include "engine/game.h"
#include "engine/scene.h"
#include "engine/entity/component/particle_component.h"
#include "entity/component/generator_component.h"
#include "dat/generator.h"
#include <glm/gtc/random.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace FFXI
{
    GeneratorLightComponent::GeneratorLightComponent(lotus::Entity* _entity, lotus::Engine* _engine, FFXI::Generator* _generator) :
        Component(_entity, _engine), generator(_generator)
    {
    }

    GeneratorLightComponent::~GeneratorLightComponent()
    {
        if (light)
            engine->lights->RemoveLight(light);
    }

    lotus::Task<> GeneratorLightComponent::init()
    {
        auto command = generator->buffer + generator->header->creation_command_offset - 16;

        while (command < generator->buffer + generator->header->tick_command_offset - 16)
        {
            uint8_t data_type = *command;
            uint8_t data_size = *(command + 1) & 0xF;
            command += 4;
            switch (data_type)
            {
            case 0x00:
                command = generator->buffer + generator->header->tick_command_offset - 16;
                break;
            case 0x01:
            {
                //maybe these are just combined into one big uint32_t of flags
                pos = *(glm::vec3*)(command + 16);
            }
                break;
            case 0x02:
            {
                dpos = *(glm::vec3*)(command);
            }
                break;
            case 0x03:
            {
                auto dpos_fluctuation = *(glm::vec3*)(command);
                dpos += glm::vec3(lotus::random::GetRandomNumber(dpos_fluctuation.x),
                    lotus::random::GetRandomNumber(dpos_fluctuation.y),
                    lotus::random::GetRandomNumber(dpos_fluctuation.z));
            }
                break;
            case 0x06:
            {
                auto gen_radius_sphere = *(float*)(command);
                auto gen_radius_sphere_fluctuation = *(float*)(command + 4);
                float sphere_radius_r = lotus::random::GetRandomNumber(gen_radius_sphere_fluctuation);
                pos += glm::sphericalRand(gen_radius_sphere + sphere_radius_r);
            }
                break;
            case 0x07:
                break;
            case 0x08:
            {
                if (glm::length(pos) > 0)
                {
                    auto dpos_origin = *(float*)(command);
                    auto origin_dir = glm::normalize(pos);
                    dpos += origin_dir * dpos_origin;
                }
            }
                break;
            case 0x16:
            {
                auto color_uint = *(uint32_t*)(command);
                colour = glm::vec4{ (color_uint & 0xFF) / 128.f, ((color_uint & 0xFF00) >> 8) / 128.f, ((color_uint & 0xFF0000) >> 16) / 128.f, ((color_uint & 0xFF000000) >> 24) / 128.f };
            }
                break;
            }
            command += (data_size - 1) * sizeof(uint32_t);
        }

        if (glm::length(pos - glm::vec3(419, -53.f, -103.f)) < 10.f)
        {

            light = engine->lights->AddLight({
                .pos = pos,
                .intensity = 100.f,
                .colour = colour,
                .radius = 0.5f
                });
        }

        co_return;
    }

    lotus::Task<> GeneratorLightComponent::tick(lotus::time_point time, lotus::duration delta)
    {
        float movement = 30.f / ((float)std::chrono::nanoseconds(1s).count() / std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count());

        auto command = generator->buffer + generator->header->tick_command_offset - 16;
        while (command < generator->buffer + generator->header->expiry_command_offset - 16)
        {
            uint8_t data_type = *command;
            uint8_t data_size = *(command + 1) & 0xF;
            command += 4;
            switch (data_type)
            {
            case 0x00:
                command = generator->buffer + generator->header->expiry_command_offset - 16;
                break;
            case 0x02:
                //apply static positional changes
            {
                pos += dpos * movement;
            }
                break;
            case 0x03:
            {
                //add to velocity (acceleration)
                dpos += *(glm::vec3*)(command) * movement;
            }
                break;
            case 0x2C:
            {
                auto dpos_exp = *(float*)(command);
            }
                break;
            default:
                break;
            }
            command += (data_size - 1) * sizeof(uint32_t);
        }
        if (light)
        {
            engine->lights->UpdateLight(light, {
                .pos = pos,
                .intensity = 300.f,
                .colour = colour,
                .radius = 0.5f
                });
        }
        co_return;
    }
}
