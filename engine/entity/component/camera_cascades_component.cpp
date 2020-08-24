#include "camera_cascades_component.h"
#include "engine/entity/camera.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/raster/renderer_rasterization.h"

namespace lotus
{
    CameraCascadesComponent::CameraCascadesComponent(Entity* _entity, Engine* _engine) : Component(_entity, _engine)
    {
    }

    void CameraCascadesComponent::render(Engine* engine, std::shared_ptr<Entity>& sp)
    {
        if (static_cast<Camera*>(entity)->updated())
        {
            auto camera = std::static_pointer_cast<Camera>(sp);
            engine->worker_pool->addWork(std::make_unique<LambdaWorkItem>([camera, engine](WorkerThread* thread)
            {
                auto renderer = static_cast<RendererRasterization*>(thread->engine->renderer.get());
                glm::vec3 lightDir = thread->engine->lights->light.diffuse_dir;
                float cascade_splits[lotus::RendererRasterization::shadowmap_cascades];

                float near_clip = camera->getNearClip();
                float far_clip = camera->getFarClip();
                float range = far_clip - near_clip;
                float ratio = far_clip / near_clip;

                for (size_t i = 0; i < lotus::RendererRasterization::shadowmap_cascades; ++i)
                {
                    float p = (i + 1) / static_cast<float>(lotus::RendererRasterization::shadowmap_cascades);
                    float log = near_clip * std::pow(ratio, p);
                    float uniform = near_clip + range * p;
                    float d = 0.95f * (log - uniform) + uniform;
                    cascade_splits[i] = (d - near_clip) / range;
                }

                float last_split = 0.0f;

                for (uint32_t i = 0; i < lotus::RendererRasterization::shadowmap_cascades; ++i)
                {
                    float split_dist = cascade_splits[i];
                    std::array<glm::vec3, 8> frustum_corners = {
                        glm::vec3{-1.f, 1.f, -1.f},
                        glm::vec3{1.f, 1.f, -1.f},
                        glm::vec3{1.f, -1.f, -1.f},
                        glm::vec3{-1.f, -1.f, -1.f},
                        glm::vec3{-1.f, 1.f, 1.f},
                        glm::vec3{1.f, 1.f, 1.f},
                        glm::vec3{1.f, -1.f, 1.f},
                        glm::vec3{-1.f, -1.f, 1.f}
                    };

                    glm::mat4 inverse_camera = glm::inverse(camera->getProjMatrix() * camera->getViewMatrix());

                    for (auto& corner : frustum_corners)
                    {
                        glm::vec4 inverse_corner = inverse_camera * glm::vec4{ corner, 1.f };
                        corner = inverse_corner / inverse_corner.w;
                    }

                    for (size_t i = 0; i < 4; ++i)
                    {
                        glm::vec3 distance = frustum_corners[i + 4] - frustum_corners[i];
                        frustum_corners[i + 4] = frustum_corners[i] + (distance * split_dist);
                        frustum_corners[i] = frustum_corners[i] + (distance * last_split);
                    }

                    glm::vec3 center = glm::vec3{ 0.f };
                    for (auto& corner : frustum_corners)
                    {
                        center += corner;
                    }
                    center /= 8.f;

                    float radius = 0.f;

                    for (auto& corner : frustum_corners)
                    {
                        float distance = glm::length(corner - center);
                        radius = glm::max(radius, distance);
                    }
                    radius = std::ceil(radius * 16.f) / 16.f;

                    glm::vec3 max_extents = glm::vec3(radius);
                    glm::vec3 min_extents = -max_extents;

                    glm::mat4 light_view = glm::lookAt(center - lightDir * -min_extents.z, center, glm::vec3{ 0.f, -1.f, 0.f });
                    glm::mat4 light_ortho = glm::ortho(min_extents.x, max_extents.x, min_extents.y, max_extents.y, min_extents.z * 2, max_extents.z * 2);
                    light_ortho[1][1] *= -1;

                    renderer->cascade_data.cascade_splits[i] =  (near_clip + split_dist * range) * -1.f;
                    renderer->cascade_data.cascade_view_proj[i] = light_ortho * light_view;

                    last_split = cascade_splits[i];
                }
                renderer->cascade_data.inverse_view = glm::inverse(camera->getViewMatrix());
            }));
        }
    }
}
