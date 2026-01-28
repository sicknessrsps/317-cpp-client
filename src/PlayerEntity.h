#pragma once
#include "PCH.h"
#include "Buffer.h"
#include "PathingEntity.h"
#include "Model.h"
#include "NPCType.h"

namespace SDL_Client
{
    class PlayerEntity : public PathingEntity
    {
    public:
        void Read(Buffer& in);
        std::shared_ptr<Model> GetModel() override;
        std::shared_ptr<Model> GetHeadModel();
        bool IsVisible() override;
    public:
        inline static bool lowmem = false;
        static LRUMap<int64_t, Model> modelCache;
        int32_t skillLevel = 0;
        std::string name;
        int32_t combatLevel = 0;
        int32_t team = 0;
        bool visible = false;

        int32_t locStartCycle = 0;
        int32_t locStopCycle = 0;
        std::shared_ptr<Model> locModel;
        int32_t locOffsetX = 0;
        int32_t locOffsetY = 0;
        int32_t locOffsetZ = 0;
        int32_t minSceneTileX = 0;
        int32_t minSceneTileZ = 0;
        int32_t maxSceneTileX = 0;
        int32_t maxSceneTileZ = 0;
        int32_t headicons = 0;
        std::shared_ptr<NPCType> transmogrify;
        std::array<int32_t, 12> appearances{};
        std::array<int32_t, 5> colors{};
    private:
        std::shared_ptr<Model> GetSequencedModel();
    private:
        int64_t modelUID = -1L;
        int32_t gender = 0;
        int64_t appearanceHashcode = 0;
    };
}
