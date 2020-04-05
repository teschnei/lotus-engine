#include "particle_tester.h"

#include <glm/gtx/rotate_vector.hpp>

#include "engine/core.h"
#include "engine/game.h"
#include "engine/entity/particle.h"

#include "config.h"
#include "dat/generator.h"
#include "dat/d3m.h"
#include "dat/dxt3.h"

#include "engine/entity/component/tick_component.h"

ParticleTester::ParticleTester(lotus::Engine* _engine) : engine(_engine),
    parser_system( static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path + R"(\ROM\0\0.dat)", engine->renderer.RTXEnabled() ),
    parser( static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path + R"(\ROM\10\9.dat)", engine->renderer.RTXEnabled() )
{
    std::unordered_map<std::string, std::shared_ptr<lotus::Texture>> texture_map;

    for (const auto& chunk : parser.root->children)
    {
        if (auto generator = dynamic_cast<FFXI::Generator*>(chunk.get()))
        {
            generators.insert(std::make_pair(std::string(generator->name, 4), generator));
        }
        else if (auto dxt3 = dynamic_cast<FFXI::DXT3*>(chunk.get()))
        {
            if (dxt3->width > 0)
            {
                auto texture = lotus::Texture::LoadTexture<FFXI::DXT3Loader>(engine, dxt3->name, dxt3);
                texture_map[dxt3->name] = std::move(texture);
            }
        }
        else if (auto keyframe = dynamic_cast<FFXI::Keyframe*>(chunk.get()))
        {
            keyframes.insert(std::make_pair(std::string(keyframe->name, 4), keyframe));
        }
    }
    for (const auto& chunk : parser.root->children)
    {
        if (auto d3m = dynamic_cast<FFXI::D3M*>(chunk.get()))
        {
            models.push_back(lotus::Model::LoadModel<FFXI::D3MLoader>(engine, std::string(d3m->name, 4), d3m));
        }
    }
    last = lotus::sim_clock::now();
}

void ParticleTester::tick(lotus::time_point time, lotus::time_point::duration delta)
{
    if (time - last > 500ms)
    {
        last = time;
        CreateParticle();
    }
}

void ParticleTester::CreateParticle()
{
    auto generator = generators["kr30"];
    auto particle = engine->game->scene->AddEntity<lotus::Particle>(std::chrono::milliseconds((long long)((long long)generator->lifetime * (1000 / 30.f))));
    auto pos = glm::vec3(0);
    pos.x = lotus::random::GetRandomNumber(generator->gen_radius, generator->gen_radius + generator->gen_radius_fluctuation);
    pos = glm::rotateY(pos, lotus::random::GetRandomNumber(0.f, glm::pi<float>() * 2));
    pos.y -= lotus::random::GetRandomNumber(generator->gen_height, generator->gen_height + generator->gen_height_fluctuation);
    particle->billboard = generator->billboard == 1;
    auto model = lotus::Model::getModel(generator->id);
    if (!model)
        model = lotus::Model::getModel("rin ");
    particle->models.push_back(std::move(model));
    particle->setScale(generator->scale);
    particle->setPos(pos + glm::vec3(259.f, -87.f, 99.f));
    particle->setRot(generator->rot);
    particle->color = glm::vec4{ (generator->color & 0xFF) / 255.f, ((generator->color & 0xFF00) >> 8) / 255.0, ((generator->color & 0xFF0000) >> 16) / 255.0, ((generator->color & 0xFF000000) >> 24) / 255.0 };
    glm::vec3 movement_per_frame = glm::vec3(lotus::random::GetRandomNumber(generator->dpos.x, generator->dpos.x + generator->dpos_fluctuation.x),
        lotus::random::GetRandomNumber(generator->dpos.y, generator->dpos.y + generator->dpos_fluctuation.y),
        lotus::random::GetRandomNumber(generator->dpos.z, generator->dpos.z + generator->dpos_fluctuation.z));
    glm::vec3 rot_per_frame = glm::vec3(lotus::random::GetRandomNumber(generator->drot.x, generator->drot.x + generator->drot_fluctuation.x),
        lotus::random::GetRandomNumber(generator->drot.y, generator->drot.y + generator->drot_fluctuation.y),
        lotus::random::GetRandomNumber(generator->drot.z, generator->drot.z + generator->drot_fluctuation.z));
    auto particle_pointer = particle.get();
    auto& kf_map = this->keyframes;
    particle->addComponent<lotus::TickComponent>([particle_pointer, generator, movement_per_frame, rot_per_frame, kf_map](lotus::time_point current_time, lotus::duration delta)
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
            auto color = particle_pointer->color;
            particle_pointer->color.a = glm::mix(prev, next, a);
        }
        if (!generator->kf_u.empty())
        {

        }
        if (!generator->kf_v.empty())
        {

        }
    });
}
