#include "system_dat.h"

#include "ffxi.h"

#include "dat/dat.h"
#include "dat/scheduler.h"
#include "dat/generator.h"
#include "dat/d3m.h"
#include "dat/dxt3.h"

SystemDat::SystemDat(FFXIGame* _game, _private_tag) : game(_game) {}

lotus::Task<std::unique_ptr<SystemDat>> SystemDat::Load(FFXIGame* game)
{
    const auto& ffxi_dat = game->dat_loader->GetDat(0);
    auto dat = std::make_unique<SystemDat>(game, _private_tag{});

    co_await dat->ParseDir(ffxi_dat.root.get());

    co_return std::move(dat);
}

lotus::Task<> SystemDat::ParseDir(FFXI::DatChunk* chunk)
{
    std::vector<lotus::Task<std::shared_ptr<lotus::Texture>>> texture_tasks;
    std::vector<lotus::Task<>> model_tasks;

    //not sure if by design or not, but it seems like all the subitems don't have any overlapping names
    for (const auto& child_chunk : chunk->children)
    {
        if (auto scheduler = dynamic_cast<FFXI::Scheduler*>(child_chunk.get()))
        {
            schedulers.insert({ std::string(child_chunk->name, 4), scheduler });
        }
        else if (auto generator = dynamic_cast<FFXI::Generator*>(child_chunk.get()))
        {
            generators.insert({ std::string(child_chunk->name, 4), generator });
        }
        else if (auto dxt3 = dynamic_cast<FFXI::DXT3*>(child_chunk.get()))
        {
            if (dxt3->width > 0)
            {
                texture_tasks.push_back(lotus::Texture::LoadTexture(dxt3->name, FFXI::DXT3Loader::LoadTexture, game->engine.get(), dxt3));
            }
        }
        else if (auto keyframe = dynamic_cast<FFXI::Keyframe*>(child_chunk.get()))
        {
            keyframes.insert({ std::string(keyframe->name, 4), keyframe });
        }
    }

    for (const auto& task : texture_tasks)
    {
        co_await task;
    }

    for (const auto& child_chunk : chunk->children)
    {
        if (auto d3m = dynamic_cast<FFXI::D3M*>(child_chunk.get()))
        {
            auto [model, model_task] = lotus::Model::LoadModel(std::string(d3m->name, 4), FFXI::D3MLoader::LoadD3M, game->engine.get(), d3m);
            generator_models.push_back(model);
            if (model_task)
                model_tasks.push_back(std::move(*model_task));
        }
        else if (auto d3a = dynamic_cast<FFXI::D3A*>(child_chunk.get()))
        {
            auto [model, model_task] = lotus::Model::LoadModel(std::string(d3a->name, 4) + "_d3a", FFXI::D3MLoader::LoadD3A, game->engine.get(), d3a);
            generator_models.push_back(model);
            if (model_task)
                model_tasks.push_back(std::move(*model_task));
        }
    }

    for (const auto& task : model_tasks)
    {
        co_await task;
    }

    for (const auto& child_chunk : chunk->children)
    {
        co_await ParseDir(child_chunk.get());
    }
}
