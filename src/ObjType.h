#pragma once

#include "PCH.h"
#include "FileArchive.h"
#include "Buffer.h"
#include "Model.h"
#include "Image24.h"

namespace SDL_Client
{
    class ObjType
    {
    public:
        static void Unpack(FileArchive& archive);
        static void Unload();
        static std::shared_ptr<ObjType> Get(int32_t id);
        static std::shared_ptr<Image24> GetIcon(int32_t id, int32_t amount, int32_t outlineColor);
    public:
        std::shared_ptr<Model> GetModel(int32_t count);
        bool ValidateWornModel(int32_t gender);
        bool ValidateHeadModel(int32_t gender);
        std::shared_ptr<Model> GetHeadModel(int32_t gender) const;
        std::shared_ptr<Model> GetWornModel(int32_t gender) const;

        inline static int32_t count = 0;
        static LRUMap<int32_t, Model> modelCache;
        static LRUMap<int32_t, Image24> iconCache;

    public:

        std::vector<std::string> inventoryOptions;
        std::vector<std::string> options;

        std::string name;
        std::string examine;

        int32_t id = -1;
        int32_t cost = 0;
        int32_t team = 0;
        int32_t iconPitch = 0;
        int32_t iconYaw = 0;
        int32_t iconZoom = 0;

        bool stackable = false;
        bool members = false;

    private:
        void Reset();
        void Read(Buffer& in);
        void ToCertificate();

        inline static Buffer dat;
        inline static std::vector<int32_t> typeOffset;
        inline static int32_t recentPos = -1;

        std::vector<int32_t> srcColor;
        std::vector<int32_t> dstColor;
        std::vector<int32_t> stackID;
        std::vector<int32_t> stackCount;

        // ============ Private 32-bit integers ============
        int32_t certificateID = 0;
        int32_t modelID = 0;
        int32_t iconOffsetX = 0;
        int32_t iconOffsetY = 0;
        int32_t maleModelID0 = 0;
        int32_t maleModelID1 = 0;
        int32_t maleModelID2 = 0;
        int32_t femaleModelID0 = 0;
        int32_t femaleModelID1 = 0;
        int32_t femaleModelID2 = 0;
        int32_t maleHeadModelID0 = 0;
        int32_t maleHeadModelID1 = 0;
        int32_t femaleHeadModelID0 = 0;
        int32_t femaleHeadModelID1 = 0;
        int32_t iconRoll = 0;
        int32_t linkedID = 0;
        int32_t scaleX = 0;
        int32_t scaleY = 0;
        int32_t scaleZ = 0;
        int32_t lightAmbient = 0;
        int32_t lightAttenuation = 0;

        int8_t maleOffsetY = 0;
        int8_t femaleOffsetY = 0;
    };


}
