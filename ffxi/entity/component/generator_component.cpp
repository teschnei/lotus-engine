#include "generator_component.h"

#include "engine/core.h"
#include "engine/game.h"
#include "engine/scene.h"
#include "engine/entity/particle.h"
#include "engine/entity/component/tick_component.h"

#include <glm/gtx/rotate_vector.hpp>

GeneratorComponent::GeneratorComponent(lotus::Entity* entity, lotus::Engine* engine, FFXI::Generator* generator, lotus::duration duration) :
    Component(entity, engine), generator(generator), duration(duration), start_time(engine->getSimulationTime())
{
    for (const auto& chunk : generator->parent->children)
    {
        if (auto keyframe = dynamic_cast<FFXI::Keyframe*>(chunk.get()))
        {
            keyframes.insert(std::make_pair(std::string(keyframe->name, 4), keyframe));
        }
    }
}

void GeneratorComponent::tick(lotus::time_point time, lotus::duration delta)
{
    if (time > start_time + duration)
    {
        remove = true;
        return;
    }
    uint32_t frequency = 1000 * ((generator->header->interval + 1) / 60.f);
    auto next_generation = std::chrono::milliseconds(generated * frequency);

    if (time > start_time + next_generation)
    {
        auto particle = engine->game->scene->AddEntity<lotus::Particle>(std::chrono::milliseconds((long long)((long long)generator->lifetime * (1000 / 30.f))));
        auto pos = glm::vec3(0);
        pos.x = lotus::random::GetRandomNumber(generator->gen_radius, generator->gen_radius + generator->gen_radius_fluctuation);
        //TODO: rotation (instead of randomly, spins around rotation+1 times per frame)
        pos = glm::rotateY(pos, lotus::random::GetRandomNumber(0.f, glm::pi<float>() * 2));
        pos.y -= lotus::random::GetRandomNumber(generator->gen_height, generator->gen_height + generator->gen_height_fluctuation);
        particle->billboard = generator->billboard == 1;
        auto model = lotus::Model::getModel(generator->id);
        if (!model)
            model = lotus::Model::getModel("rin ");
        particle->models.push_back(std::move(model));
        particle->setScale(generator->scale);
        particle->setPos(pos + entity->getPos());
        particle->setRot(generator->rot + gen_rot_add);
        gen_rot_add += generator->gen_rot_add;
        particle->color = glm::vec4{ (generator->color & 0xFF) / 255.f, ((generator->color & 0xFF00) >> 8) / 255.0, ((generator->color & 0xFF0000) >> 16) / 255.0, ((generator->color & 0xFF000000) >> 24) / 255.0 };
        glm::vec3 movement_per_frame = glm::vec3(lotus::random::GetRandomNumber(generator->dpos.x, generator->dpos.x + generator->dpos_fluctuation.x),
            lotus::random::GetRandomNumber(generator->dpos.y, generator->dpos.y + generator->dpos_fluctuation.y),
            lotus::random::GetRandomNumber(generator->dpos.z, generator->dpos.z + generator->dpos_fluctuation.z));
        glm::vec3 rot_per_frame = glm::vec3(lotus::random::GetRandomNumber(generator->drot.x, generator->drot.x + generator->drot_fluctuation.x),
            lotus::random::GetRandomNumber(generator->drot.y, generator->drot.y + generator->drot_fluctuation.y),
            lotus::random::GetRandomNumber(generator->drot.z, generator->drot.z + generator->drot_fluctuation.z));
        auto particle_pointer = particle.get();
        auto& kf_map = this->keyframes;
        auto generator = this->generator;
        auto particle_tick = [particle_pointer, generator, movement_per_frame, rot_per_frame, kf_map](lotus::time_point current_time, lotus::duration delta)
        {
            float movement = 30.f / ((float)std::chrono::nanoseconds(1s).count() / std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count());
            particle_pointer->setPos(particle_pointer->getPos() + (movement * movement_per_frame));
            particle_pointer->setRot(particle_pointer->getRotEuler() + (movement * rot_per_frame));

            auto spawn_delta = current_time - particle_pointer->getSpawnTime();

            float progress = static_cast<float>(spawn_delta.count()) / static_cast<float>(particle_pointer->getLifetime().count());

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
                auto scale = particle_pointer->getScale();
                scale.x = glm::mix(prev, next, a);
                particle_pointer->setScale(scale);
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
                auto scale = particle_pointer->getScale();
                scale.y = glm::mix(prev, next, a);
                particle_pointer->setScale(scale);
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
                auto scale = particle_pointer->getScale();
                scale.z = glm::mix(prev, next, a);
                particle_pointer->setScale(scale);
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
            }
            if (!generator->kf_u.empty())
            {

            }
            if (!generator->kf_v.empty())
            {

            }
        };
        particle_tick(time, 0s);
        particle->addComponent<lotus::TickComponent>(particle_tick);
        generated++;
    }
}
