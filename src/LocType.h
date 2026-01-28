#pragma once

#include "PCH.h"
#include "FileArchive.h"
#include "Buffer.h"
#include "Model.h"

namespace SDL_Client
{

    class LocType
    {
    public:
        static std::shared_ptr<LocType> Get(int32_t locID);
        static void Unpack(FileArchive& archive);
        std::shared_ptr<Model> GetModel(int32_t kind, int32_t rotation, int32_t heightmapSW,
            int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW, int32_t transformID);
        std::shared_ptr<LocType> GetOverrideType();
        static void Unload();
        void Prefetch(OnDemand& onDemand);
        bool Validate(int32_t kind) const;
        bool Validate() const;

        static LRUMap<int64_t, Model> modelCacheDynamic;
        static LRUMap<int64_t, Model> modelCacheStatic;
        static Game* game;


        inline static int32_t TYPE_WALL_STRAIGHT = 0;
        inline static int32_t TYPE_WALL_CORNER_DIAGONAL = 1;
        inline static int32_t TYPE_WALL_L = 2;
        inline static int32_t TYPE_WALL_SQUARE_CORNER = 3;
        inline static int32_t TYPE_WALLDECOR_STRAIGHT = 4;
        inline static int32_t TYPE_WALLDECOR_STRAIGHT_OFFSET = 5;
        inline static int32_t TYPE_WALLDECOR_DIAGONAL_NOOFFSET = 6;
        inline static int32_t TYPE_WALLDECOR_DIAGONAL_OFFSET = 7;
        inline static int32_t TYPE_WALLDECOR_DIAGONAL_BOTH = 8;
        inline static int32_t TYPE_WALL_DIAGONAL = 9;
        inline static int32_t TYPE_CENTREPIECE = 10;
        inline static int32_t TYPE_CENTREPIECE_DIAGONAL = 11;
        inline static int32_t TYPE_ROOF_STRAIGHT = 12;
        inline static int32_t TYPE_ROOF_DIAGONAL = 13;
        inline static int32_t TYPE_ROOF_DIAGONAL_WITH_ROOFEDGE = 14;
        inline static int32_t TYPE_ROOF_L_CONCAVE = 15;
        inline static int32_t TYPE_ROOF_L_CONVEX = 16;
        inline static int32_t TYPE_ROOF_FLAT = 17;
        inline static int32_t TYPE_ROOFEDGE_STRAIGHT = 18;
        inline static int32_t TYPE_ROOFEDGE_DIAGONALCORNER = 19;
        inline static int32_t TYPE_ROOFEDGE_L = 20;
        inline static int32_t TYPE_ROOFEDGE_SQUARECORNER = 21;
        inline static int32_t TYPE_GROUND_DECOR = 22;

    public:
        std::vector<int32_t> overrideTypeIDs;
        std::vector<std::string> options;

        std::string name;
        std::string examine;

        int32_t seqID = -1;
        int32_t sizeX = 1;
        int32_t sizeZ = 1;
        int32_t index = -1;
        int32_t decorOffset = 0;
        int32_t varbit = 0;
        int32_t varp = 0;
        int32_t mapsceneIcon = 0;
        int32_t mapfunctionIcon = 0;
        int32_t interactionSideFlags = 0;

        bool interactable = true;
        bool adjustToTerrain = false;
        bool castShadow = true;
        bool important = false;
        bool occludes = false;
        bool solid = true;
        bool lowmem = false;
        bool blocksProjectiles = false;

    private:
        void Read(Buffer& buffer);
        void Reset();
        std::shared_ptr<Model> GetModel(int32_t kind, int32_t transformID, int32_t rotation);

        inline static int32_t count;
        inline static std::vector<int32_t> offsets;
        inline static int32_t cachePos;

        std::vector<int32_t> modelKinds;
        std::vector<int32_t> modelIDs;
        std::vector<int32_t> srcColor;
        std::vector<int32_t> dstColor;
        std::vector<std::shared_ptr<Model>> TMP_MODELS = std::vector<std::shared_ptr<Model>>(4);

        int32_t lightAmbient = 0;
        int32_t lightAttenuation = 0;
        int32_t scaleX = 0;
        int32_t scaleZ = 0;
        int32_t scaleY = 0;
        int32_t translateX = 0;
        int32_t translateY = 0;
        int32_t translateZ = 0;
        int32_t supportsObj = 0;

        bool dynamic = false;
        bool invert = false;
        bool decorative = false;
    };

}
