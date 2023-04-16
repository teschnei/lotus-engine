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
#include "component/actor_skeleton_component.h"

namespace lotus
{
    class Scene;
}

namespace FFXI
{
    class Dat;
    class ActorSkeletonStatic;
}

//main FFXI entity class
class Actor
{
public:
    using InitComponents = std::tuple<lotus::Component::AnimationComponent*, lotus::Component::RenderBaseComponent*, lotus::Component::DeformedMeshComponent*,
                                      lotus::Component::DeformableRasterComponent*, lotus::Component::DeformableRaytraceComponent*, FFXI::ActorComponent*, FFXI::ActorSkeletonComponent*>;
    static lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, InitComponents>> Init(lotus::Engine* engine, lotus::Scene* scene, uint16_t modelid);
    using InitPCComponents = std::tuple<lotus::Component::AnimationComponent*, lotus::Component::RenderBaseComponent*, lotus::Component::DeformedMeshComponent*,
                                      lotus::Component::DeformableRasterComponent*, lotus::Component::DeformableRaytraceComponent*, FFXI::ActorComponent*, FFXI::ActorSkeletonComponent*>;
    static lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, InitPCComponents>> Init(lotus::Engine* engine, lotus::Scene* scene, FFXI::ActorSkeletonComponent::LookData look);
    static size_t GetPCModelDatID(uint16_t modelid, uint8_t race);
protected:
    static lotus::WorkerTask<InitComponents> Load(std::shared_ptr<lotus::Entity> entity, lotus::Engine* engine, lotus::Scene* scene,
        std::shared_ptr<const FFXI::ActorSkeletonStatic>, std::variant<FFXI::ActorSkeletonComponent::LookData, uint16_t>, std::vector<std::reference_wrapper<const FFXI::Dat>> dats);
    //static lotus::WorkerTask<std::tuple<>> LoadPC(std::shared_ptr<lotus::Entity> entity, lotus::Engine* engine, lotus::Scene* scene,
    //    InitComponents components, FFXI::ActorPCModelsComponent::LookData look, std::initializer_list<std::reference_wrapper<const FFXI::Dat>> dats);
};
