#include "particle_component.h"

#include "engine/core.h"
#include "engine/game.h"
#include "engine/scene.h"
#include "engine/entity/component/particle_component.h"
#include "entity/component/generator_component.h"
#include "entity/component/actor_skeleton_component.h"
#include "dat/generator.h"
#include <glm/gtc/random.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace FFXI
{
    ParticleComponent::ParticleComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Component::ParticleComponent& _engine_particle,
        lotus::Component::RenderBaseComponent& _base, std::weak_ptr<lotus::Entity> _actor, FFXI::Generator* _generator, size_t _index) :
        Component(_entity, _engine), particle_component(_engine_particle), base_component(_base), actor(_actor), generator(_generator), index(_index), start_time(engine->getSimulationTime())
    {
        for (const auto& chunk : generator->parent->children)
        {
            if (auto keyframe = dynamic_cast<FFXI::Keyframe*>(chunk.get()))
            {
                keyframes.insert(std::make_pair(std::string(keyframe->name, 4), keyframe));
            }
        }
    }

    ParticleComponent::~ParticleComponent()
    {
        if (light)
            engine->lights->RemoveLight(light);
    }

    lotus::Task<> ParticleComponent::init()
    {
        auto actor_component = engine->game->scene->component_runners->getComponent<FFXI::ActorSkeletonComponent>(actor.lock().get());
        auto animation_component = engine->game->scene->component_runners->getComponent<lotus::Component::AnimationComponent>(actor.lock().get());
        auto generator_points = actor_component->getSkeletonStatic()->getGeneratorPoints();

        //TODO: 90% sure this should be generator->header->flags1 & 0x02?
        if (generator->header->flags2 == 0x80 && generator->header->flags3 == 0x08)
        {
            if (auto bone_id = generator->header->bone_point >> 2; bone_id < FFXI::SK2::GeneratorPointMax)
            {
                auto& bone_point = generator_points[bone_id];
                bone = &animation_component->skeleton->bones[bone_point.bone_index];
                bone_offset = bone_point.offset;
            }
        }
        else if (generator->header->flags1 & 0x01)
        {
            if (auto bone_id = (generator->header->flags1 + ((generator->header->bone_point & 0x01) << 8)) >> 4; bone_id < FFXI::SK2::GeneratorPointMax)
            {
                auto& bone_point = generator_points[bone_id];
                bone = &animation_component->skeleton->bones[bone_point.bone_index];
                bone_offset = bone_point.offset;
            }
        }

        std::string id;
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
                base_component.setBillboard(*(uint16_t*)(command));
                pos_flags = *(uint16_t*)(command + 2);
                id = std::string((char*)command + 8, 4);
                local_pos = *(glm::vec3*)(command + 16);
                duration = std::chrono::milliseconds(((long long)((long long)(*(uint16_t*)(command + 30)) * (1000 / 60.f))));
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
                local_pos += glm::sphericalRand(gen_radius_sphere + sphere_radius_r);
            }
                break;
            case 0x07:
                break;
            case 0x08:
            {
                if (local_pos.length() > 0)
                {
                    auto dpos_origin = *(float*)(command);
                    auto origin_dir = glm::normalize(local_pos);
                    dpos += origin_dir * dpos_origin;
                }
            }
                break;
            case 0x09:
            {
                local_rot = *(glm::vec3*)(command);
            }
                break;
            case 0x0a:
            {
                auto rot_fluctuation = *(glm::vec3*)(command);
                local_rot += glm::vec3(lotus::random::GetRandomNumber(rot_fluctuation.x),
                    lotus::random::GetRandomNumber(rot_fluctuation.y),
                    lotus::random::GetRandomNumber(rot_fluctuation.z));
            }
                break;
            case 0x0b:
                drot = *(glm::vec3*)(command);
                break;
            case 0x0c:
            {
                auto drot_fluctuation = *(glm::vec3*)(command);
                drot += glm::vec3(lotus::random::GetRandomNumber(drot_fluctuation.x),
                    lotus::random::GetRandomNumber(drot_fluctuation.y),
                    lotus::random::GetRandomNumber(drot_fluctuation.z));
            }
                break;
            case 0x0f:
            {
                local_scale = *(glm::vec3*)(command);
            }
                break;
            case 0x10:
            {
                auto scale_fluctuation = *(glm::vec3*)(command);
                local_scale += glm::vec3(lotus::random::GetRandomNumber(scale_fluctuation.x),
                    lotus::random::GetRandomNumber(scale_fluctuation.y),
                    lotus::random::GetRandomNumber(scale_fluctuation.z));
            }
                break;
            case 0x11:
            {
                auto scale_all_fluctuation = *(float*)(command);
                local_scale += glm::vec3(lotus::random::GetRandomNumber(scale_all_fluctuation));
            }
                break;
            case 0x12:
            {
                dscale = *(glm::vec3*)(command);
            }
                break;
            case 0x13:
            {
                auto dscale_fluctuation = *(glm::vec3*)(command);
                dscale += glm::vec3(lotus::random::GetRandomNumber(dscale_fluctuation.x),
                    lotus::random::GetRandomNumber(dscale_fluctuation.y),
                    lotus::random::GetRandomNumber(dscale_fluctuation.z));
            }
                break;
            case 0x16:
            {
                auto color = *(uint32_t*)(command);
                particle_component.color = glm::vec4{ (color & 0xFF) / 128.f, ((color & 0xFF00) >> 8) / 128.f, ((color & 0xFF0000) >> 16) / 128.f, ((color & 0xFF000000) >> 24) / 128.f };
            }
                break;
            case 0x17:
                //color fluctuation?
                break;
            case 0x18:
                break;
            case 0x19:
                break;
            case 0x1a:
                break;
            case 0x1d:
                break;
            case 0x1e:
            {
                auto blend_method = *(uint32_t*)(command);
                particle_component.pipeline_index = (blend_method & 0b110) >> 1;
            }
                break;
            case 0x1f:
            {
                auto gen_radius_fluctuation = *(float*)(command);
                auto gen_radius = *(float*)(command + 4);
                auto gen_multi = *(glm::vec3*)(command + 8);
                auto gen_axis_rot = *(glm::vec2*)(command + 20);
                auto gen_height = *(float*)(command + 28);
                auto gen_height_fluctuation = *(float*)(command + 32);
                auto rotations = *(uint32_t*)(command + 40);

                glm::vec3 cylinder{ gen_radius + lotus::random::GetRandomNumber(gen_radius_fluctuation), lotus::random::GetRandomNumber(gen_height), 0.f };
                float cylinder_rot = 0.f;
                if (rotations > 0)
                {
                    cylinder_rot = ((glm::pi<float>() * 2) / (rotations + 1)) * index;
                }
                else
                {
                    cylinder_rot = lotus::random::GetRandomNumber(0.f, glm::pi<float>() * 2);
                }
                cylinder = glm::rotateY(cylinder, cylinder_rot);
                cylinder *= gen_multi;
                cylinder = glm::rotateX(cylinder, gen_axis_rot.y);
                cylinder = glm::rotateZ(cylinder, gen_axis_rot.x);
                local_pos += cylinder;

                if (base_component.getBillboard() == lotus::Component::RenderBaseComponent::Billboard::None)
                {
                    auto rot_mat = glm::eulerAngleXZY(local_rot.x, local_rot.z, local_rot.y);
                    auto gen_rot_mat = glm::eulerAngleXZY(0.f, 0.f, cylinder_rot);
                    auto final_rot_mat = gen_rot_mat * rot_mat;
                    glm::extractEulerAngleXZY(final_rot_mat, local_rot.x, local_rot.z, local_rot.y);
                }
            }
                break;
            case 0x21:
                kf_x_pos = std::string((char*)command + 4, 4);
                break;
            case 0x22:
                kf_y_pos = std::string((char*)command + 4, 4);
                break;
            case 0x23:
                kf_z_pos = std::string((char*)command + 4, 4);
                break;
            case 0x24:
                kf_z_rot = std::string((char*)command + 4, 4);
                break;
            case 0x25:
                kf_z_rot = std::string((char*)command + 4, 4);
                break;
            case 0x26:
                kf_z_rot = std::string((char*)command + 4, 4);
                break;
            case 0x27:
                kf_x_scale = std::string((char*)command + 4, 4);
                break;
            case 0x28:
                kf_y_scale = std::string((char*)command + 4, 4);
                break;
            case 0x29:
                kf_z_scale = std::string((char*)command + 4, 4);
                break;
            case 0x2a:
                kf_r = std::string((char*)command + 4, 4);
                break;
            case 0x2b:
                kf_g = std::string((char*)command + 4, 4);
                break;
            case 0x2c:
                kf_b = std::string((char*)command + 4, 4);
                break;
            case 0x2d:
                kf_a = std::string((char*)command + 4, 4);
                break;
            case 0x2e:
                kf_u = std::string((char*)command + 4, 4);
                break;
            case 0x2f:
                kf_v = std::string((char*)command + 4, 4);
                break;
            case 0x30:
                break;
            case 0x31:
                break;
            case 0x32:
                break;
            case 0x33:
                break;
            case 0x34:
                break;
            case 0x35:
                break;
            case 0x36:
                break;
            case 0x39:
                break;
            case 0x3a:
                //handled in generator component
                break;
            case 0x3b:
                //gen_rot_add: value added to rot after each generation
                //local_rot += *(glm::vec3*)(command) * glm::vec3(index);
                break;
            case 0x3c:
                //another generator id
                break;
            case 0x3d:
                break;
            case 0x3e:
                break;
            case 0x3f:
                break;
            case 0x40:
                break;
            case 0x41:
                break;
            case 0x42:
                break;
            case 0x43:
                break;
            case 0x44:
            {
                auto sub_generator_id = std::string((char*)command + 4, 4);
                //assume sub generator is in same place as generator
                for (const auto& chunk : generator->parent->children)
                {
                    if (auto sub_generator = dynamic_cast<FFXI::Generator*>(chunk.get()); sub_generator && std::string(chunk->name, 4) == sub_generator_id)
                    {
                        engine->game->scene->AddComponents(co_await FFXI::GeneratorComponent::make_component(entity, engine, sub_generator, duration, nullptr));
                        break;
                    }
                }
            }
                break;
            case 0x45:
                break;
            case 0x47:
                break;
            case 0x49:
                break;
            case 0x4C:
                break;
            case 0x4F:
                break;
            case 0x53:
                break;
            case 0x54:
                break;
            case 0x55:
                break;
            case 0x56:
                break;
            case 0x58:
                break;
            case 0x59:
                break;
            case 0x5A:
                break;
            case 0x5B:
                break;
            case 0x60:
                break;
            case 0x61:
                break;
            case 0x62:
                break;
            case 0x63:
                break;
            case 0x67:
                break;
            case 0x6A:
                break;
            case 0x6C:
                break;
            case 0x72:
                break;
            case 0x78:
                break;
            case 0x82:
                break;
            case 0x87:
                break;
            case 0x8F:
                break;
            }
            command += (data_size - 1) * sizeof(uint32_t);
        }

        auto parent_render = engine->game->scene->component_runners->getComponent<lotus::Component::RenderBaseComponent>(actor.lock().get());
        origin = parent_render ? parent_render->getPos() : glm::vec3{};
        if (bone)
        {
            origin += bone->trans + (bone->rot * (bone_offset * bone->scale));
        }

        auto rot_mat = glm::eulerAngleXZY(local_rot.x, local_rot.z, local_rot.y);

        if (glm::length(local_pos - glm::vec3(419, -53.f, -103.f)) < 50.f)
        {
            //std::cout << std::format("{} {} {}", local_pos.x, local_pos.y, local_pos.z) << std::endl;
        }

        base_component.setPos(local_pos + origin);
        base_component.setRot(glm::toQuat(rot_mat));
        base_component.setScale(local_scale);

        co_return;
    }

    lotus::Task<> ParticleComponent::tick(lotus::time_point time, lotus::duration delta)
    {
        if (time > start_time + duration)
        {
            if (generator->header->flags5 & 0x10)
            {
         //       start_time = time;
         //       co_await init();
            }
        //    else
            {
                entity->remove();
                co_return;
            }
        }

        glm::vec3 kf_scale = local_scale;

        float movement = 30.f / ((float)std::chrono::nanoseconds(1s).count() / std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count());
        auto spawn_delta = time - start_time;
        float progress = static_cast<float>(spawn_delta.count()) / static_cast<float>(duration.count());

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
                local_pos += dpos * movement;
            }
                break;
            case 0x03:
            {
                //add to velocity (acceleration)
                dpos += *(glm::vec3*)(command) * movement;
            }
                break;
            case 0x05:
                // apply static rotational changes
            {
                local_rot += drot * movement;
            }
                break;
            case 0x08:
                // apply static scale changes
            {
                local_scale += dscale * movement;
                kf_scale += dscale * movement;
            }
                break;
            case 0x0B:
                // 0x19?
                break;
            case 0x0E:
                // 0x01 model?
                break;
            case 0x0F:
                // 0x21
                break;
            case 0x10:
                // 0x22
                break;
            case 0x11:
                // 0x23
                break;
            case 0x12:
                // 0x24
                break;
            case 0x13:
                // 0x25
                break;
            case 0x14:
                // 0x26
                break;
            case 0x15:
                // 0x27
            {
                kf_scale.x = keyframeScale(local_scale.x, progress, kf_x_scale);
            }
                break;
            case 0x16:
                // 0x28
            {
                kf_scale.y = keyframeScale(local_scale.y, progress, kf_y_scale);
            }
                break;
            case 0x17:
                // 0x29
            {
                kf_scale.z = keyframeScale(local_scale.z, progress, kf_z_scale);
            }
                break;
            case 0x18:
                // 0x2a
            {
                particle_component.color.r = keyframeScale(particle_component.color.r, progress, kf_r);
            }
                break;
            case 0x19:
                // 0x2b
            {
                particle_component.color.g = keyframeScale(particle_component.color.g, progress, kf_g);
            }
                break;
            case 0x1A:
                // 0x2c
            {
                particle_component.color.b = keyframeScale(particle_component.color.b, progress, kf_b);
            }
                break;
            case 0x1B:
                // 0x2d
            {
                particle_component.color.a = keyframeScale(particle_component.color.a, progress, kf_a);
            }
                break;
            case 0x1D:
                // 0x2f
                break;
            case 0x1E:
                // 0x33
                break;
            case 0x1F:
                // 0x34
                break;
            case 0x27:
            {
                particle_component.uv_offset.x += *(float*)(command);
            }
                break;

            case 0x28:
            {
                particle_component.uv_offset.y += *(float*)(command);
            }
                break;

            case 0x2C:
            {
                //multiply dpos per frame
                auto dpos_exp = *(float*)(command);
                dpos *= glm::vec3(std::pow(dpos_exp, movement));
            }
                break;
            default:
            {
                break;
            }
            }
            command += (data_size - 1) * sizeof(uint32_t);
        }
        if (!(pos_flags & 0x80))
        {
            auto parent_render = engine->game->scene->component_runners->getComponent<lotus::Component::RenderBaseComponent>(actor.lock().get());
            origin = parent_render ? parent_render->getPos() : glm::vec3{};
            if (bone)
            {
                origin += bone->trans + (bone->rot * (bone_offset * bone->scale));
            }
        }
        auto rot_mat = glm::eulerAngleXZY(local_rot.x, local_rot.z, local_rot.y);
        base_component.setPos(local_pos + origin);
        base_component.setRot(glm::toQuat(rot_mat));
        base_component.setScale(kf_scale);
        if (light > 0)
        {
            engine->lights->UpdateLight(light, {
                .pos = local_pos + origin,
                .intensity = 100.f,
                .colour = glm::vec3(1.f),
                .radius = 0.05f
                });
        }
        co_return;
    }

    float ParticleComponent::keyframeScale(float in, float progress, std::string keyframe_id)
    {
        auto keyframe = keyframes.at(keyframe_id);
        float prev = keyframe->intervals[0].second;
        float prev_key = keyframe->intervals[0].first;
        float next = keyframe->intervals[1].second;
        float a = 0.f;
        for (const auto& [key, value] : keyframe->intervals)
        {
            next = value;
            if (progress <= key)
            {
                a = key == 0.f ? 0.f : (progress - prev_key) / (key - prev_key);
                break;
            }
            prev_key = key;
            prev = value;
        }
        if (keyframe->intervals.size() == 2)
        {
            return glm::mix(in, next, a);
        }
        else
        {
            return glm::mix(prev, next, a);
        }
    }
}
