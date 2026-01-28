#include "LocEntity.h"

#include "VarbitType.h"
#include "Game.h"

namespace SDL_Client
{

    Game* LocEntity::game = nullptr;

    LocEntity::LocEntity(int32_t id, int32_t rotation, int32_t kind, int32_t heightmapSE, int32_t heightmapNE,
        int32_t heightmapSW, int32_t heightmapNW, int32_t seqID, bool randomFrame)
            : id(id), kind(kind), rotation(rotation), heightmapSW(heightmapSW), heightmapSE(heightmapSE),
                heightmapNE(heightmapNE), heightmapNW(heightmapNW)
    {
        if (seqID != -1) {
            seq = std::make_shared<SeqType>(SeqType::instances[seqID]);
            seqFrame = 0;
            seqCycle = Game::loopCycle;

            if (randomFrame && (seq->loopFrameCount != -1)) {
                seqFrame = static_cast<int32_t>(SDL_randf() * static_cast<float>(seq->frameCount));
                seqCycle -= static_cast<int32_t>(SDL_randf() * static_cast<float>(seq->GetFrameDuration(seqFrame)));
            }
        }
        const auto& type = LocType::Get(id);
        varbit = type->varbit;
        varp = type->varp;
        overrideTypeIDs = type->overrideTypeIDs;
    }

    std::shared_ptr<Model> LocEntity::GetModel()
    {
        int32_t transformID = -1;

        if (seq != nullptr) {
            int32_t delta = Game::loopCycle - seqCycle;

            if ((delta > 100) && (seq->loopFrameCount > 0)) {
                delta = 100;
            }

            while (delta > seq->GetFrameDuration(seqFrame)) {
                delta -= seq->GetFrameDuration(seqFrame);
                seqFrame++;

                if (seqFrame < seq->frameCount) {
                    continue;
                }

                seqFrame -= seq->loopFrameCount;

                if ((seqFrame >= 0) && (seqFrame < seq->frameCount)) {
                    continue;
                }

                seq = nullptr;
                break;
            }
            seqCycle = Game::loopCycle - delta;

            if (seq != nullptr) {
                transformID = seq->transformIDs[seqFrame];
            }
        }

         std::shared_ptr<LocType> type;

        if (!overrideTypeIDs.empty()) {
            type = GetOverrideType();
        } else {
            type = LocType::Get(id);
        }

        if (type == nullptr) {
            return nullptr;
        } else {
            return type->GetModel(kind, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, transformID);
        }
    }

    std::shared_ptr<LocType> LocEntity::GetOverrideType() const
    {
        int32_t value = -1;

        if (varbit != -1) {
            const auto& varb = VarbitType::instances[this->varbit];
            int32_t varpIndex = varb->varp;
            int32_t low = varb->lsb;
            int32_t high = varb->msb;
            int32_t mask = Game::BITMASK[high - low];
            value = (game->varps[varpIndex] >> low) & mask;
        } else if (varp != -1) {
            value = game->varps[varp];
        }

        if ((value < 0) || (value >= overrideTypeIDs.size()) || (overrideTypeIDs[value] == -1)) {
            return nullptr;
        } else {
            return LocType::Get(overrideTypeIDs[value]);
        }
    }
}
