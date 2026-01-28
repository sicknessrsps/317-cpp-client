#include "LocType.h"

#include "LocEntity.h"
#include "SeqTransform.h"
#include "StringUtil.h"
#include "VarbitType.h"

namespace SDL_Client
{

    static Buffer dat;
    static std::vector<std::shared_ptr<LocType>> cache;

    LRUMap<int64_t, Model> LocType::modelCacheDynamic(30);
    LRUMap<int64_t, Model> LocType::modelCacheStatic(500);

    Game* LocType::game = nullptr;

    std::shared_ptr<LocType> LocType::Get(int32_t locID)
    {
        if (locID >= count) {
            locID = 0;
        }
        for (int32_t i = 0; i < 20; i++) {
            auto& cached = cache[i];
            if (cached && cached->index == locID) {
                return cached;
            }
        }
        cachePos = (cachePos + 1) % 20;
        auto& type = cache[cachePos];
        if (!type) {
            type = std::make_shared<LocType>();
        }
        dat.position = offsets[locID];
        type->index = locID;
        type->Reset();
        type->Read(dat);
        return type;
    }

    void LocType::Read(Buffer& buffer)
    {
        int32_t interactable = -1;

        while (true) {
            int32_t code = buffer.ReadU8();

            if (code == 0) {
                break;
            } else if (code == 1) {
                int32_t k = buffer.ReadU8();
                if (k > 0) {
                    if ((modelIDs.empty()) || lowmem) {
                        modelKinds.resize(k);
                        modelIDs.resize(k);
                        for (int32_t k1 = 0; k1 < k; k1++) {
                            modelIDs[k1] = buffer.ReadU16();
                            modelKinds[k1] = buffer.ReadU8();
                        }
                    } else {
                        buffer.position += k * 3;
                    }
                }
            } else if (code == 2) {
                name = buffer.ReadString();
            } else if (code == 3) {
                examine = buffer.ReadString();
            } else if (code == 5) {
                int32_t modelCount = buffer.ReadU8();
                if (modelCount > 0) {
                    if ((modelIDs.empty()) || lowmem) {
                        modelKinds.clear();
                        modelIDs.resize(modelCount);
                        for (int32_t l1 = 0; l1 < modelCount; l1++) {
                            modelIDs[l1] = buffer.ReadU16();
                        }
                    } else {
                        buffer.position += modelCount * 2;
                    }
                }
            } else if (code == 14) {
                sizeX = buffer.ReadU8();
            } else if (code == 15) {
                sizeZ = buffer.ReadU8();
            } else if (code == 17) {
                solid = false;
            } else if (code == 18) {
                blocksProjectiles = false;
            } else if (code == 19) {
                interactable = buffer.ReadU8();

                if (interactable == 1) {
                    this->interactable = true;
                }
            } else if (code == 21) {
                adjustToTerrain = true;
            } else if (code == 22) {
                dynamic = true;
            } else if (code == 23) {
                occludes = true;
            } else if (code == 24) {
                seqID = buffer.ReadU16();
                if (seqID == 65535) {
                    seqID = -1;
                }
            } else if (code == 28) {
                decorOffset = buffer.ReadU8();
            } else if (code == 29) {
                lightAmbient = buffer.Read8();
            } else if (code == 39) {
                lightAttenuation = buffer.Read8();
            } else if ((code >= 30) && (code < 39)) {
                if (options.empty()) {
                    options.resize(5);
                }
                options[code - 30] = buffer.ReadString();
                if (StringUtil::EqualsIgnoreCase(options[code - 30], "hidden")) {
                    options[code - 30] = std::string();
                }
            } else if (code == 40) {
                int32_t recolorCount = buffer.ReadU8();
                srcColor.resize(recolorCount);
                dstColor.resize(recolorCount);
                for (int32_t i = 0; i < recolorCount; i++) {
                    srcColor[i] = buffer.ReadU16();
                    dstColor[i] = buffer.ReadU16();
                }
            } else if (code == 60) {
                mapfunctionIcon = buffer.ReadU16();
            } else if (code == 62) {
                invert = true;
            } else if (code == 64) {
                castShadow = false;
            } else if (code == 65) {
                scaleX = buffer.ReadU16();
            } else if (code == 66) {
                scaleZ = buffer.ReadU16();
            } else if (code == 67) {
                scaleY = buffer.ReadU16();
            } else if (code == 68) {
                mapsceneIcon = buffer.ReadU16();
            } else if (code == 69) {
                interactionSideFlags = buffer.ReadU8();
            } else if (code == 70) {
                translateX = buffer.Read16();
            } else if (code == 71) {
                translateY = buffer.Read16();
            } else if (code == 72) {
                translateZ = buffer.Read16();
            } else if (code == 73) {
                important = true;
            } else if (code == 74) {
                decorative = true;
            } else if (code == 75) {
                supportsObj = buffer.ReadU8();
            } else if (code == 77) {
                varbit = buffer.ReadU16();

                if (varbit == 65535) {
                    varbit = -1;
                }

                varp = buffer.ReadU16();

                if (varp == 65535) {
                    varp = -1;
                }

                int32_t overrideCount = buffer.ReadU8();
                overrideTypeIDs.resize(overrideCount + 1);

                for (int32_t i = 0; i <= overrideCount; i++) {
                    overrideTypeIDs[i] = buffer.ReadU16();

                    if (overrideTypeIDs[i] == 65535) {
                        overrideTypeIDs[i] = -1;
                    }
                }
            }
        }

        // no code 19
        if (interactable == -1) {
            this->interactable = (!modelIDs.empty()) && ((modelKinds.empty()) || (modelKinds[0] == 10));

            if (!options.empty()) {
                this->interactable = true;
            }
        }

        if (decorative) {
            solid = false;
            blocksProjectiles = false;
        }

        if (supportsObj == -1) {
            supportsObj = solid ? 1 : 0;
        }
    }

    void LocType::Reset()
    {
        modelIDs.clear();
        modelKinds.clear();
        name.clear();
        examine.clear();
        srcColor.clear();
        dstColor.clear();
        sizeX = 1;
        sizeZ = 1;
        solid = true;
        blocksProjectiles = true;
        interactable = false;
        adjustToTerrain = false;
        dynamic = false;
        occludes = false;
        seqID = -1;
        decorOffset = 16;
        lightAmbient = 0;
        lightAttenuation = 0;
        options.clear();
        mapfunctionIcon = -1;
        mapsceneIcon = -1;
        invert = false;
        castShadow = true;
        scaleX = 128;
        scaleZ = 128;
        scaleY = 128;
        interactionSideFlags = 0;
        translateX = 0;
        translateY = 0;
        translateZ = 0;
        important = false;
        decorative = false;
        supportsObj = -1;
        varbit = -1;
        varp = -1;
        overrideTypeIDs.clear();
    }

    std::shared_ptr<Model> LocType::GetModel(int32_t kind, int32_t transformID, int32_t rotation)
    {
        std::shared_ptr<Model> model = nullptr;
        int64_t bitset;
        if (modelKinds.empty()) {
            if (kind != 10) {
                return nullptr;
            }

            bitset = (static_cast<int64_t>(index) << 6) + rotation + (static_cast<int64_t>(transformID + 1) << 32);
            auto cached = modelCacheDynamic.get(bitset);

            if (cached != nullptr) {
                return cached;
            }

            if (modelIDs.empty()) {
                return nullptr;
            }

            bool flip = invert ^ (rotation > 3);
            int32_t modelCount = modelIDs.size();

            for (int32_t i = 0; i < modelCount; i++) {
                int32_t modelID = modelIDs[i];

                if (flip) {
                    modelID += 0x10000;
                }

                model = modelCacheStatic.get(modelID);
                if (model == nullptr) {
                    model = Model::TryGet(modelID & 0xffff);
                    if (model == nullptr) {
                        return nullptr;
                    }
                    if (flip) {
                        model->RotateY180();
                    }
                    modelCacheStatic.put(modelID, model);
                }

                if (modelCount > 1) {
                    TMP_MODELS[i] = model;
                }
            }

            if (modelCount > 1) {
                model = std::make_shared<Model>(modelCount, TMP_MODELS);
            }
        } else {
            int32_t kindIndex = -1;

            for (int32_t i = 0; i < modelKinds.size(); i++) {
                if (modelKinds[i] != kind) {
                    continue;
                }
                kindIndex = i;
                break;
            }

            if (kindIndex == -1) {
                return nullptr;
            }

            bitset = (static_cast<int64_t>(index) << 6)
               + (static_cast<int64_t>(kindIndex) << 3)
               + rotation
               + (static_cast<int64_t>(transformID + 1) << 32);

            auto cached = modelCacheDynamic.get(bitset);

            if (cached != nullptr) {
                return cached;
            }

            int32_t modelID = modelIDs[kindIndex];
            bool flip = invert ^ (rotation > 3);

            if (flip) {
                modelID += 0x10000;
            }

            model = modelCacheStatic.get(modelID);

            if (model == nullptr) {
                model = Model::TryGet(modelID & 0xffff);

                if (model == nullptr) {
                    return nullptr;
                }

                if (flip) {
                    model->RotateY180();
                }

                modelCacheStatic.put(modelID, model);
            }
        }

        bool scaled = (scaleX != 128) || (scaleZ != 128) || (scaleY != 128);
        bool translated = (translateX != 0) || (translateY != 0) || (translateZ != 0);

        auto modified = std::make_shared<Model>(srcColor.empty(), SeqTransform::IsNull(transformID),
            (rotation == 0) && (transformID == -1) && !scaled && !translated, *model);
        if (transformID != -1) {
            modified->CreateLabelReferences();
            modified->ApplyTransform(transformID);
            modified->labelFaces.clear();
            modified->labelVertices.clear();
        }

        while (rotation-- > 0) {
            modified->RotateY90();
        }

        if (!srcColor.empty()) {
            for (int32_t k2 = 0; k2 < srcColor.size(); k2++) {
                modified->Recolor(srcColor[k2], dstColor[k2]);
            }
        }

        if (scaled) {
            modified->Scale(scaleX, scaleY, scaleZ);
        }

        if (translated) {
            modified->Translate(translateX, translateY, translateZ);
        }

        modified->CalculateNormals(64 + lightAmbient, 768 + (lightAttenuation * 5),
            -50, -10, -50, !dynamic);

        if (supportsObj == 1) {
            modified->objRaise = modified->minY;
        }

        modelCacheDynamic.put(bitset, modified);
        return modified;
    }

    void LocType::Unpack(FileArchive& archive)
    {
        dat = Buffer(archive.Read("loc.dat"));
        Buffer buffer(archive.Read("loc.idx"));
        count = buffer.ReadU16();
        offsets.resize(count);
        int32_t offset = 2;
        for (int32_t j = 0; j < count; j++) {
            offsets[j] = offset;
            offset += buffer.ReadU16();
        }
        cache.resize(20);
        for (int32_t i = 0; i < 20; i++) {
            cache[i] = std::make_shared<LocType>();
        }
    }

    std::shared_ptr<Model> LocType::GetModel(int32_t kind, int32_t rotation, int32_t heightmapSW, int32_t heightmapSE,
        int32_t heightmapNE, int32_t heightmapNW, int32_t transformID)
    {

        auto model = GetModel(kind, transformID, rotation);

        if (model == nullptr) {
            return nullptr;
        }

        if (adjustToTerrain || dynamic) {
            model = std::make_shared<Model>(adjustToTerrain, dynamic, *model);
        }

        if (adjustToTerrain) {
            int32_t groundY = (heightmapSW + heightmapSE + heightmapNE + heightmapNW) / 4;
            for (int32_t i = 0; i < model->vertexCount; i++) {
                int32_t x = model->vertexX[i];
                int32_t z = model->vertexZ[i];
                int32_t heightS = heightmapSW + (((heightmapSE - heightmapSW) * (x + 64)) / 128);
                int32_t heightN = heightmapNW + (((heightmapNE - heightmapNW) * (x + 64)) / 128);
                int32_t y = heightS + (((heightN - heightS) * (z + 64)) / 128);
                model->vertexY[i] += y - groundY;
            }
            model->CalculateBoundsY();
        }
        return model;
    }

    std::shared_ptr<LocType> LocType::GetOverrideType()
    {
        int32_t value = -1;

        if (varbit != -1) {
            auto& vB = VarbitType::instances[this->varbit];
            int32_t varp = vB->varp;
            int32_t low = vB->lsb;
            int32_t high = vB->msb;
            int32_t mask = Game::BITMASK[high - low];
            value = (LocEntity::game->varps[varp] >> low) & mask;
        } else if (varp != -1) {
            value = game->varps[varp];
        }

        if ((value < 0) || (value >= overrideTypeIDs.size()) || (overrideTypeIDs[value] == -1)) {
            return nullptr;
        } else {
            return Get(overrideTypeIDs[value]);
        }
    }

    void LocType::Unload()
    {
        modelCacheStatic.clear();
        modelCacheDynamic.clear();
        offsets.clear();
        cache.clear();
        dat.Clear();
    }

    void LocType::Prefetch(OnDemand& onDemand)
    {
        if (modelIDs.empty()) {
            return;
        }
        for (int32_t modelID : modelIDs) {
            onDemand.Prefetch(modelID & 0xffff, 0);
        }
    }

    bool LocType::Validate(int32_t kind) const
     {
        if (modelKinds.empty()) {
            if (modelIDs.empty()) {
                return true;
            }
            if (kind != 10) {
                return true;
            }
            bool valid = true;
            for (int32_t modelID : modelIDs) {
                valid &= Model::Validate(modelID & 0xffff);
            }
            return valid;
        }
        for (int32_t i = 0; i < modelKinds.size(); i++) {
            if (modelKinds[i] == kind) {
                return Model::Validate(modelIDs[i] & 0xffff);
            }
        }
        return true;
     }

     bool LocType::Validate() const
     {
        if (modelIDs.empty()) {
            return true;
        }
        bool ok = true;
        for (int32_t modelID : modelIDs) {
            ok &= Model::Validate(modelID & 0xffff);
        }
        return ok;
     }
}
