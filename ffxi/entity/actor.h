#pragma once
#include <filesystem>
#include "engine/entity/deformable_entity.h"
#include "engine/task.h"

namespace FFXI {
    class SK2;
    class OS2;
}

//main FFXI entity class
class Actor : public lotus::DeformableEntity
{
public:
    explicit Actor(lotus::Engine* engine);
    static lotus::Task<std::shared_ptr<Actor>> Init(lotus::Engine* engine, const std::filesystem::path& dat);

    float speed{ 4.f };
private:
    lotus::WorkerTask<> Load(const std::filesystem::path& dat);
};

class FFXIActorLoader : public lotus::ModelLoader
{
public:
    FFXIActorLoader(const std::vector<FFXI::OS2*>& os2s, FFXI::SK2* sk2);
    virtual lotus::Task<> LoadModel(std::shared_ptr<lotus::Model>&) override;
private:
    const std::vector<FFXI::OS2*>& os2s;
    FFXI::SK2* sk2;
};
