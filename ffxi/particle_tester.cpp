#include "particle_tester.h"

#include "engine/core.h"
#include "engine/game.h"
#include "engine/entity/particle.h"

#include "config.h"
#include "dat/dat_parser.h"
#include "dat/generator.h"
#include "dat/d3m.h"
#include "dat/dxt3.h"

ParticleTester::ParticleTester(lotus::Engine* _engine) : engine(_engine)
{
    auto path = static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path;
    FFXI::DatParser parser{ path + R"(\ROM\10\9.dat)", engine->renderer.RTXEnabled() };

    std::unordered_map<std::string, std::shared_ptr<lotus::Texture>> texture_map;

    for (const auto& chunk : parser.root->children)
    {
        if (auto generator = dynamic_cast<FFXI::Generator*>(chunk.get()))
        {

        }
        else if (auto dxt3 = dynamic_cast<FFXI::DXT3*>(chunk.get()))
        {
            if (dxt3->width > 0)
            {
                auto texture = lotus::Texture::LoadTexture<FFXI::DXT3Loader>(engine, dxt3->name, dxt3);
                texture_map[dxt3->name] = std::move(texture);
            }
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
    if (time - last > 5s)
    {
        last = time;
        CreateParticle();
    }
}

void ParticleTester::CreateParticle()
{
    auto particle = engine->game->scene->AddEntity<lotus::Particle>(5s);
    particle->billboard = true;
    particle->models.push_back(lotus::Model::getModel("kir1"));
    particle->setPos(glm::vec3(259.f, -88.f, 99.f));
}
