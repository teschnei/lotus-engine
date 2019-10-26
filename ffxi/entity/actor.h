#pragma once
#include "engine/entity/renderable_entity.h"

namespace FFXI {
    class SK2;
    class OS2;
}

class Actor : public lotus::RenderableEntity
{
public:
    Actor();
    void Init(const std::shared_ptr<Actor>& sp, lotus::Engine* engine, const std::string& dat);
};

class FFXIActorLoader : public lotus::ModelLoader
{
public:
    FFXIActorLoader(const std::vector<std::unique_ptr<FFXI::OS2>>* os2s, FFXI::SK2* sk2);
    virtual void LoadModel(std::shared_ptr<lotus::Model>&) override;
private:
    const std::vector<std::unique_ptr<FFXI::OS2>>* os2s;
    FFXI::SK2* sk2;
};
