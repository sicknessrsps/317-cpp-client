#pragma once
#include "Entity.h"
#include "SpotAnimType.h"

namespace SDL_Client
{
    class SpotAnimEntity : public Entity
    {
    public:
        SpotAnimEntity(int32_t level, int32_t cycle, int32_t delay, int32_t id, int32_t y, int32_t z, int32_t x);
        virtual std::shared_ptr<Model> GetModel() override;
        void Update(int32_t delta);
    public:
        int32_t level = 0;
        int32_t x = 0;
        int32_t z = 0;
        int32_t y = 0;
        int32_t startCycle = 0;
        SpotAnimType type;
        bool seqComplete = false;
        int32_t seqFrame = 0;
        int32_t seqCycle = 0;
    };
}
