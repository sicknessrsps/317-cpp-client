#include "NPCType.h"
#include "Game.h"
#include "VarbitType.h"
#include "SeqTransform.h"
#include "StringUtil.h"

namespace SDL_Client
{
    static std::vector<std::shared_ptr<NPCType>> cache;

    LRUMap<int64_t, Model> NPCType::modelCache(30);
    Game* NPCType::game = nullptr;

    void NPCType::Unpack(FileArchive& archive)
    {
        dat = Buffer(archive.Read("npc.dat"));
        Buffer idx(archive.Read("npc.idx"));
        count = idx.ReadU16();
        offsets.resize(count);

        int32_t offset = 2;
        for (int32_t i = 0; i < count; i++) {
            offsets[i] = offset;
            offset += idx.ReadU16();
        }

        cache.resize(20);
        for (int32_t i = 0; i < 20; i++) {
            cache[i] = std::make_shared<NPCType>();
        }
    }

    void NPCType::Unload()
    {
        modelCache = LRUMap<int64_t, Model>(30);
        offsets.clear();
        cache.clear();
        dat.Clear();
    }

    std::shared_ptr<NPCType> NPCType::Get(int32_t id)
    {
        for (int32_t i = 0; i < 20; i++) {
            if (cache[i]->uid == static_cast<int64_t>(id)) {
                return cache[i];
            }
        }

        cachePos = (cachePos + 1) % 20;
        const auto& type = cache[cachePos];
        dat.position = offsets[id];
        type->uid = id;
        type->Read(dat);
        return type;
    }

    std::shared_ptr<Model> NPCType::GetHeadModel()
    {
        if (!overrides.empty()) {
            auto type = GetOverrideType();
            if (type == nullptr) {
                return nullptr;
            }
            return type->GetHeadModel();
        }

        if (headModelIDs.empty()) {
            return nullptr;
        }

        bool loaded = true;
        for (int32_t value : headModelIDs) {
            if (!Model::Validate(value)) {
                loaded = false;
            }
        }

        if (!loaded) {
            return nullptr;
        }

        std::vector<std::shared_ptr<Model>> models(headModelIDs.size());
        for (size_t i = 0; i < headModelIDs.size(); i++) {
            models[i] = Model::TryGet(headModelIDs[i]);
        }

        std::shared_ptr<Model> model;
        if (models.size() == 1) {
            model = models[0];
        } else {
            model = std::make_shared<Model>(static_cast<int32_t>(models.size()), models);
        }

        if (!colorSrc.empty()) {
            for (size_t i = 0; i < colorSrc.size(); i++) {
                model->Recolor(colorSrc[i], colorDst[i]);
            }
        }

        return model;
    }

    std::shared_ptr<NPCType> NPCType::GetOverrideType() const
    {
        int32_t value = -1;

        if (varbit != -1) {
            const auto& vb = VarbitType::instances[varbit];
            int32_t varpIndex = vb->varp;
            int32_t lsb = vb->lsb;
            int32_t msb = vb->msb;
            int32_t mask = Game::BITMASK[msb - lsb];
            value = (game->varps[varpIndex] >> lsb) & mask;
        } else if (varp != -1) {
            value = game->varps[varp];
        }

        if ((value < 0) || (value >= static_cast<int32_t>(overrides.size())) || (overrides[value] == -1)) {
            return nullptr;
        }
        return Get(overrides[value]);
    }

    std::shared_ptr<Model> NPCType::GetSequencedModel(int32_t secondaryTransformID, int32_t primaryTransformID, std::vector<int32_t>& seqMask)
    {
        if (!overrides.empty()) {
            auto override = GetOverrideType();
            if (override == nullptr) {
                return nullptr;
            }
            return override->GetSequencedModel(secondaryTransformID, primaryTransformID, seqMask);
        }

        auto model = modelCache.get(uid);

        if (model == nullptr) {
            bool invalid = false;

            for (int32_t value : modelIDs) {
                if (!Model::Validate(value)) {
                    invalid = true;
                }
            }

            if (invalid) {
                return nullptr;
            }

            std::vector<std::shared_ptr<Model>> models(modelIDs.size());
            for (size_t i = 0; i < modelIDs.size(); i++) {
                models[i] = Model::TryGet(modelIDs[i]);
            }

            if (models.size() == 1) {
                model = models[0];
            } else {
                model = std::make_shared<Model>(static_cast<int32_t>(models.size()), models);
            }

            if (!colorSrc.empty()) {
                for (size_t i = 0; i < colorSrc.size(); i++) {
                    model->Recolor(colorSrc[i], colorDst[i]);
                }
            }

            model->CreateLabelReferences();
            model->CalculateNormals(64 + lightAmbient, 850 + lightAttenuation, -30, -50, -30, true);
            modelCache.put(uid, model);
        }

        auto tmp = std::make_shared<Model>();
        tmp->Set(*model, SeqTransform::IsNull(primaryTransformID) & SeqTransform::IsNull(secondaryTransformID));

        if ((primaryTransformID != -1) && (secondaryTransformID != -1)) {
            tmp->ApplyTransforms(primaryTransformID, secondaryTransformID, seqMask);
        } else if (primaryTransformID != -1) {
            tmp->ApplyTransform(primaryTransformID);
        }

        if ((scaleXY != 128) || (scaleZ != 128)) {
            tmp->Scale(scaleXY, scaleXY, scaleZ);
        }

        tmp->CalculateBoundsCylinder();
        tmp->labelFaces.clear();
        tmp->labelVertices.clear();

        if (size == 1) {
            tmp->pickable = true;
        }

        return tmp;
    }

    void NPCType::Read(Buffer& in)
    {
        while (true) {
            int32_t code = in.ReadU8();

            if (code == 0) {
                return;
            } else if (code == 1) {
                int32_t modelCount = in.ReadU8();
                modelIDs.resize(modelCount);
                for (int32_t i = 0; i < modelCount; i++) {
                    modelIDs[i] = in.ReadU16();
                }
            } else if (code == 2) {
                name = in.ReadString();
            } else if (code == 3) {
                examine = in.ReadString();
            } else if (code == 12) {
                size = in.Read8();
            } else if (code == 13) {
                seqStandID = in.ReadU16();
            } else if (code == 14) {
                seqWalkID = in.ReadU16();
            } else if (code == 17) {
                seqWalkID = in.ReadU16();
                seqTurnAroundID = in.ReadU16();
                seqTurnLeftID = in.ReadU16();
                seqTurnRightID = in.ReadU16();
            } else if ((code >= 30) && (code < 40)) {
                if (options.empty()) {
                    options.resize(5);
                }
                options[code - 30] = in.ReadString();
                if (StringUtil::EqualsIgnoreCase(options[code - 30], "hidden")) {
                    options[code - 30] = std::string();
                }
            } else if (code == 40) {
                int32_t recolorCount = in.ReadU8();
                colorSrc.resize(recolorCount);
                colorDst.resize(recolorCount);
                for (int32_t i = 0; i < recolorCount; i++) {
                    colorSrc[i] = in.ReadU16();
                    colorDst[i] = in.ReadU16();
                }
            } else if (code == 60) {
                int32_t headCount = in.ReadU8();
                headModelIDs.resize(headCount);
                for (int32_t i = 0; i < headCount; i++) {
                    headModelIDs[i] = in.ReadU16();
                }
            } else if ((code == 90) || (code == 91) || (code == 92)) {
                in.ReadU16();
            } else if (code == 93) {
                showOnMinimap = false;
            } else if (code == 95) {
                level = in.ReadU16();
            } else if (code == 97) {
                scaleXY = in.ReadU16();
            } else if (code == 98) {
                scaleZ = in.ReadU16();
            } else if (code == 99) {
                important = true;
            } else if (code == 100) {
                lightAmbient = in.Read8();
            } else if (code == 101) {
                lightAttenuation = in.Read8() * 5;
            } else if (code == 102) {
                headicon = in.ReadU16();
            } else if (code == 103) {
                turnSpeed = in.ReadU16();
            } else if (code == 106) {
                varbit = in.ReadU16();
                if (varbit == 65535) {
                    varbit = -1;
                }

                varp = in.ReadU16();
                if (varp == 65535) {
                    varp = -1;
                }

                int32_t overrideCount = in.ReadU8();
                overrides.resize(overrideCount + 1);
                for (int32_t i = 0; i <= overrideCount; i++) {
                    overrides[i] = in.ReadU16();
                    if (overrides[i] == 65535) {
                        overrides[i] = -1;
                    }
                }
            } else if (code == 107) {
                interactable = false;
            }
        }
    }
}
