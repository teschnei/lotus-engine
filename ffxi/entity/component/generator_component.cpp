#include "generator_component.h"

#include "engine/core.h"
#include "ffxi.h"
#include "generator_light_component.h"
#include "dat/generator.h"
#include "dat/sep.h"
#include "entity/particle.h"

namespace FFXI
{
    GeneratorComponent::GeneratorComponent(lotus::Entity* _entity, lotus::Engine* _engine, FFXI::Generator* _generator, lotus::duration _duration, FFXI::SchedulerComponent* _parent) :
        Component(_entity, _engine), generator(_generator), duration(_duration), parent(_parent), start_time(engine->getSimulationTime())
    {
    }

    lotus::Task<> GeneratorComponent::init()
    {
        std::string id;
        std::vector<D3M::Vertex> ring_vertices;
        std::vector<uint16_t> ring_indices;
        auto command = generator->buffer + generator->header->creation_command_offset - 16;
        //figure out what type of generator this is
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
                id = std::string((char*)command + 8, 4);
                base_pos = *(glm::vec3*)(command + 16);
                type = static_cast<Type>(*(uint8_t*)(command + 29));
                break;
            case 0x3a:
            {
                //generate a mesh (ring?)
                float* radii = (float*)(command);
                uint32_t* colours = (uint32_t*)(command + 16);
                uint8_t segments = *(uint8_t*)(command + 32);
                uint8_t circles = 2 + *(uint8_t*)(command + 33);

                for (auto r = 0; r < circles; ++r)
                {
                    for (auto v = 0; v < segments; ++v)
                    {
                        D3M::Vertex vertex;
                        vertex.pos = glm::vec3(std::cos((v * 2 * glm::pi<float>()) / segments) * radii[r], std::sin((v * 2 * glm::pi<float>()) / segments) * radii[r], 0.0);
                        vertex.normal = glm::vec3(0.0, 0.0, 1.0);
                        vertex.color = glm::vec4{ ((colours[r] & 0x0000FF)) / 128.f, ((colours[r] & 0x00FF00) >> 8) / 128.f, ((colours[r] & 0xFF0000) >> 16) / 128.f, ((colours[r] & 0xFF000000) >> 24) / 128.f };
                        vertex.uv = glm::vec2(0.0);
                        ring_vertices.push_back(vertex);
                    }
                }
                for (auto r = 0; r < circles - 1; ++r)
                {
                    uint16_t base = r * segments;
                    for (auto v = 0; v < segments; ++v)
                    {
                        ring_indices.push_back(base + v);
                        ring_indices.push_back(base + v + segments);
                        ring_indices.push_back(base + (v + 1) % segments);

                        ring_indices.push_back(base + v + segments);
                        ring_indices.push_back(base + (v + 1) % segments);
                        ring_indices.push_back(base + (v + 1) % segments + segments);
                    }
                }
            }
            break;
            }
            command += (data_size - 1) * sizeof(uint32_t);
        }

        switch (type)
        {
        case Type::ModelD3MMMB:
            model = lotus::Model::getModel(id);
            break;
        case Type::ModelD3A:
            model = lotus::Model::getModel(id + "_d3a");
            break;
        case Type::ModelRing:
        {
            auto [new_model, model_task] = lotus::Model::LoadModel(std::string(generator->name, 4) + "_" + id,
                FFXI::D3MLoader::LoadModelRing, engine, std::move(ring_vertices), std::move(ring_indices));
            model = new_model;
            if (model_task)
                co_await *model_task;
        }
            break;
        case Type::Sound:
            for (const auto& chunk : generator->parent->children)
            {
                if (auto sep = dynamic_cast<FFXI::Sep*>(chunk.get()); sep && std::string(chunk->name, 4) == generator->id)
                {
                    sound = sep->playSound(static_cast<FFXIGame*>(engine->game)->audio.get());
                    break;
                }
            }
            break;
        case Type::Light:
            light = true;
            break;
        default:
            //std::cout << std::format("unhandled generation type: {:#04x}", static_cast<uint8_t>(type)) << std::endl;
            break;
        }
    }

    lotus::Task<> GeneratorComponent::tick(lotus::time_point time, lotus::duration delta)
    {
        uint32_t frequency = 1000 * ((generator->header->interval + 1) / 60.f);
        auto next_generation = std::chrono::milliseconds(generations * frequency);

        auto total_generations = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / frequency;

        if (time > start_time + duration && duration > 0ms && generations > total_generations)
        {
            //if (componentsEmpty())
            {
                remove();
            }
            co_return;
        }

        if (time > start_time + next_generation && (generations <= total_generations || (generator->header->flags5 & 0x10 && model)))
        {
            for (int occ = 0; occ < generator->header->occurences + 1; ++occ)
            {
                //spawn new particle
                //MMB not supported yet (needs new pipelines...)
                //if (model && model->meshes[0]->pipelines.size() > 2 && std::string(generator->name, 4) == "l067" && model->name[0] == 'h')
                //TODO: only for landscape generators
                if (engine->camera && glm::distance2(engine->camera->getPos(), base_pos) < (100.f * 100.f))
                {
                    if (model && model->meshes[0]->pipelines.size() > 2)
                    {
                        auto e = co_await engine->game->scene->AddEntity<FFXIParticle>(entity->getSharedPtr(), generator, model, index);
                    }
                    if (sound)
                    {

                    }
                    if (light)
                    {
                        auto e = engine->game->scene->AddComponents(co_await FFXI::GeneratorLightComponent::make_component(entity, engine, generator));
                    }
                }
                index++;
            }
            generations++;
        }

        co_return;
    }
}
