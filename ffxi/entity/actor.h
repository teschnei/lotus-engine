#pragma once
#include "engine/entity/entity.h"
#include "engine/task.h"
#include "dat/sk2.h"
#include "dat/scheduler.h"
#include "dat/generator.h"
#include "engine/entity/component/animation_component.h"
#include "engine/entity/component/render_base_component.h"
#include "engine/entity/component/deformable_raster_component.h"
#include "engine/entity/component/deformable_raytrace_component.h"
#include "component/actor_component.h"
#include "component/actor_pc_models_component.h"

namespace lotus
{
    class Scene;
}

namespace FFXI
{
    class Dat;
}

//main FFXI entity class
class Actor
{
public:
    using InitComponents = std::tuple<lotus::Component::AnimationComponent*, lotus::Component::RenderBaseComponent*, lotus::Component::DeformedMeshComponent*,
                                      lotus::Component::DeformableRasterComponent*, lotus::Component::DeformableRaytraceComponent*, FFXI::ActorComponent*>;
    static lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, InitComponents>> Init(lotus::Engine* engine, lotus::Scene* scene, size_t modelid);
    using InitPCComponents = std::tuple<lotus::Component::AnimationComponent*, lotus::Component::RenderBaseComponent*, lotus::Component::DeformedMeshComponent*,
                                      lotus::Component::DeformableRasterComponent*, lotus::Component::DeformableRaytraceComponent*, FFXI::ActorComponent*, FFXI::ActorPCModelsComponent*>;
    static lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, InitPCComponents>> Init(lotus::Engine* engine, lotus::Scene* scene, FFXI::ActorPCModelsComponent::LookData look);
    static size_t GetPCModelDatID(uint16_t modelid, uint8_t race);
protected:
    static std::vector<size_t> GetPCSkeletonDatIDs(uint8_t race);

    static lotus::WorkerTask<InitComponents> Load(std::shared_ptr<lotus::Entity> entity, lotus::Engine* engine, lotus::Scene* scene, std::initializer_list<std::reference_wrapper<const FFXI::Dat>> dats);
};
