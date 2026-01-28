#pragma once

#include "PCH.h"
#include "FileArchive.h"
#include "Buffer.h"
#include "Model.h"
#include "Image24.h"
#include "BitmapFont.h"
#include "LRUMap.h"

namespace SDL_Client
{
    class Game;

    class IfType
    {
    public:
        static constexpr int32_t TYPE_PARENT = 0;
        static constexpr int32_t TYPE_UNUSED = 1;
        static constexpr int32_t TYPE_INVENTORY = 2;
        static constexpr int32_t TYPE_RECT = 3;
        static constexpr int32_t TYPE_TEXT = 4;
        static constexpr int32_t TYPE_IMAGE = 5;
        static constexpr int32_t TYPE_MODEL = 6;
        static constexpr int32_t TYPE_INVENTORY_TEXT = 7;

        static constexpr int32_t OPTION_TYPE_STANDARD = 1;
        static constexpr int32_t OPTION_TYPE_SPELL = 2;
        static constexpr int32_t OPTION_TYPE_CLOSE = 3;
        static constexpr int32_t OPTION_TYPE_TOGGLE = 4;
        static constexpr int32_t OPTION_TYPE_SELECT = 5;
        static constexpr int32_t OPTION_TYPE_CONTINUE = 6;

        static constexpr int32_t MODEL_TYPE_NONE = 0;
        static constexpr int32_t MODEL_TYPE_NORMAL = 1;
        static constexpr int32_t MODEL_TYPE_NPC = 2;
        static constexpr int32_t MODEL_TYPE_PLAYER = 3;
        static constexpr int32_t MODEL_TYPE_OBJ = 4;
        static constexpr int32_t MODEL_TYPE_PLAYER_DESIGN = 5;

    public:
        static void Unpack(FileArchive& config, const std::vector<std::shared_ptr<BitmapFont>>& fonts, FileArchive& media);
        static void CacheModel(int32_t id, int32_t type, std::shared_ptr<Model> model);

    public:
        void InventorySwap(int32_t src, int32_t dst);
        std::shared_ptr<Model> GetModel(int32_t category, int32_t id);
        std::shared_ptr<Model> GetModel(int32_t primaryTransformID, int32_t secondaryTransformID, bool active);

    public:
        static LRUMap<int64_t, Model> modelCache;
        static std::vector<std::shared_ptr<IfType>> instances;

        std::shared_ptr<Image24> activeImage;
        std::shared_ptr<Image24> image;
        std::shared_ptr<BitmapFont> font;

        std::vector<int32_t> childID;
        std::vector<int32_t> childX;
        std::vector<int32_t> childY;
        std::vector<std::string> inventoryOptions;
        std::vector<int32_t> inventorySlotObjCount;
        std::vector<std::shared_ptr<Image24>> inventorySlotImage;
        std::vector<int32_t> inventorySlotObjID;
        std::vector<int32_t> inventorySlotOffsetX;
        std::vector<int32_t> inventorySlotOffsetY;
        std::vector<int32_t> scriptComparator;
        std::vector<int32_t> scriptOperand;
        std::vector<std::vector<int32_t>> scripts;

        std::string activeText;
        std::string option;
        std::string spellAction;
        std::string spellName;
        std::string text;

        // ============ 32-bit integers ============
        int32_t activeColor = 0;
        int32_t activeHoverColor = 0;
        int32_t activeModelType = 0;
        int32_t activeModelID = 0;
        int32_t activeSeqID = -1;
        int32_t color = 0;
        int32_t contentType = 0;
        int32_t delegateHover = -1;
        int32_t height = 0;
        int32_t hoverColor = 0;
        int32_t id = 0;
        int32_t inventoryMarginX = 0;
        int32_t inventoryMarginY = 0;
        int32_t modelPitch = 0;
        int32_t modelType = 0;
        int32_t modelID = 0;
        int32_t modelYaw = 0;
        int32_t modelZoom = 0;
        int32_t optionType = 0;
        int32_t parentID = -1;
        int32_t scrollableHeight = 0;
        int32_t scrollPosition = 0;
        int32_t seqCycle = 0;
        int32_t seqFrame = 0;
        int32_t seqID = -1;
        int32_t spellFlags = 0;
        int32_t type = 0;
        int32_t width = 0;
        int32_t x = 0;
        int32_t y = 0;


        int8_t transparency = 0;
        bool inventoryInteractable = false;
        bool center = false;
        bool fill = false;

        bool hide = false;
        bool inventoryDraggable = false;
        bool inventoryMoveReplaces = false;
        bool inventoryUsable = false;
        bool shadow = false;


        static Game* game;

    private:
        static std::shared_ptr<Image24> GetImage(int32_t id, FileArchive& media, const std::string& name);
        static LRUMap<int64_t, Image24> imageCache;
    };
}