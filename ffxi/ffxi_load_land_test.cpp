#include "ffxi_load_land_test.h"

#include "core/game.h"

#include <fstream>
#include "mzb.h"
#include "mmb.h"
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

class TextureLoader : public lotus::TextureLoader
{
public:
    TextureLoader(uint32_t _width, uint32_t _height, uint8_t* _pixels, vk::Format _format) : lotus::TextureLoader(), width(_width), height(_height), pixels(_pixels), format(_format) {}
    virtual std::unique_ptr<lotus::WorkItem> LoadTexture(std::shared_ptr<lotus::Texture>& texture) override
    {
        uint32_t stride = 4;
        if (format == vk::Format::eBc2UnormBlock)
            stride = 1;
        VkDeviceSize imageSize = width * height * stride;

        texture->setWidth(width);
        texture->setHeight(height);

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        std::vector<uint8_t> texture_data;
        texture_data.resize(imageSize);
        memcpy(texture_data.data(), pixels, imageSize);

        texture->image = engine->renderer.memory_manager->GetImage(texture->getWidth(), texture->getHeight(), format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = texture->image->image;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = format;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        texture->image_view = engine->renderer.device->createImageViewUnique(image_view_info, nullptr, engine->renderer.dispatch);

        vk::SamplerCreateInfo sampler_info = {};
        sampler_info.magFilter = vk::Filter::eLinear;
        sampler_info.minFilter = vk::Filter::eLinear;
        sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
        sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
        sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
        sampler_info.anisotropyEnable = true;
        sampler_info.maxAnisotropy = 16;
        sampler_info.borderColor = vk::BorderColor::eIntOpaqueBlack;
        sampler_info.unnormalizedCoordinates = false;
        sampler_info.compareEnable = false;
        sampler_info.compareOp = vk::CompareOp::eAlways;
        sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;

        texture->sampler = engine->renderer.device->createSamplerUnique(sampler_info, nullptr, engine->renderer.dispatch);

        return std::make_unique<lotus::TextureInitTask>(engine->renderer.getCurrentImage(), texture, format, vk::ImageTiling::eOptimal, std::move(texture_data));
    }
    
    uint32_t width;
    uint32_t height;
    uint8_t* pixels;
    vk::Format format;
};


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
            textures.push_back(std::make_unique<FFXI::Texture>(&buffer[offset + sizeof(DATHEAD)]));
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
    auto [default_texture, texture_task] = lotus::Texture::LoadTexture<TestTextureLoader>(game->engine.get(), "default");
    game->engine->worker_pool.addWork(std::move(texture_task));
    std::unordered_map<std::string, std::shared_ptr<lotus::Texture>> texture_map;

    for (const auto& texture_data : textures)
    {
        if (texture_data->width > 0)
        {
            auto [texture, texture_task] = lotus::Texture::LoadTexture<TextureLoader>(game->engine.get(), texture_data->name, texture_data->width, texture_data->height, texture_data->pixels.data(), texture_data->format);
            if (texture_task)
                game->engine->worker_pool.addWork(std::move(texture_task));
            texture_map[texture_data->name] = std::move(texture);
        }
    }

    for (const auto& mmb : mmbs)
    {
        std::string name(mmb->name, 16);
        auto model = lotus::Model::LoadModel<FFXI::MMBLoader>(game->engine.get(), name, mmb.get());

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

        glm::mat4 model = pos_mat * rot_mat * scale_mat;
        glm::mat3 model_it = glm::transpose(glm::inverse(glm::mat3(model)));
        lotus::LandscapeEntity::InstanceInfo info{model, model_it};
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
