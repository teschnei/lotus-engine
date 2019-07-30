#include "ffxi_load_land_test.h"

#include "core/game.h"

#include <fstream>
#include "mzb.h"
#include "mmb.h"
#include "core/task/model_init.h"
#include "core/entity/landscape_entity.h"
#include <map>
#include "core/task/landscape_entity_init.h"
#include "../test_loader.h"

#pragma pack(push,1)
typedef struct 
{
  unsigned int  id;
  long   type:7;
  long   next:19;
  long   is_shadow:1;
  long   is_extracted:1;
  long   ver_num:3;
  long   is_virtual:1;
  unsigned int 	parent;
  unsigned int  child;
} DATHEAD;
#pragma pack(pop)



FFXILoadLandTest::FFXILoadLandTest(lotus::Game* _game) : game(_game)
{
    std::ifstream dat{R"(E:\Apps\SteamLibrary\SteamApps\common\ffxi\SquareEnix\FINAL FANTASY XI\ROM\342\73.dat)", std::ios::ate | std::ios::binary };

    size_t fileSize = (size_t) dat.tellg();
    std::vector<uint8_t> buffer(fileSize);

    dat.seekg(0);
    dat.read((char*)buffer.data(), fileSize);
    dat.close();

    int MZB = 0;
    int MMB = 0;
    int IMG = 0;
    int BONE = 0;
    int ANIM = 0;
    int VERT = 0;

    int offset = 0;
    while(offset < buffer.size())
    {
        DATHEAD* dathead = (DATHEAD*)&buffer[offset];
        int len = (dathead->next & 0x7ffff) * 16;

        switch (dathead->type)
        {
        case 0x1C:
            MZB++;
            if (FFXI::MZB::DecodeMZB(&buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD)))
            {
                mzb = std::make_unique<FFXI::MZB>(&buffer[offset + sizeof(DATHEAD)], len - (sizeof(DATHEAD)));
            }
            break;
        case 0x2E:
            MMB++;
            if (FFXI::MMB::DecodeMMB(&buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD)))
            {
                mmbs.push_back(std::make_unique<FFXI::MMB>(&buffer[offset + sizeof(DATHEAD)], len - (sizeof(DATHEAD))));
            }
            break;
        case 0x20:
            IMG++;
            break;
        case 0x29:
            BONE++;
            break;
        case 0x2A:
            VERT++;
            break;
        case 0x2B:
            ANIM++;
            break;
        default:
            break;
        }
        offset += len;
    }
}

std::shared_ptr<lotus::RenderableEntity> FFXILoadLandTest::getLand()
{
    auto entity = std::make_shared<lotus::LandscapeEntity>();
    auto [texture, texture_task] = lotus::Texture::LoadTexture<TestTextureLoader>(game->engine.get(), "test");
    game->engine->worker_pool.addWork(std::move(texture_task));

    for (const auto& mmb : mmbs)
    {
        std::string name(mmb->name, 16);
        auto model = std::make_unique<lotus::Model>(name);

        for (const auto& mmb_model : mmb->models)
        {
            auto mesh = std::make_shared<lotus::Mesh>();
            mesh->texture = texture;

            mesh->setVertexInputAttributeDescription(FFXI::MMB::Vertex::getAttributeDescriptions());
            mesh->setVertexInputBindingDescription(FFXI::MMB::Vertex::getBindingDescriptions());
            mesh->setIndexCount(static_cast<int>(mmb_model.indices.size()));

            std::vector<uint8_t> vertices_uint8;
            vertices_uint8.resize(mmb_model.vertices.size() * sizeof(FFXI::MMB::Vertex));
            memcpy(vertices_uint8.data(), mmb_model.vertices.data(), vertices_uint8.size());

            std::vector<uint8_t> indices_uint8;
            indices_uint8.resize(mmb_model.indices.size() * sizeof(uint16_t));
            memcpy(indices_uint8.data(), mmb_model.indices.data(), indices_uint8.size());

            mesh->vertex_buffer = game->engine->renderer.memory_manager->GetBuffer(vertices_uint8.size(), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
            mesh->index_buffer = game->engine->renderer.memory_manager->GetBuffer(indices_uint8.size(), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);

            game->engine->worker_pool.addWork(std::make_unique<lotus::ModelInitTask>(game->engine->renderer.getCurrentImage(), mesh, std::move(vertices_uint8), std::move(indices_uint8)));

            if (!lotus::Mesh::addMesh(name, mesh))
            {
                //__debugbreak();
            }

            model->meshes.push_back(std::move(mesh));
        }
        entity->models.push_back(std::move(model));
    }

    entity->instance_buffer = game->engine->renderer.memory_manager->GetBuffer(sizeof(lotus::LandscapeEntity::InstanceInfo) * mzb->vecMZB.size(),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);

    std::map<std::string, std::vector<lotus::LandscapeEntity::InstanceInfo>> temp_map;
    std::vector<lotus::LandscapeEntity::InstanceInfo> instance_info;

    for (const auto& mzb_piece : mzb->vecMZB)
    {
        std::string name(mzb_piece.id, 16);

        auto pos_mat = glm::translate(glm::mat4{ 1.f }, glm::vec3{ mzb_piece.fTransX, mzb_piece.fTransY, mzb_piece.fTransZ });
        auto rot_mat = glm::toMat4(glm::quat{ glm::vec3{mzb_piece.fRotX, mzb_piece.fRotY, mzb_piece.fRotZ} });
        auto scale_mat = glm::scale(glm::mat4{ 1.f }, glm::vec3{ mzb_piece.fScaleX, mzb_piece.fScaleY, mzb_piece.fScaleZ });

        lotus::LandscapeEntity::InstanceInfo info{ pos_mat * rot_mat * scale_mat };
        temp_map[name].push_back(info);
    }

    for (auto& [name, info_vec] : temp_map)
    {
        entity->instance_offsets[name] = std::make_pair(instance_info.size(), info_vec.size());
        instance_info.insert(instance_info.end(), std::make_move_iterator(info_vec.begin()), std::make_move_iterator(info_vec.end()));
    }

    game->engine->worker_pool.addWork(std::make_unique<lotus::LandscapeEntityInitTask>(entity, std::move(instance_info)));


    return entity;
}
