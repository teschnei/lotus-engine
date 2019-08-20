#include "scene.h"
#include "entity/renderable_entity.h"
#include "core.h"
#include "renderer/renderer.h"

namespace lotus
{
void Scene::render(Engine* engine)
{
        engine->worker_pool.addWork(std::make_unique<lotus::LambdaWorkItem>([](lotus::WorkerThread* thread) {
            glm::vec3 lightDir = glm::normalize(-glm::vec3{ -25.f, -100.f, -50.f });
            float cascade_splits[lotus::Renderer::shadowmap_cascades];

            float near_clip = thread->engine->camera.getNearClip();
            float far_clip = thread->engine->camera.getFarClip();
            float range = far_clip - near_clip;
            float ratio = far_clip / near_clip;

            for (size_t i = 0; i < lotus::Renderer::shadowmap_cascades; ++i)
            {
                float p = (i + 1) / static_cast<float>(lotus::Renderer::shadowmap_cascades);
                float log = near_clip * std::pow(ratio, p);
                float uniform = near_clip + range * p;
                float d = 0.95f * (log - uniform) + uniform;
                cascade_splits[i] = (d - near_clip) / range;
            }

            float last_split = 0.0f;

            for (size_t i = 0; i < lotus::Renderer::shadowmap_cascades; ++i)
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

                glm::mat4 inverse_camera = glm::inverse(thread->engine->camera.getProjMatrix() * thread->engine->camera.getViewMatrix());

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

                thread->engine->renderer.cascades[i].split_depth = (near_clip + split_dist * range) * -1.f;
                thread->engine->renderer.cascades[i].view_proj_matrix = light_ortho * light_view;

                thread->engine->renderer.cascade_matrices[i] = thread->engine->renderer.cascades[i].view_proj_matrix;
                thread->engine->renderer.cascade_data.cascade_splits[i] = thread->engine->renderer.cascades[i].split_depth;
                thread->engine->renderer.cascade_data.cascade_view_proj[i] = thread->engine->renderer.cascades[i].view_proj_matrix;

                last_split = cascade_splits[i];
            }

            auto data = thread->engine->renderer.device->mapMemory(thread->engine->renderer.cascade_matrices_ubo->memory, thread->engine->renderer.cascade_matrices_ubo->memory_offset, sizeof(thread->engine->renderer.cascade_matrices), {}, thread->engine->renderer.dispatch);
                memcpy(static_cast<uint8_t*>(data) + (thread->engine->renderer.getCurrentImage() * sizeof(thread->engine->renderer.cascade_matrices)), &thread->engine->renderer.cascade_matrices, sizeof(thread->engine->renderer.cascade_matrices));
            thread->engine->renderer.device->unmapMemory(thread->engine->renderer.cascade_matrices_ubo->memory, thread->engine->renderer.dispatch);

            thread->engine->renderer.cascade_data.inverse_view = glm::inverse(thread->engine->camera.getViewMatrix());
            thread->engine->renderer.cascade_data.light_dir = lightDir;
            data = thread->engine->renderer.device->mapMemory(thread->engine->renderer.cascade_data_ubo->memory, thread->engine->renderer.cascade_data_ubo->memory_offset, sizeof(thread->engine->renderer.cascade_data), {}, thread->engine->renderer.dispatch);
                memcpy(static_cast<uint8_t*>(data) + (thread->engine->renderer.getCurrentImage() * sizeof(thread->engine->renderer.cascade_data)), &thread->engine->renderer.cascade_data, sizeof(thread->engine->renderer.cascade_data));
            thread->engine->renderer.device->unmapMemory(thread->engine->renderer.cascade_data_ubo->memory, thread->engine->renderer.dispatch);
            }));
    for (const auto& entity : entities)
    {
        if (auto renderable_entity = std::dynamic_pointer_cast<RenderableEntity>(entity))
        {
            renderable_entity->render(engine, renderable_entity);
        }
    }
}
}

