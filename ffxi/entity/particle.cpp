#include "particle.h"

#include "engine/scene.h"
#include "engine/entity/component/particle_component.h"
#include "engine/entity/component/particle_raster_component.h"
#include "engine/entity/component/particle_raytrace_component.h"
#include "component/particle_component.h"

lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, std::tuple<>>> FFXIParticle::Init(lotus::Engine* engine, lotus::Scene* scene, std::weak_ptr<lotus::Entity> parent, FFXI::Generator* generator, std::shared_ptr<lotus::Model> model, size_t index)
{
    auto entity = std::make_shared<lotus::Entity>();
    co_await FFXIParticle::Load(entity, engine, scene, parent, generator, model, index);
    co_return std::make_pair(entity, std::tuple<>());
}

lotus::WorkerTask<> FFXIParticle::Load(std::shared_ptr<lotus::Entity> entity, lotus::Engine* engine, lotus::Scene* scene, std::weak_ptr<lotus::Entity> parent, FFXI::Generator* generator, std::shared_ptr<lotus::Model> model, size_t index)
{
    auto p = co_await lotus::Component::RenderBaseComponent::make_component(entity.get(), engine);
    auto pc = co_await lotus::Component::ParticleComponent::make_component(entity.get(), engine, model);
    auto fpc = co_await FFXI::ParticleComponent::make_component(entity.get(), engine, *pc, *p, parent, generator, index);
    auto r = engine->config->renderer.RasterizationEnabled() ? co_await lotus::Component::ParticleRasterComponent::make_component(entity.get(), engine, *pc, *p) : nullptr;
    auto rt = engine->config->renderer.RaytraceEnabled() ? co_await lotus::Component::ParticleRaytraceComponent::make_component(entity.get(), engine, *pc, *p) : nullptr;

    scene->AddComponents(std::move(p), std::move(pc), std::move(fpc), std::move(r), std::move(rt));
}
