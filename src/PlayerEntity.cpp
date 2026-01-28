#include "PlayerEntity.h"

#include "Game.h"
#include "StringUtil.h"
#include "IdkType.h"
#include "SeqTransform.h"
#include "SeqType.h"
#include "SpotAnimType.h"

namespace SDL_Client
{
    LRUMap<int64_t, Model> PlayerEntity::modelCache(260);

    void PlayerEntity::Read(Buffer& in)
    {
        in.position = 0;
        gender = in.ReadU8();
        headicons = in.ReadU8();
        transmogrify = nullptr;
        team = 0;

        for (int32_t part = 0; part < 12; part++) {
            int32_t msb = in.ReadU8();

            if (msb == 0) {
                appearances[part] = 0;
                continue;
            }

            int32_t lsb = in.ReadU8();
            appearances[part] = (msb << 8) + lsb;

            if ((part == 0) && (appearances[0] == 65535)) {
                transmogrify = NPCType::Get(in.ReadU16());
                break;
            }

            if ((appearances[part] >= 512) && ((appearances[part] - 512) < ObjType::count)) {
                int32_t team = ObjType::Get(appearances[part] - 512)->team;

                if (team != 0) {
                    this->team = team;
                }
            }
        }

        for (int32_t part = 0; part < 5; part++) {
            int32_t color = in.ReadU8();

            if ((color < 0) || (color >= Game::designPartColor[part].size())) {
                color = 0;
            }

            colors[part] = color;
        }

        seqStandID = in.ReadU16();
        if (seqStandID == 65535) {
            seqStandID = -1;
        }
        seqTurnID = in.ReadU16();
        if (seqTurnID == 65535) {
            seqTurnID = -1;
        }
        seqWalkID = in.ReadU16();
        if (seqWalkID == 65535) {
            seqWalkID = -1;
        }
        seqTurnAroundID = in.ReadU16();
        if (seqTurnAroundID == 65535) {
            seqTurnAroundID = -1;
        }
        seqTurnLeftID = in.ReadU16();
        if (seqTurnLeftID == 65535) {
            seqTurnLeftID = -1;
        }
        seqTurnRightID = in.ReadU16();
        if (seqTurnRightID == 65535) {
            seqTurnRightID = -1;
        }
        seqRunID = in.ReadU16();
        if (seqRunID == 65535) {
            seqRunID = -1;
        }

        name = StringUtil::FormatName(StringUtil::FromBase37(in.Read64()));
        combatLevel = in.ReadU8();
        skillLevel = in.ReadU16();
        visible = true;

        appearanceHashcode = 0L;

        for (int32_t part = 0; part < 12; part++) {
            appearanceHashcode <<= 4;
            if (appearances[part] >= 256) {
                appearanceHashcode += appearances[part] - 256;
            }
        }

        if (appearances[0] >= 256) {
            appearanceHashcode += (appearances[0] - 256) >> 4;
        }

        if (appearances[1] >= 256) {
            appearanceHashcode += (appearances[1] - 256) >> 8;
        }

        for (int32_t part = 0; part < 5; part++) {
            appearanceHashcode <<= 3;
            appearanceHashcode += colors[part];
        }

        appearanceHashcode <<= 1;
        appearanceHashcode += gender;
    }

    std::shared_ptr<Model> PlayerEntity::GetModel()
    {
        if (!visible) {
            return nullptr;
        }

        auto model = GetSequencedModel();
        if (model == nullptr) {
            return nullptr;
        }

        height = model->minY;
        model->pickable = true;

        if (lowmem) {
            return model;
        }

        if (spotanimID != -1 && (spotanimFrame != -1)) {
            auto& spot = SpotAnimType::instances[spotanimID];
            const auto& spotModel1 = spot.GetModel();

            if (spotModel1 != nullptr) {
                auto spotModel2 = std::make_shared<Model>(true, SeqTransform::IsNull(spotanimFrame), false, *spotModel1);
                spotModel2->Translate(0, -spotanimOffset, 0);
                spotModel2->CreateLabelReferences();
                spotModel2->ApplyTransform(spot.seq.transformIDs[spotanimFrame]);
                spotModel2->labelFaces.clear();
                spotModel2->labelVertices.clear();
                if ((spot.scaleXY != 128) || (spot.scaleZ != 128)) {
                    spotModel2->Scale(spot.scaleXY, spot.scaleXY, spot.scaleZ);
                }
                spotModel2->CalculateNormals(64 + spot.lightAmbient, 850 + spot.lightAttenuation, -30, -50, -30, true);
                model = std::make_shared<Model>(2, -819, std::vector{model, spotModel2});
            }
        }

        // transform player into object
        if (locModel != nullptr) {
            if (Game::loopCycle >= locStopCycle) {
                locModel = nullptr;
            }

            if ((Game::loopCycle >= locStartCycle) && (Game::loopCycle < locStopCycle)) {
                auto lModel = this->locModel;
                lModel->Translate(locOffsetX - x, locOffsetY - y, locOffsetZ - z);

                if (dstYaw == 512) {
                    lModel->RotateY90();
                    lModel->RotateY90();
                    lModel->RotateY90();
                } else if (dstYaw == 1024) {
                    lModel->RotateY90();
                    lModel->RotateY90();
                } else if (dstYaw == 1536) {
                    lModel->RotateY90();
                }

                model = std::make_shared<Model>(2, -819, std::vector{model, lModel});

                if (dstYaw == 512) {
                    lModel->RotateY90();
                } else if (dstYaw == 1024) {
                    lModel->RotateY90();
                    lModel->RotateY90();
                } else if (dstYaw == 1536) {
                    lModel->RotateY90();
                    lModel->RotateY90();
                    lModel->RotateY90();
                }

                locModel->Translate(x - locOffsetX, y - locOffsetY, z - locOffsetZ);
            }
        }

        model->pickable = true;
        return model;
    }

    std::shared_ptr<Model> PlayerEntity::GetHeadModel()
    {
        if (!visible) {
            return nullptr;
        }

        if (transmogrify != nullptr) {
            return transmogrify->GetHeadModel();
        }

        bool invalid = false;

        for (int32_t part = 0; part < 12; part++) {
            int32_t value = appearances[part];

            if ((value >= 256) && (value < 512) && !IdkType::instances[value - 256].ValidateHeadModel()) {
                invalid = true;
            }

            if ((value >= 512) && !ObjType::Get(value - 512)->ValidateHeadModel(gender)) {
                invalid = true;
            }
        }

        if (invalid) {
            return nullptr;
        }

        std::vector<std::shared_ptr<Model>> models(12);
        int32_t modelCount = 0;

        for (int32_t part = 0; part < 12; part++) {
            int32_t value = appearances[part];

            if ((value >= 256) && (value < 512)) {
                auto model = IdkType::instances[value - 256].GetHeadModel();
                if (model != nullptr) {
                    models[modelCount++] = model;
                }
            }
            if (value >= 512) {
                auto model = ObjType::Get(value - 512)->GetHeadModel(gender);
                if (model != nullptr) {
                    models[modelCount++] = model;
                }
            }
        }

        auto model = std::make_shared<Model>(modelCount, models);

        for (int32_t part = 0; part < 5; part++) {
            if (colors[part] != 0) {
                model->Recolor(Game::designPartColor[part][0], Game::designPartColor[part][colors[part]]);
                if (part == 1) {
                    model->Recolor(Game::designHairColor[0], Game::designHairColor[colors[part]]);
                }
            }
        }

        return model;
    }

    bool PlayerEntity::IsVisible()
    {
        return visible;
    }

    std::shared_ptr<Model> PlayerEntity::GetSequencedModel()
    {
        if (transmogrify != nullptr) {
            int32_t transformID = -1;
            if ((primarySeqID >= 0) && (primarySeqDelay == 0)) {
                transformID = SeqType::instances[primarySeqID].transformIDs[primarySeqFrame];
            } else if (secondarySeqID >= 0) {
                transformID = SeqType::instances[secondarySeqID].transformIDs[secondarySeqFrame];
            }
            std::vector<int32_t> empty;
            return transmogrify->GetSequencedModel(-1, transformID, empty);
        }

        int64_t hashCode = this->appearanceHashcode;
        int32_t primaryTransformID = -1;
        int32_t secondaryTransformID = -1;
        int32_t rightHandValue = -1;
        int32_t leftHandValue = -1;

        if ((primarySeqID >= 0) && (primarySeqDelay == 0)) {
            const auto& type = SeqType::instances[primarySeqID];
            primaryTransformID = type.transformIDs[primarySeqFrame];

            if ((secondarySeqID >= 0) && (secondarySeqID != seqStandID)) {
                secondaryTransformID = SeqType::instances[secondarySeqID].transformIDs[secondarySeqFrame];
            }

            if (type.rightHandOverride >= 0) {
                rightHandValue = type.rightHandOverride;
                hashCode += (static_cast<int64_t>(rightHandValue) - appearances[5]) << 8;
            }

            if (type.leftHandOverride >= 0) {
                leftHandValue = type.leftHandOverride;
                hashCode += (static_cast<int64_t>(leftHandValue) - appearances[3]) << 16;
            }
        } else if (secondarySeqID >= 0) {
            primaryTransformID = SeqType::instances[secondarySeqID].transformIDs[secondarySeqFrame];
        }

        auto model = modelCache.get(hashCode);

        if (model == nullptr) {
            bool invalid = false;

            for (int32_t part = 0; part < 12; part++) {
                int32_t value = appearances[part];

                if ((leftHandValue >= 0) && (part == 3)) {
                    value = leftHandValue;
                }

                if ((rightHandValue >= 0) && (part == 5)) {
                    value = rightHandValue;
                }

                if ((value >= 256) && (value < 512) && !IdkType::instances[value - 256].ValidateModel()) {
                    invalid = true;
                }

                if ((value >= 512) && !ObjType::Get(value - 512)->ValidateWornModel(gender)) {
                    invalid = true;
                }
            }

            if (invalid) {
                if (modelUID != -1L) {
                    model = modelCache.get(modelUID);
                }
                if (model == nullptr) {
                    return nullptr;
                }
            }
        }

        if (model == nullptr) {
            std::vector<std::shared_ptr<Model>> models(12);
            int32_t modelCount = 0;

            for (int32_t part = 0; part < 12; part++) {
                int32_t value = appearances[part];

                if ((leftHandValue >= 0) && (part == 3)) {
                    value = leftHandValue;
                }

                if ((rightHandValue >= 0) && (part == 5)) {
                    value = rightHandValue;
                }

                if ((value >= 256) && (value < 512)) {
                    auto kitModel = IdkType::instances[value - 256].GetModel();
                    if (kitModel != nullptr) {
                        models[modelCount++] = kitModel;
                    }
                }

                if (value >= 512) {
                    auto objModel = ObjType::Get(value - 512)->GetWornModel(gender);

                    if (objModel != nullptr) {
                        models[modelCount++] = objModel;
                    }
                }
            }

            model = std::make_shared<Model>(modelCount, models);
            for (int32_t part = 0; part < 5; part++) {
                if (colors[part] != 0) {
                    model->Recolor(Game::designPartColor[part][0], Game::designPartColor[part][colors[part]]);
                    if (part == 1) {
                        model->Recolor(Game::designHairColor[0], Game::designHairColor[colors[part]]);
                    }
                }
            }
            model->CreateLabelReferences();
            model->CalculateNormals(64, 850, -30, -50, -30, true);
            modelCache.put(hashCode, model);
            modelUID = hashCode;
        }

        if (lowmem) {
            return model;
        }

        auto tmp = std::make_shared<Model>();
        tmp->Set(*model, SeqTransform::IsNull(primaryTransformID) & SeqTransform::IsNull(secondaryTransformID));

        if ((primaryTransformID != -1) && (secondaryTransformID != -1)) {
            tmp->ApplyTransforms(primaryTransformID, secondaryTransformID, SeqType::instances[primarySeqID].mask);
        } else if (primaryTransformID != -1) {
            tmp->ApplyTransform(primaryTransformID);
        }

        tmp->CalculateBoundsCylinder();
        tmp->labelFaces.clear();
        tmp->labelVertices.clear();
        return tmp;
    }
}
