#include "ObjType.h"
#include "Game.h"
#include "StringUtil.h"

namespace SDL_Client
{
    static std::vector<std::shared_ptr<ObjType>> recent;

    LRUMap<int32_t, Model> ObjType::modelCache(50);
    LRUMap<int32_t, Image24> ObjType::iconCache(100);

    void ObjType::Unpack(FileArchive& archive)
    {
        dat = Buffer(archive.Read(("obj.dat")));
        Buffer idx(archive.Read("obj.idx"));
        count = idx.ReadU16();
        typeOffset.resize(count);
        int32_t offset = 2;
        for (int32_t j = 0; j < count; j++) {
            typeOffset[j] = offset;
            offset += idx.ReadU16();
        }
        recent.resize(10);
        for (int k = 0; k < 10; k++) {
            recent[k] = std::make_shared<ObjType>();
        }
    }

    void ObjType::Unload()
    {
        modelCache.clear();
        iconCache.clear();
        typeOffset.clear();
        recent.clear();
        dat.Clear();
    }

    std::shared_ptr<ObjType> ObjType::Get(int32_t id)
    {
        for (int32_t i = 0; i < 10; i++) {
            if (recent[i]->id == id) {
                return recent[i];
            }
        }

        recentPos = (recentPos + 1) % 10;
        const auto& type = recent[recentPos];
        dat.position = typeOffset[id];
        type->id = id;
        type->Reset();
        type->Read(dat);

        if (type->certificateID != -1) {
            type->ToCertificate();
        }

        if (!Game::members && type->members) {
            type->name = "Members Object";
            type->examine = "Login to a members' server to use this object.";
            type->options.clear();
            type->inventoryOptions.clear();
            type->team = 0;
        }

        return type;
    }

    std::shared_ptr<Image24> ObjType::GetIcon(int32_t id, int32_t amount, int32_t outlineColor)
    {
        if (outlineColor == 0) {
            auto icon = iconCache.get(id);

            if ((icon != nullptr) && (icon->cropH != amount) && (icon->cropH != -1)) {
                icon->unlink();
                icon = nullptr;
            }

            if (icon != nullptr) {
                return icon;
            }
        }

        auto type = Get(id);

        if (type->stackID.empty()) {
            amount = -1;
        }

        if (amount > 1) {
            int32_t newID = -1;
            for (int32_t stack = 0; stack < 10; stack++) {
                if ((amount >= type->stackCount[stack]) && (type->stackCount[stack] != 0)) {
                    newID = type->stackID[stack];
                }
            }
            if (newID != -1) {
                type = Get(newID);
            }
        }

        auto model = type->GetModel(1);

        if (model == nullptr) {
            return nullptr;
        }

        // this will be the original item icon to draw over the certificate icon (if present)
        std::shared_ptr<Image24> linkedIcon = nullptr;

        if (type->certificateID != -1) {
            linkedIcon = GetIcon(type->linkedID, 10, -1);

            if (linkedIcon == nullptr) {
                return nullptr;
            }
        }

        auto icon = std::make_shared<Image24>(32, 32);

        // store state
        int32_t _cx = Draw3D::centerX;
        int32_t _cy = Draw3D::centerY;
        std::vector<int32_t> _loff = Draw3D::lineOffset;
        SDL_Surface* _surface = Draw2D::surface;
        int32_t _l = Draw2D::left;
        int32_t _r = Draw2D::right;
        int32_t _t = Draw2D::top;
        int32_t _b = Draw2D::bottom;

        // set up drawing area
        Draw3D::jagged = false;
        Draw2D::Bind(icon->surface);
        // Don't call FillRect - pixels are already initialized to 0 (transparent)
        // FillRect would write 0xFF000000 (opaque black) instead
        Draw3D::Init2D();

        int32_t zoom = type->iconZoom;

        if (outlineColor == -1) {
            zoom = static_cast<int32_t>(static_cast<double>(zoom) * 1.5);
        }

        if (outlineColor > 0) {
            zoom = static_cast<int32_t>(static_cast<double>(zoom) * 1.04);
        }

        int32_t sinPitch = (Draw3D::sin[type->iconPitch] * zoom) >> 16;
        int32_t cosPitch = (Draw3D::cos[type->iconPitch] * zoom) >> 16;

        model->DrawSimple(0, type->iconYaw, type->iconRoll, type->iconPitch, type->iconOffsetX, sinPitch + (model->minY / 2) + type->iconOffsetY, cosPitch + type->iconOffsetY);

        // define outline (use uint32_t cast since pixels with alpha are negative as signed int)
        for (int32_t x = 31; x >= 0; x--) {
            for (int32_t y = 31; y >= 0; y--) {
                if (icon->pixels[x + (y * 32)] != 0) {
                    continue;
                }
                if ((x > 0) && (static_cast<uint32_t>(icon->pixels[(x - 1) + (y * 32)]) > 1)) {
                    icon->pixels[x + (y * 32)] = 1;
                } else if ((y > 0) && (static_cast<uint32_t>(icon->pixels[x + ((y - 1) * 32)]) > 1)) {
                    icon->pixels[x + (y * 32)] = 1;
                } else if ((x < 31) && (static_cast<uint32_t>(icon->pixels[x + 1 + (y * 32)]) > 1)) {
                    icon->pixels[x + (y * 32)] = 1;
                } else if ((y < 31) && (static_cast<uint32_t>(icon->pixels[x + ((y + 1) * 32)]) > 1)) {
                    icon->pixels[x + (y * 32)] = 1;
                }
            }
        }

        // color outline
        if (outlineColor > 0) {
            for (int32_t x = 31; x >= 0; x--) {
                for (int32_t y = 31; y >= 0; y--) {
                    if (icon->pixels[x + (y * 32)] == 0) {
                        if ((x > 0) && (icon->pixels[(x - 1) + (y * 32)] == 1)) {
                            icon->pixels[x + (y * 32)] = 0xFF000000 | outlineColor;
                        } else if ((y > 0) && (icon->pixels[x + ((y - 1) * 32)] == 1)) {
                            icon->pixels[x + (y * 32)] = 0xFF000000 | outlineColor;
                        } else if ((x < 31) && (icon->pixels[x + 1 + (y * 32)] == 1)) {
                            icon->pixels[x + (y * 32)] = 0xFF000000 | outlineColor;
                        } else if ((y < 31) && (icon->pixels[x + ((y + 1) * 32)] == 1)) {
                            icon->pixels[x + (y * 32)] = 0xFF000000 | outlineColor;
                        }
                    }
                }
            }
        }
        // default outline (shadow)
        else if (outlineColor == 0) {
            for (int32_t x = 31; x >= 0; x--) {
                for (int32_t y = 31; y >= 0; y--) {
                    if ((icon->pixels[x + (y * 32)] == 0) && (x > 0) && (y > 0) && (icon->pixels[(x - 1) + ((y - 1) * 32)] != 0)) {
                        icon->pixels[x + (y * 32)] = 0xFF302020;
                    }
                }
            }
        }

        // ensure outline markers have proper alpha
        for (int32_t i = 0; i < 32 * 32; i++) {
            if (icon->pixels[i] == 1) {
                icon->pixels[i] = 0xFF000001;
            }
        }

        if (type->certificateID != -1) {
            int32_t w = linkedIcon->cropW;
            int32_t h = linkedIcon->cropH;
            linkedIcon->cropW = 32;
            linkedIcon->cropH = 32;
            linkedIcon->Draw(0, 0);
            linkedIcon->cropW = w;
            linkedIcon->cropH = h;
        }

        if (outlineColor == 0) {
            iconCache.put(id, icon);
        }

        // restore state
        Draw2D::Bind(_surface);
        Draw2D::SetBounds(_l, _t, _r, _b);
        Draw3D::centerX = _cx;
        Draw3D::centerY = _cy;
        Draw3D::lineOffset = _loff;
        Draw3D::jagged = true;

        if (type->stackable) {
            icon->cropW = 33;
        } else {
            icon->cropW = 32;
        }

        icon->cropH = amount;
        return icon;
    }

    /**
     * Retrieves a fully built ground model of this {@link ObjType}.
     *
     * @param count the stack count.
     * @return the model or <code>null</code> if unavailable.
     */
    std::shared_ptr<Model> ObjType::GetModel(int32_t count)
    {
        if ((!stackID.empty()) && (count > 1)) {
            int32_t id = -1;

            for (int32_t i = 0; i < 10; i++) {
                if ((count >= stackCount[i]) && (stackCount[i] != 0)) {
                    id = stackID[i];
                }
            }

            if (id != -1) {
                return Get(id)->GetModel(1);
            }
        }

        std::shared_ptr<Model> model = modelCache.get(id);
        if (model != nullptr) {
            return model;
        }

        model = Model::TryGet(modelID);

        if (model == nullptr) {
            return nullptr;
        }

        if ((scaleX != 128) || (scaleZ != 128) || (scaleY != 128)) {
            model->Scale(scaleX, scaleY, scaleZ);
        }

        if (!srcColor.empty()) {
            for (int32_t i = 0; i < srcColor.size(); i++) {
                model->Recolor(srcColor[i], dstColor[i]);
            }
        }

        model->CalculateNormals(64 + lightAmbient, 768 + lightAttenuation, -50, -10, -50, true);
        model->pickable = true;
        modelCache.put(id, model);
        return model;
    }

    bool ObjType::ValidateWornModel(int32_t gender)
    {
        int32_t modelID0 = maleModelID0;
        int32_t modelID1 = maleModelID1;
        int32_t modelID2 = maleModelID2;

        if (gender == 1) {
            modelID0 = femaleModelID0;
            modelID1 = femaleModelID1;
            modelID2 = femaleModelID2;
        }

        if (modelID0 == -1) {
            return true;
        }

        bool valid = Model::Validate(modelID0);

        if ((modelID1 != -1) && !Model::Validate(modelID1)) {
            valid = false;
        }

        if ((modelID2 != -1) && !Model::Validate(modelID2)) {
            valid = false;
        }

        return valid;
    }

    bool ObjType::ValidateHeadModel(int32_t gender)
    {
        int32_t modelID0 = maleHeadModelID0;
        int32_t modelID1 = maleHeadModelID1;

        if (gender == 1) {
            modelID0 = femaleHeadModelID0;
            modelID1 = femaleHeadModelID1;
        }

        if (modelID0 == -1) {
            return true;
        }

        bool valid = Model::Validate(modelID0);

        if ((modelID1 != -1) && !Model::Validate(modelID1)) {
            valid = false;
        }
        return valid;
    }

    std::shared_ptr<Model> ObjType::GetHeadModel(int32_t gender) const
    {
        int32_t modelID0 = maleHeadModelID0;
        int32_t modelID1 = maleHeadModelID1;

        if (gender == 1) {
            modelID0 = femaleHeadModelID0;
            modelID1 = femaleHeadModelID1;
        }

        if (modelID0 == -1) {
            return nullptr;
        }

        auto model = Model::TryGet(modelID0);

        if (model == nullptr) {
            return nullptr;
        }

        if (modelID1 != -1) {
            model = std::make_shared<Model>(2, std::vector{model, Model::TryGet(modelID1)});
        }

        if (!srcColor.empty()) {
            for (size_t i = 0; i < srcColor.size(); i++) {
                model->Recolor(srcColor[i], dstColor[i]);
            }
        }
        return model;
    }

    std::shared_ptr<Model> ObjType::GetWornModel(int32_t gender) const
    {
        int32_t modelID0 = maleModelID0;
        int32_t modelID1 = maleModelID1;
        int32_t modelID2 = maleModelID2;

        if (gender == 1) {
            modelID0 = femaleModelID0;
            modelID1 = femaleModelID1;
            modelID2 = femaleModelID2;
        }

        if (modelID0 == -1) {
            return nullptr;
        }

        auto model = Model::TryGet(modelID0);

        if (model == nullptr) {
            return nullptr;
        }

        if (modelID1 != -1) {
            if (modelID2 != -1) {
                model = std::make_shared<Model>(3, std::vector{model, Model::TryGet(modelID1), Model::TryGet(modelID2)});
            } else {
                model = std::make_shared<Model>(2, std::vector{model, Model::TryGet(modelID1)});
            }
        }

        if ((gender == 0) && (maleOffsetY != 0)) {
            model->Translate(0, maleOffsetY, 0);
        }

        if ((gender == 1) && (femaleOffsetY != 0)) {
            model->Translate(0, femaleOffsetY, 0);
        }

        if (!srcColor.empty()) {
            for (int32_t i = 0; i < srcColor.size(); i++) {
                model->Recolor(srcColor[i], dstColor[i]);
            }
        }

        return model;
    }

    void ObjType::Reset()
    {
        modelID = 0;
        name.clear();
        examine.clear();
        srcColor.clear();
        dstColor.clear();
        iconZoom = 2000;
        iconPitch = 0;
        iconYaw = 0;
        iconRoll = 0;
        iconOffsetX = 0;
        iconOffsetY = 0;
        stackable = false;
        cost = 1;
        members = false;
        options.clear();
        inventoryOptions.clear();
        maleModelID0 = -1;
        maleModelID1 = -1;
        maleOffsetY = 0;
        femaleModelID0 = -1;
        femaleModelID1 = -1;
        femaleOffsetY = 0;
        maleModelID2 = -1;
        femaleModelID2 = -1;
        maleHeadModelID0 = -1;
        maleHeadModelID1 = -1;
        femaleHeadModelID0 = -1;
        femaleHeadModelID1 = -1;
        stackID.clear();
        stackCount.clear();
        linkedID = -1;
        certificateID = -1;
        scaleX = 128;
        scaleZ = 128;
        scaleY = 128;
        lightAmbient = 0;
        lightAttenuation = 0;
        team = 0;
    }

    void ObjType::Read(Buffer& in)
    {
        while (true) {
            int32_t code = in.ReadU8();

            if (code == 0) {
                return;
            } else if (code == 1) {
                modelID = in.ReadU16();
            } else if (code == 2) {
                name = in.ReadString();
            } else if (code == 3) {
                examine = in.ReadString();
            } else if (code == 4) {
                iconZoom = in.ReadU16();
            } else if (code == 5) {
                iconPitch = in.ReadU16();
            } else if (code == 6) {
                iconYaw = in.ReadU16();
            } else if (code == 7) {
                iconOffsetX = in.ReadU16();
                if (iconOffsetX > 32767) {
                    iconOffsetX -= 0x10000;
                }
            } else if (code == 8) {
                iconOffsetY = in.ReadU16();
                if (iconOffsetY > 32767) {
                    iconOffsetY -= 0x10000;
                }
            } else if (code == 10) {
                in.ReadU16();
            } else if (code == 11) {
                stackable = true;
            } else if (code == 12) {
                cost = in.Read32();
            } else if (code == 16) {
                members = true;
            } else if (code == 23) {
                maleModelID0 = in.ReadU16();
                maleOffsetY = in.Read8();
            } else if (code == 24) {
                maleModelID1 = in.ReadU16();
            } else if (code == 25) {
                femaleModelID0 = in.ReadU16();
                femaleOffsetY = in.Read8();
            } else if (code == 26) {
                femaleModelID1 = in.ReadU16();
            } else if ((code >= 30) && (code < 35)) {
                if (options.empty()) {
                    options.resize(5);
                }
                options[code - 30] = in.ReadString();
                if (StringUtil::EqualsIgnoreCase(options[code - 30], "hidden")) {
                    options[code - 30] = std::string();
                }
            } else if ((code >= 35) && (code < 40)) {
                if (inventoryOptions.empty()) {
                    inventoryOptions.resize(5);
                }
                inventoryOptions[code - 35] = in.ReadString();
            } else if (code == 40) {
                int32_t recolorCount = in.ReadU8();
                srcColor.resize(recolorCount);
                dstColor.resize(recolorCount);
                for (int32_t i = 0; i < recolorCount; i++) {
                    srcColor[i] = in.ReadU16();
                    dstColor[i] = in.ReadU16();
                }
            } else if (code == 78) {
                maleModelID2 = in.ReadU16();
            } else if (code == 79) {
                femaleModelID2 = in.ReadU16();
            } else if (code == 90) {
                maleHeadModelID0 = in.ReadU16();
            } else if (code == 91) {
                femaleHeadModelID0 = in.ReadU16();
            } else if (code == 92) {
                maleHeadModelID1 = in.ReadU16();
            } else if (code == 93) {
                femaleHeadModelID1 = in.ReadU16();
            } else if (code == 95) {
                iconRoll = in.ReadU16();
            } else if (code == 97) {
                linkedID = in.ReadU16();
            } else if (code == 98) {
                certificateID = in.ReadU16();
            } else if ((code >= 100) && (code < 110)) {
                if (stackID.empty()) {
                    stackID.resize(10);
                    stackCount.resize(10);
                }
                stackID[code - 100] = in.ReadU16();
                stackCount[code - 100] = in.ReadU16();
            } else if (code == 110) {
                scaleX = in.ReadU16();
            } else if (code == 111) {
                scaleZ = in.ReadU16();
            } else if (code == 112) {
                scaleY = in.ReadU16();
            } else if (code == 113) {
                lightAmbient = in.Read8();
            } else if (code == 114) {
                lightAttenuation = in.Read8() * 5;
            } else if (code == 115) {
                team = in.ReadU8();
            }
        }
    }

    void ObjType::ToCertificate()
    {
        const auto& cert = Get(certificateID);
        modelID = cert->modelID;
        iconZoom = cert->iconZoom;
        iconPitch = cert->iconPitch;
        iconYaw = cert->iconYaw;
        iconRoll = cert->iconRoll;
        iconOffsetX = cert->iconOffsetX;
        iconOffsetY = cert->iconOffsetY;
        srcColor = cert->srcColor;
        dstColor = cert->dstColor;

        const auto& linked = Get(linkedID);
        name = linked->name;
        members = linked->members;
        cost = linked->cost;

        std::string s = "a";
        char c = linked->name.at(0);

        if ((c == 'A') || (c == 'E') || (c == 'I') || (c == 'O') || (c == 'U')) {
            s = "an";
        }

        examine = "Swap this note at any bank for " + s + " " + linked->name + ".";
        stackable = true;
    }
}
