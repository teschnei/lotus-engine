#pragma once

#include "core/renderer/mesh.h"

namespace lotus
{
    class Model
    {
    public:
        explicit Model(const std::string& name);

        std::string name;
        std::vector<std::shared_ptr<Mesh>> meshes;
    };
}
