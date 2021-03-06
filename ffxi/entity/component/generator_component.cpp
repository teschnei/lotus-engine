#include "generator_component.h"

#include "engine/core.h"
#include "engine/game.h"
#include "engine/scene.h"
#include "engine/entity/particle.h"
#include "engine/entity/component/tick_component.h"
#include "engine/entity/component/animation_component.h"
#include "engine/entity/component/particle_parent_component.h"

#include "entity/actor.h"

#include <glm/gtc/random.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/euler_angles.hpp>

GeneratorComponent::GeneratorComponent(lotus::Entity* entity, lotus::Engine* engine, FFXI::Generator* generator, lotus::duration duration, std::shared_ptr<lotus::Particle> parent, glm::vec3 offset) :
    Component(entity, engine), generator(generator), duration(duration), parent(parent), offset(offset), start_time(engine->getSimulationTime())
{
    for (const auto& chunk : generator->parent->children)
    {
        if (auto keyframe = dynamic_cast<FFXI::Keyframe*>(chunk.get()))
        {
            keyframes.insert(std::make_pair(std::string(keyframe->name, 4), keyframe));
        }
    }
}

lotus::Task<> GeneratorComponent::tick(lotus::time_point time, lotus::duration delta)
{
    uint32_t frequency = 1000 * ((generator->header->interval + 1) / 60.f);
    auto next_generation = std::chrono::milliseconds(generated * frequency);

    auto total_generations = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / frequency;

    if (time > start_time + duration && duration > 0ms && generated > total_generations)
    {
        if (componentsEmpty())
        {
            remove = true;
        }
        co_return;
    }
    else if (duration == 0ms && generated > 0)
    {
        if (componentsEmpty())
        {
            remove = true;
        }
        co_return;
    }

    if (time > start_time + next_generation)
    {
        for (int occ = 0; occ < generator->header->occurences + 1; ++occ)
        {
            std::shared_ptr<lotus::Model> model;
            if (generator->effect_type == 0x0B)
            {
                //TODO: review the particle shader and make sure it's compatible with the vertex format of mmbs
                //model = lotus::Model::getModel(generator->id);
            }
            else if (generator->effect_type == 0x0E)
            {
                model = lotus::Model::getModel(generator->id + "_d3a");
            }
            /*
            //not sure yet what 0x1D is
            else if (generator->effect_type == 0x1D)
            {
                model = lotus::Model::getModel(generator->id);
            }
            //0x3D is sound effect
            else if (generator->effect_type == 0x3D)
            {}
            */
            else if (generator->effect_type == 0x24)
            {
                if (!generator->ring)
                {
                    auto [new_model, model_task] = lotus::Model::LoadModel(std::string(generator->name, 4) + "_" + generator->id,
                        FFXI::D3MLoader::LoadModelRing, engine, std::move(generator->ring_vertices), std::move(generator->ring_indices));
                    generator->ring = new_model;
                    co_await *model_task;
                }
                model = generator->ring;
            }
            if (model)
            {
                auto particle_lifetime = std::chrono::milliseconds(((long long)((long long)generator->lifetime * (1000 / 60.f))));
                auto particle = co_await engine->game->scene->AddEntity<lotus::Particle>(particle_lifetime, model);

                lotus::Skeleton::Bone* bone = nullptr;
                glm::vec3 bone_offset{};
                //TODO: 90% sure this should be generator->header->flags1 & 0x02?
                if (generator->header->flags2 == 0x80 && generator->header->flags3 == 0x08)
                {
                    if (auto bone_id = generator->header->bone_point >> 2; bone_id < FFXI::SK2::GeneratorPointMax)
                    {
                        auto& bone_point = (static_cast<Actor*>(entity)->generator_points[bone_id]);
                        auto actor = static_cast<Actor*>(entity);
                        bone = &actor->animation_component->skeleton->bones[bone_point.bone_index];
                        bone_offset = bone_point.offset;
                    }
                }
                else if (generator->header->flags1 & 0x01)
                {
                    if (auto bone_id = (generator->header->flags1 + ((generator->header->bone_point & 0x01) << 8)) >> 4; bone_id < FFXI::SK2::GeneratorPointMax)
                    {
                        auto& bone_point = (static_cast<Actor*>(entity)->generator_points[bone_id]);
                        auto actor = static_cast<Actor*>(entity);
                        bone = &actor->animation_component->skeleton->bones[bone_point.bone_index];
                        bone_offset = bone_point.offset;
                    }
                }

                if (generator->billboard > 0)
                {
                    if (generator->billboard & 0x1)
                        particle->billboard = lotus::Particle::Billboard::All;
                    if (generator->billboard & 0x4000)
                        particle->billboard = lotus::Particle::Billboard::Y;
                }

                float scale_all_fluctuation = lotus::random::GetRandomNumber(0.f, generator->scale_all_fluctuation);
                particle->base_scale = glm::vec3(lotus::random::GetRandomNumber(generator->scale.x, generator->scale.x + generator->scale_fluctuation.x),
                    lotus::random::GetRandomNumber(generator->scale.y, generator->scale.y + generator->scale_fluctuation.y),
                    lotus::random::GetRandomNumber(generator->scale.z, generator->scale.z + generator->scale_fluctuation.z));
                particle->base_rot = glm::vec3(lotus::random::GetRandomNumber(generator->rot.x, generator->rot.x + generator->rot_fluctuation.x),
                    lotus::random::GetRandomNumber(generator->rot.y, generator->rot.y + generator->rot_fluctuation.y),
                    lotus::random::GetRandomNumber(generator->rot.z, generator->rot.z + generator->rot_fluctuation.z)) + gen_rot_add;
                //the offset3 parameters are probably a better way of actually doing all this - they seem to basically be instructions
                float gen_radius = lotus::random::GetRandomNumber(generator->gen_radius, generator->gen_radius + generator->gen_radius_fluctuation);
                float gen_height = lotus::random::GetRandomNumber(generator->gen_height - generator->gen_height_fluctuation, generator->gen_height + generator->gen_height_fluctuation);
                if (gen_radius > 0 || gen_height > 0)
                {
                    glm::vec3 cylinder{};
                    float cylinder_rot = 0.f;
                    if (generator->rotations > 0)
                    {
                        cylinder_rot = gen_rot_add_cylinder;
                        gen_rot_add_cylinder += (glm::pi<float>() * 2) / (generator->rotations + 1);
                    }
                    else
                    {
                        cylinder_rot = lotus::random::GetRandomNumber(0.f, glm::pi<float>() * 2);
                    }
                    cylinder.x = gen_radius;
                    cylinder = glm::rotateY(cylinder, cylinder_rot);
                    cylinder.y = lotus::random::GetRandomNumber(0.f, gen_height);
                    cylinder *= generator->gen_multi;
                    cylinder = glm::rotateX(cylinder, generator->gen_axis_rot.y);
                    cylinder = glm::rotateZ(cylinder, generator->gen_axis_rot.x);

                    particle->base_pos += cylinder;

                    if (particle->billboard == lotus::Particle::Billboard::None)
                    {
                        auto rot_mat = glm::eulerAngleXZY(particle->base_rot.x, particle->base_rot.z, particle->base_rot.y);
                        auto gen_rot_mat = glm::eulerAngleXZY(0.f, 0.f, cylinder_rot);
                        auto final_rot_mat = gen_rot_mat * rot_mat;
                        glm::extractEulerAngleXZY(final_rot_mat, particle->base_rot.x, particle->base_rot.z, particle->base_rot.y);
                    }
                }
                float sphere_radius = lotus::random::GetRandomNumber(generator->gen_radius_sphere, generator->gen_radius_sphere + generator->gen_radius_sphere_fluctuation);
                if (sphere_radius > 0)
                    particle->base_pos += glm::sphericalRand(sphere_radius);
                particle->base_pos += generator->pos;

                if (scale_all_fluctuation > 0)
                {
                    particle->base_scale += glm::vec3(scale_all_fluctuation);
                }

                glm::vec3 origin{};
                if (generator->pos_flags & 0x80)
                {
                    origin = entity->getPos();
                }
                else
                {
                    if (bone)
                    {
                        origin = bone->trans + (bone->rot * (bone_offset * bone->scale));
                        particle->addComponent<lotus::ParticleParentComponent>(std::static_pointer_cast<lotus::RenderableEntity>(entity->getSharedPtr()), lotus::ParticleParentComponent::FollowType::FollowParentAll);
                    }
                    else
                    {
                        particle->addComponent<lotus::ParticleParentComponent>(std::static_pointer_cast<lotus::RenderableEntity>(entity->getSharedPtr()), lotus::ParticleParentComponent::FollowType::FollowParentPos);
                    }
                }

                if (parent)
                {
                    offset = parent->getPos() - origin;
                }

                gen_rot_add += generator->gen_rot_add;
                particle->color = glm::vec4{ (generator->color & 0xFF) / 255.f, ((generator->color & 0xFF00) >> 8) / 255.0, ((generator->color & 0xFF0000) >> 16) / 255.0, ((generator->color & 0xFF000000) >> 24) / 128.0 };
                glm::vec3 movement_per_frame = glm::vec3(lotus::random::GetRandomNumber(generator->dpos.x, generator->dpos.x + generator->dpos_fluctuation.x),
                    lotus::random::GetRandomNumber(generator->dpos.y, generator->dpos.y + generator->dpos_fluctuation.y),
                    lotus::random::GetRandomNumber(generator->dpos.z, generator->dpos.z + generator->dpos_fluctuation.z));
                glm::vec3 rot_per_frame = glm::vec3(lotus::random::GetRandomNumber(generator->drot.x, generator->drot.x + generator->drot_fluctuation.x),
                    lotus::random::GetRandomNumber(generator->drot.y, generator->drot.y + generator->drot_fluctuation.y),
                    lotus::random::GetRandomNumber(generator->drot.z, generator->drot.z + generator->drot_fluctuation.z));
                glm::vec3 scale_per_frame = glm::vec3(lotus::random::GetRandomNumber(generator->dscale.x, generator->dscale.x + generator->dscale_fluctuation.x),
                    lotus::random::GetRandomNumber(generator->dscale.y, generator->dscale.y + generator->dscale_fluctuation.y),
                    lotus::random::GetRandomNumber(generator->dscale.z, generator->dscale.z + generator->dscale_fluctuation.z));

                if (generator->dpos_origin > 0 && particle->base_pos.length() > 0)
                {
                    auto origin_dir = glm::normalize(particle->base_pos);
                    movement_per_frame += origin_dir * generator->dpos_origin;
                }

                particle->setPos(particle->base_pos + origin + offset);
                particle->setRot(particle->base_rot);
                particle->setScale(particle->base_scale);

                if (!generator->sub_generator.empty())
                {
                    //assume sub generator is in same place as generator
                    for (const auto& chunk : generator->parent->children)
                    {
                        if (auto sub_generator = dynamic_cast<FFXI::Generator*>(chunk.get()); sub_generator && std::string(chunk->name, 4) == generator->sub_generator)
                        {
                            addComponent<GeneratorComponent>(sub_generator, particle_lifetime, particle);
                            break;
                        }
                    }
                }

                auto particle_pointer = particle.get();
                auto entity_pointer = entity;
                auto& kf_map = this->keyframes;
                auto generator = this->generator;
                auto offset = this->offset;
                auto colour = particle->color;
                auto particle_tick = [particle_pointer, generator, movement_per_frame, rot_per_frame, scale_per_frame, kf_map, entity_pointer, bone, bone_offset, origin, offset, colour](lotus::Engine* engine, lotus::time_point current_time, lotus::duration delta)
                {
                    //not sure if the 60fps base is constant for every generator or set somewhere
                    auto spawn_delta = current_time - particle_pointer->getSpawnTime();
                    float frames = 60.f / ((float)std::chrono::nanoseconds(1s).count() / std::chrono::duration_cast<std::chrono::nanoseconds>(spawn_delta).count());
                    float dpos_exp = std::pow(generator->dpos_exp, frames);

                    float movement = 60.f / ((float)std::chrono::nanoseconds(1s).count() / std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count());
                    particle_pointer->base_pos += movement * (movement_per_frame + (frames * generator->dpos_acceleration)) * dpos_exp;
                    particle_pointer->base_rot += movement * rot_per_frame;
                    particle_pointer->base_scale += movement * scale_per_frame;

                    particle_pointer->uv_offset += movement * generator->duv;

                    glm::vec3 new_origin{};

                    if (generator->pos_flags & 0x80)
                    {
                        //don't follow bone/entity
                        new_origin = origin + offset;
                    }
                    else
                    {
                        new_origin = offset;
                        if (bone)
                        {
                            new_origin += bone->trans + (bone->rot * (bone_offset * bone->scale));
                        }
                    }

                    auto lifetime = static_cast<float>(particle_pointer->getLifetime().count());

                    glm::vec3 kf_pos{ 0 };
                    glm::vec3 kf_rot{ 0 };
                    glm::vec3 kf_scale = particle_pointer->base_scale;

                    if (lifetime > 0)
                    {
                        float progress = static_cast<float>(spawn_delta.count()) / lifetime;

                        if (progress > 1.0)
                        {
                            if (!generator->end_generator.empty())
                            {
                                //assume end generator is in same place as generator
                                for (const auto& chunk : generator->parent->children)
                                {
                                    if (auto end_generator = dynamic_cast<FFXI::Generator*>(chunk.get()); end_generator && std::string(chunk->name, 4) == generator->end_generator)
                                    {
                                        entity_pointer->addComponent<GeneratorComponent>(end_generator, 0ms, nullptr, particle_pointer->base_pos);
                                        break;
                                    }
                                }
                            }
                            return true;
                        }

                        if (!generator->kf_x_pos.empty())
                        {
                            auto keyframe = kf_map.at(generator->kf_x_pos);
                            float prev = keyframe->intervals[0].second;
                            float next = keyframe->intervals[1].second;
                            for (const auto& [key, value] : keyframe->intervals)
                            {
                                next = value;
                                if (progress > key)
                                {
                                    break;
                                }
                                prev = value;
                            }
                        }
                        if (!generator->kf_y_pos.empty())
                        {

                        }
                        if (!generator->kf_z_pos.empty())
                        {

                        }
                        if (!generator->kf_x_rot.empty())
                        {

                        }
                        if (!generator->kf_y_rot.empty())
                        {

                        }
                        if (!generator->kf_z_rot.empty())
                        {

                        }
                        if (!generator->kf_x_scale.empty())
                        {
                            auto keyframe = kf_map.at(generator->kf_x_scale);
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
                                kf_scale.x = glm::mix(kf_scale.x, next, a);
                            }
                            else
                            {
                                kf_scale.x = glm::mix(prev, next, a);
                            }
                        }
                        if (!generator->kf_y_scale.empty())
                        {
                            auto keyframe = kf_map.at(generator->kf_y_scale);
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
                                kf_scale.y = glm::mix(kf_scale.y, next, a);
                            }
                            else
                            {
                                kf_scale.y = glm::mix(prev, next, a);
                            }
                        }
                        if (!generator->kf_z_scale.empty())
                        {
                            auto keyframe = kf_map.at(generator->kf_z_scale);
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
                                kf_scale.z = glm::mix(kf_scale.z, next, a);
                            }
                            else
                            {
                                kf_scale.z = glm::mix(prev, next, a);
                            }
                        }
                        if (!generator->kf_r.empty())
                        {

                        }
                        if (!generator->kf_g.empty())
                        {

                        }
                        if (!generator->kf_b.empty())
                        {

                        }
                        if (!generator->kf_a.empty())
                        {
                            auto keyframe = kf_map.at(generator->kf_a);
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
                                particle_pointer->color.a = glm::mix(colour.a, next, a);
                            }
                            else
                            {
                                particle_pointer->color.a = glm::mix(prev, next, a);
                            }
                        }
                        if (!generator->kf_u.empty())
                        {

                        }
                        if (!generator->kf_v.empty())
                        {

                        }

                    }

                    particle_pointer->setPos(particle_pointer->base_pos + new_origin);
                    particle_pointer->setRot(particle_pointer->base_rot + kf_rot);
                    particle_pointer->setScale(kf_scale);
                    return false;
                };
                particle_tick(engine, time, 0s);
                co_await particle->addComponent<lotus::TickComponent>(particle_tick);
            }
        }
        generated++;
    }
}
