#pragma once
#include "PCH.h"
#include "Entity.h"

namespace SDL_Client
{
    class ObjEntity : public Entity
    {
    public:
        ObjEntity() = default;
        int32_t id = 0;
        int32_t count = 0;
        std::shared_ptr<Model> GetModel() override;

    };
}
