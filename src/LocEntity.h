#pragma once
#include "Entity.h"
#include "LocType.h"
#include "Game.h"

namespace SDL_Client
{
    class LocEntity : public Entity
    {
    public:
        LocEntity(int32_t id, int32_t rotation, int32_t kind, int32_t heightmapSE,
            int32_t heightmapNE, int32_t heightmapSW, int32_t heightmapNW, int32_t seqID, bool randomFrame);
        std::shared_ptr<Model> GetModel() override;
        std::shared_ptr<LocType> GetOverrideType() const;
    public:
        std::vector<int32_t> overrideTypeIDs;
        int32_t varbit;
        int32_t varp;
        int32_t heightmapSW;
        int32_t heightmapSE;
        int32_t heightmapNE;
        int32_t heightmapNW;
        int32_t id;
        int32_t kind;
        int32_t rotation;
        static Game* game;
        int32_t seqFrame;
        std::shared_ptr<SeqType> seq;
        int32_t seqCycle;
    };

}
