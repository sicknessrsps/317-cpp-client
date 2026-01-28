#include "IfType.h"
#include "Game.h"
#include "NPCType.h"
#include "ObjType.h"
#include "SeqTransform.h"
#include "StringUtil.h"

namespace SDL_Client
{
    LRUMap<int64_t, Model> IfType::modelCache(30);
    LRUMap<int64_t, Image24> IfType::imageCache(1);
    std::vector<std::shared_ptr<IfType>> IfType::instances;
    Game* IfType::game = nullptr;

    void IfType::Unpack(FileArchive& config, const std::vector<std::shared_ptr<BitmapFont>>& fonts, FileArchive& media)
    {
        imageCache = LRUMap<int64_t, Image24>(500);
        Buffer in(config.Read("data"));

        int32_t parentID = -1;
        int32_t count = in.ReadU16();

        instances.resize(count);

        while (in.position < static_cast<int32_t>(in.data.size())) {
            int32_t id = in.ReadU16();

            if (id == 65535) {
                parentID = in.ReadU16();
                id = in.ReadU16();
            }

            auto iface = std::make_shared<IfType>();
            instances[id] = iface;
            iface->id = id;
            iface->parentID = parentID;
            iface->type = in.ReadU8();
            iface->optionType = in.ReadU8();
            iface->contentType = in.ReadU16();
            iface->width = in.ReadU16();
            iface->height = in.ReadU16();
            iface->transparency = static_cast<int8_t>(in.ReadU8());
            iface->delegateHover = in.ReadU8();

            if (iface->delegateHover != 0) {
                iface->delegateHover = ((iface->delegateHover - 1) << 8) + in.ReadU8();
            } else {
                iface->delegateHover = -1;
            }

            int32_t comparatorCount = in.ReadU8();
            if (comparatorCount > 0) {
                iface->scriptComparator.resize(comparatorCount);
                iface->scriptOperand.resize(comparatorCount);
                for (int32_t i = 0; i < comparatorCount; i++) {
                    iface->scriptComparator[i] = in.ReadU8();
                    iface->scriptOperand[i] = in.ReadU16();
                }
            }

            int32_t scriptCount = in.ReadU8();
            if (scriptCount > 0) {
                iface->scripts.resize(scriptCount);
                for (int32_t scriptID = 0; scriptID < scriptCount; scriptID++) {
                    int32_t length = in.ReadU16();
                    iface->scripts[scriptID].resize(length);
                    for (int32_t i = 0; i < length; i++) {
                        iface->scripts[scriptID][i] = in.ReadU16();
                    }
                }
            }

            if (iface->type == TYPE_PARENT) {
                iface->scrollableHeight = in.ReadU16();
                iface->hide = in.ReadU8() == 1;
                int32_t childCount = in.ReadU16();
                iface->childID.resize(childCount);
                iface->childX.resize(childCount);
                iface->childY.resize(childCount);
                for (int32_t i = 0; i < childCount; i++) {
                    iface->childID[i] = in.ReadU16();
                    iface->childX[i] = in.Read16();
                    iface->childY[i] = in.Read16();
                }
            }

            if (iface->type == TYPE_UNUSED) {
                in.ReadU16();
                in.ReadU8();
            }

            if (iface->type == TYPE_INVENTORY) {
                iface->inventorySlotObjID.resize(iface->width * iface->height);
                iface->inventorySlotObjCount.resize(iface->width * iface->height);
                iface->inventoryDraggable = in.ReadU8() == 1;
                iface->inventoryInteractable = in.ReadU8() == 1;
                iface->inventoryUsable = in.ReadU8() == 1;
                iface->inventoryMoveReplaces = in.ReadU8() == 1;
                iface->inventoryMarginX = in.ReadU8();
                iface->inventoryMarginY = in.ReadU8();
                iface->inventorySlotOffsetX.resize(20);
                iface->inventorySlotOffsetY.resize(20);
                iface->inventorySlotImage.resize(20);

                for (int32_t slot = 0; slot < 20; slot++) {
                    if (in.ReadU8() == 1) {
                        iface->inventorySlotOffsetX[slot] = in.Read16();
                        iface->inventorySlotOffsetY[slot] = in.Read16();
                        std::string imageName = in.ReadString();

                        if (!imageName.empty()) {
                            size_t imageID = imageName.rfind(',');
                            if (imageID != std::string::npos) {
                                iface->inventorySlotImage[slot] = GetImage(
                                    std::stoi(imageName.substr(imageID + 1)),
                                    media,
                                    imageName.substr(0, imageID)
                                );
                            }
                        }
                    }
                }

                iface->inventoryOptions.resize(5);

                for (int32_t i = 0; i < 5; i++) {
                    iface->inventoryOptions[i] = in.ReadString();
                    if (iface->inventoryOptions[i].empty()) {
                        iface->inventoryOptions[i].clear();
                    }
                }
            }

            if (iface->type == TYPE_RECT) {
                iface->fill = in.ReadU8() == 1;
            }

            if ((iface->type == TYPE_TEXT) || (iface->type == TYPE_UNUSED)) {
                iface->center = in.ReadU8() == 1;
                int32_t fontID = in.ReadU8();
                if (!fonts.empty() && fontID < static_cast<int32_t>(fonts.size())) {
                    iface->font = fonts[fontID];
                }
                iface->shadow = in.ReadU8() == 1;
            }

            if (iface->type == TYPE_TEXT) {
                iface->text = in.ReadString();
                iface->activeText = in.ReadString();
            }

            if ((iface->type == TYPE_UNUSED) || (iface->type == TYPE_RECT) || (iface->type == TYPE_TEXT)) {
                iface->color = in.Read32();
            }

            if ((iface->type == TYPE_RECT) || (iface->type == TYPE_TEXT)) {
                iface->activeColor = in.Read32();
                iface->hoverColor = in.Read32();
                iface->activeHoverColor = in.Read32();
            }

            if (iface->type == TYPE_IMAGE) {
                std::string s = in.ReadString();
                if (!s.empty()) {
                    size_t comma = s.rfind(',');
                    if (comma != std::string::npos) {
                        iface->image = GetImage(std::stoi(s.substr(comma + 1)), media, s.substr(0, comma));
                    }
                }
                s = in.ReadString();
                if (!s.empty()) {
                    size_t comma = s.rfind(',');
                    if (comma != std::string::npos) {
                        iface->activeImage = GetImage(std::stoi(s.substr(comma + 1)), media, s.substr(0, comma));
                    }
                }
            }

            if (iface->type == TYPE_MODEL) {
                int32_t tmp = in.ReadU8();
                if (tmp != 0) {
                    iface->modelType = MODEL_TYPE_NORMAL;
                    iface->modelID = ((tmp - 1) << 8) + in.ReadU8();
                }

                tmp = in.ReadU8();
                if (tmp != 0) {
                    iface->activeModelType = MODEL_TYPE_NORMAL;
                    iface->activeModelID = ((tmp - 1) << 8) + in.ReadU8();
                }

                tmp = in.ReadU8();
                if (tmp != 0) {
                    iface->seqID = ((tmp - 1) << 8) + in.ReadU8();
                } else {
                    iface->seqID = -1;
                }

                tmp = in.ReadU8();
                if (tmp != 0) {
                    iface->activeSeqID = ((tmp - 1) << 8) + in.ReadU8();
                } else {
                    iface->activeSeqID = -1;
                }

                iface->modelZoom = in.ReadU16();
                iface->modelPitch = in.ReadU16();
                iface->modelYaw = in.ReadU16();
            }

            if (iface->type == TYPE_INVENTORY_TEXT) {
                iface->inventorySlotObjID.resize(iface->width * iface->height);
                iface->inventorySlotObjCount.resize(iface->width * iface->height);
                iface->center = in.ReadU8() == 1;
                int32_t fontID = in.ReadU8();
                if (!fonts.empty() && fontID < static_cast<int32_t>(fonts.size())) {
                    iface->font = fonts[fontID];
                }
                iface->shadow = in.ReadU8() == 1;
                iface->color = in.Read32();
                iface->inventoryMarginX = in.Read16();
                iface->inventoryMarginY = in.Read16();
                iface->inventoryInteractable = in.ReadU8() == 1;
                iface->inventoryOptions.resize(5);
                for (int32_t option = 0; option < 5; option++) {
                    iface->inventoryOptions[option] = in.ReadString();
                    if (iface->inventoryOptions[option].empty()) {
                        iface->inventoryOptions[option].clear();
                    }
                }
            }

            if (iface->type == 8) {
                iface->spellAction = in.ReadString();
            }

            if ((iface->optionType == OPTION_TYPE_SPELL) || (iface->type == TYPE_INVENTORY)) {
                iface->spellAction = in.ReadString();
                iface->spellName = in.ReadString();
                iface->spellFlags = in.ReadU16();
            }

            if ((iface->optionType == OPTION_TYPE_STANDARD) || (iface->optionType == OPTION_TYPE_TOGGLE) ||
                (iface->optionType == OPTION_TYPE_SELECT) || (iface->optionType == OPTION_TYPE_CONTINUE)) {
                iface->option = in.ReadString();

                if (iface->option.empty()) {
                    if (iface->optionType == OPTION_TYPE_STANDARD) {
                        iface->option = "Ok";
                    } else if (iface->optionType == OPTION_TYPE_TOGGLE) {
                        iface->option = "Select";
                    } else if (iface->optionType == OPTION_TYPE_SELECT) {
                        iface->option = "Select";
                    } else if (iface->optionType == OPTION_TYPE_CONTINUE) {
                        iface->option = "Continue";
                    }
                }
            }
        }
        imageCache.clear();
    }

    std::shared_ptr<Image24> IfType::GetImage(int32_t id, FileArchive& media, const std::string& name)
    {
        int64_t uid = (StringUtil::HashCode(name) << 8) + static_cast<int64_t>(id);
        auto image = imageCache.get(uid);

        if (image != nullptr) {
            return image;
        }

        if (id < 0 || id >= Image24::Count(media, name)) {
            return nullptr;
        }

        image = std::make_shared<Image24>(media, name, id);
        imageCache.put(uid, image);
        return image;
    }

    void IfType::CacheModel(int32_t id, int32_t type, std::shared_ptr<Model> model)
    {
        modelCache.clear();

        if ((model != nullptr) && (type != MODEL_TYPE_OBJ)) {
            modelCache.put((static_cast<int64_t>(type) << 16) + id, model);
        }
    }

    void IfType::InventorySwap(int32_t src, int32_t dst)
    {
        int32_t tmp = inventorySlotObjID[src];
        inventorySlotObjID[src] = inventorySlotObjID[dst];
        inventorySlotObjID[dst] = tmp;

        tmp = inventorySlotObjCount[src];
        inventorySlotObjCount[src] = inventorySlotObjCount[dst];
        inventorySlotObjCount[dst] = tmp;
    }

    std::shared_ptr<Model> IfType::GetModel(int32_t category, int32_t id)
    {
        auto model = modelCache.get((static_cast<int64_t>(category) << 16) + id);

        if (model != nullptr) {
            return model;
        }

        if (category == MODEL_TYPE_NORMAL) {
            model = Model::TryGet(id);
        } else if (category == MODEL_TYPE_NPC) {
            model = NPCType::Get(id)->GetHeadModel();
        } else if (category == MODEL_TYPE_PLAYER) {
            model = Game::localPlayer->GetHeadModel();
        } else if (category == MODEL_TYPE_OBJ) {
            model = ObjType::Get(id)->GetModel(50);
        }

        if (model != nullptr) {
            modelCache.put((static_cast<int64_t>(category) << 16) + id, model);
        }

        return model;
    }

    std::shared_ptr<Model> IfType::GetModel(int32_t primaryTransformID, int32_t secondaryTransformID, bool active)
    {
        std::shared_ptr<Model> model;

        if (active) {
            model = GetModel(activeModelType, activeModelID);
        } else {
            model = GetModel(modelType, modelID);
        }

        if (model == nullptr) {
            return nullptr;
        }

        if ((primaryTransformID == -1) && (secondaryTransformID == -1) && (model->faceColor.empty())) {
            return model;
        }

        model = std::make_shared<Model>(true, SeqTransform::IsNull(primaryTransformID) && SeqTransform::IsNull(secondaryTransformID), false, *model);

        if ((primaryTransformID != -1) || (secondaryTransformID != -1)) {
            model->CreateLabelReferences();
        }

        if (primaryTransformID != -1) {
            model->ApplyTransform(primaryTransformID);
        }

        if (secondaryTransformID != -1) {
            model->ApplyTransform(secondaryTransformID);
        }

        model->CalculateNormals(64, 768, -50, -10, -50, true);
        return model;
    }
}