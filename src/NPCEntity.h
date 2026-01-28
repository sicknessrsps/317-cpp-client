#pragma once
#include "PCH.h"
#include "PathingEntity.h"
#include "Model.h"
#include "NPCType.h"

namespace SDL_Client
{
    class NPCEntity : public PathingEntity
    {
    public:
        std::shared_ptr<Model> GetModel() override;
        bool IsVisible() override;
    public:
        std::shared_ptr<NPCType> type;
    private:
        std::shared_ptr<Model> GetSequencedModel() const;
    };
}