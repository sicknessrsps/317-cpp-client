#pragma once
#include "PCH.h"
#include "SceneTile.h"
#include "SceneOccluder.h"
#include "Entity.h"
#include "SceneLoc.h"

namespace SDL_Client {

    class Scene {
    public:
        Scene(int32_t maxTileZ, int32_t maxTileX, Array3DIndexed<int32_t>& levelHeightmaps, int32_t maxLevel);
        static void Init(int32_t viewportWidth, int32_t viewportHeight);
        void Draw(int32_t eyeX, int32_t eyeZ, int32_t eyeYaw, int32_t eyeY, int32_t topLevel, int32_t eyePitch);
        void SetTile(int32_t level, int32_t x, int32_t z, int32_t shape, int32_t rotation, int32_t textureID, int32_t southwestY,
            int32_t southeastY, int32_t northeastY, int32_t northwestY, int32_t southwestColor1, int32_t southeastColor1,
            int32_t northeastColor1, int32_t northwestColor1, int32_t southwestColor2, int32_t southeastColor2,
            int32_t northeastColor2, int32_t northwestColor2, int32_t backgroundRGB, int32_t foregroundRGB);
        void SetMinLevel(int32_t level);
        inline void DrawTile(SceneTile& next, bool checkAdjacent);
        void DrawTileOverlay(int32_t tileX, int32_t sinEyePitch, int32_t sinEyeYaw, const SceneTileOverlay& overlay, int32_t cosEyePitch, int32_t tileZ, int32_t cosEyeYaw);
        void DrawTileUnderlay(const SceneTileUnderlay& underlay, int32_t level, int32_t sinEyePitch, int32_t cosEyePitch, int32_t sinEyeYaw, int32_t cosEyeYaw, int32_t tileX, int32_t tileZ);
        void SetDrawLevel(int32_t level, int32_t stx, int32_t stz, int32_t drawLevel);
        void SetWallDecoration(int32_t type, const std::shared_ptr<Entity>& entity, int32_t level, int32_t tileX, int32_t tileZ, int32_t y, int32_t rotation, int32_t offsetX, int32_t offsetZ, int32_t bitset, int8_t info);
        void SetWallDecorationOffset(int32_t level, int32_t stx, int32_t stz, int32_t offset);
        void Click(int32_t mouseX, int32_t mouseY);
        void Reset();
        bool Add(const std::shared_ptr<Entity>& entity, int32_t level, int32_t tileX, int32_t tileZ, int32_t y, int32_t width, int32_t length, int32_t yaw, int32_t bitset, int8_t info);
        bool Add(const std::shared_ptr<Entity>& entity, int32_t level, int32_t tileX, int32_t tileZ, int32_t tileSizeX, int32_t tileSizeZ, int32_t x, int32_t z, int32_t y,
            int32_t yaw, int32_t bitset, int8_t info, bool temporary);
        static void AddOccluder(int32_t level, int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ, int32_t type);
        void AddGroundDecoration(const std::shared_ptr<Entity>& entity, int32_t tileLevel, int32_t tileX, int32_t tileZ, int32_t y, int32_t bitset, int8_t info);
        void SetWall(int32_t typeA, const std::shared_ptr<Entity>& entityA, int32_t typeB,
            const std::shared_ptr<Entity>& entityB, int32_t level, int32_t tileX, int32_t tileZ, int32_t y, int32_t bitset, int8_t info);
        int32_t GetWallBitset(int32_t level, int32_t x, int32_t z);
        void BuildModels(int32_t lightAmbient, int32_t lightAttenuation, int32_t lightSrcX, int32_t lightSrcY, int32_t lightSrcZ);
        int32_t GetWallDecorationBitset(int32_t level, int32_t x, int32_t z);
        std::shared_ptr<SceneWallDecoration> GetWallDecoration(int32_t level, int32_t x, int32_t z);
        int32_t GetLocBitset(int32_t level, int32_t x, int32_t z);
        std::shared_ptr<SceneLoc> GetLoc(int32_t level, int32_t x, int32_t z);
        int32_t GetGroundDecorationBitset(int32_t level, int32_t x, int32_t z);
        std::shared_ptr<SceneGroundDecoration> GetGroundDecoration(int32_t z, int32_t x, int32_t level);
        int32_t GetInfo(int32_t level, int32_t x, int32_t z, int32_t bitset);
        std::shared_ptr<SceneWall> GetWall(int32_t level, int32_t x, int32_t z);
        void RemoveObjStack(int32_t level, int32_t x, int32_t z);
        void AddObjStack(const std::shared_ptr<Entity>& topObj, const std::shared_ptr<Entity>& bottomObj, const std::shared_ptr<Entity>& middleObj,
                         int32_t level, int32_t stx, int32_t stz, int32_t y, int32_t bitset);
        bool AddTemporary(const std::shared_ptr<Entity>& entity, int32_t level, int32_t x, int32_t z, int32_t y,
            int32_t yaw, int32_t bitset, bool forwardPadding, int32_t padding);
        bool AddTemporary(const std::shared_ptr<Entity>& entity, int32_t level, int32_t minTileX, int32_t minTileZ,
            int32_t maxTileX, int32_t maxTileZ, int32_t x, int32_t z, int32_t y, int32_t yaw, int32_t bitset);
        bool AddTemporary(Entity* entity, int32_t level, int32_t x, int32_t z, int32_t y,
            int32_t yaw, int32_t bitset, bool forwardPadding, int32_t padding);
        bool AddTemporary(Entity* entity, int32_t level, int32_t minTileX, int32_t minTileZ,
            int32_t maxTileX, int32_t maxTileZ, int32_t x, int32_t z, int32_t y, int32_t yaw, int32_t bitset);
        void RemoveWall(int32_t x, int32_t level, int32_t z);
        void RemoveWallDecoration(int32_t level, int32_t x, int32_t z);
        void RemoveGroundDecoration(int32_t level, int32_t x, int32_t z);
        void RemoveLoc(int32_t level, int32_t x, int32_t z);
        void RemoveLoc(const std::shared_ptr<SceneLoc>& loc);
        void ClearTemporaryLocs();
        void SetBridge(int32_t stx, int32_t stz);
        void DrawBridgeTile(int32_t tileX, int32_t tileZ, SceneTile& bridge);
        void DrawMinimapTile(std::vector<int32_t>& dst, int32_t offset, int32_t step, int32_t level, int32_t x, int32_t z);

        static void Unload();
    private:
        static bool TestPoint(int32_t y, int32_t z, int32_t x);
        static void InitVisibilityMatrix();
        bool DrawTileUnderlayOrOverlay(SceneTile& tile, int32_t x, int32_t z, int32_t level);
        static int32_t MulLightness(int32_t hsl, int32_t lightness);
        bool TileVisible(int32_t level, int32_t x, int32_t z);
        void UpdateActiveOccluders();
        bool Occluded(int32_t x, int32_t y, int32_t z);
        static bool PointInsideTriangle(int32_t x, int32_t y, int32_t y0, int32_t y1, int32_t y2, int32_t x0, int32_t x1, int32_t x2);
        bool LocVisible(int32_t level, int32_t minTileX, int32_t maxTileX, int32_t minTileZ, int32_t maxTileZ, int32_t y);
        void MergeLocNormals(int32_t level, int32_t tileSizeX, int32_t tileSizeZ, int32_t tileX, int32_t tileZ, Model& model);
        void MergeNormals(Model& modelA, Model& modelB, int32_t offsetX, int32_t offsetY, int32_t offsetZ, bool allowFaceRemoval);
        void MergeGroundDecorationNormals(int32_t tileX, int32_t level, Model& model, int32_t tileZ);
        bool WallVisible(int32_t level, int32_t tileX, int32_t tileZ, int32_t type);
        bool Visible(int32_t level, int32_t tileX, int32_t tileZ, int32_t y);
        void DrawWallDecor(int32_t allowWallTypes, SceneWallDecoration& decor, bool front) const;
        void DrawObjStack(std::shared_ptr<SceneObjStack> stack, int32_t offset) const;
    public:
        inline static int32_t cycle = 0;
        inline static int32_t eyeTileX = 0;
        inline static int32_t eyeTileZ = 0;
        inline static int32_t eyeX = 0;
        inline static int32_t eyeY = 0;
        inline static int32_t eyeZ = 0;
        inline static int32_t sinEyePitch = 0;
        inline static int32_t cosEyePitch = 0;
        inline static int32_t sinEyeYaw = 0;
        inline static int32_t cosEyeYaw = 0;
        inline static int32_t clickTileX = -1;
        inline static int32_t clickTileZ = -1;
        int32_t maxLevel = 0;
        int32_t maxTileX = 0;
        int32_t maxTileZ = 0;
        inline static int32_t minLevel = 0;

        inline static int32_t minDrawTileX = 0;
        inline static int32_t maxDrawTileX = 0;
        inline static int32_t minDrawTileZ = 0;
        inline static int32_t maxDrawTileZ = 0;

        static constexpr int32_t LEVEL_COUNT = 4;
        inline static std::vector<int32_t> levelOccluderCount = std::vector<int32_t>(LEVEL_COUNT);
        inline static Array2DIndexed<SceneOccluder> levelOccluders = Array2DIndexed<SceneOccluder>(LEVEL_COUNT, 1000);
        inline static std::vector<SceneOccluder> activeOccluders = std::vector<SceneOccluder>(500);
        inline static int32_t activeOccluderCount = 0;

        inline static int32_t tilesRemaining = 0;
        inline static int32_t topLevel = 0;
        inline static bool lowmem = false;

        /**
         * Occlusion flags for walls of kind 0, 2 and decorations 4, 5,
         */
        static constexpr int32_t ROTATION_WALL_TYPE[4] = {
            1 << 0,
            1 << 1,
            1 << 2,
            1 << 3,
        };
        /**
         * Occlusion flags for walls of type 1 and 3.
         */
        static constexpr int32_t ROTATION_WALL_CORNER_TYPE[4] = {
            1 << 4,
            1 << 5,
            1 << 6,
            1 << 7,
        };
        static constexpr int32_t FRONT_WALL_TYPES[9] = {
            0b00010011, // eyeTileX >  tileX  &&  eyeTileZ <  tileZ
            0b00110111, // eyeTileX == tileX  &&  eyeTileZ <  tileZ
            0b00100110, // eyeTileX <  tileX  &&  eyeTileZ <  tileZ
            0b10011011, // eyeTileX >  tileX  &&  eyeTileZ == tileZ
            0b11111111, // eyeTileX == tileX  &&  eyeTileZ == tileZ
            0b01101110, // eyeTileX <  tileX  &&  eyeTileZ == tileZ
            0b10001001, // eyeTileX >  tileX  &&  eyeTileZ >  tileZ
            0b11001101, // eyeTileX =  tileX  &&  eyeTileZ >  tileZ
            0b01001100, // eyeTileX <  tileX  &&  eyeTileZ >  tileZ
        };
        static constexpr int32_t DIRECTION_ALLOW_WALL_CORNER_TYPE[9] = {
            0b10100000, // eyeTileX >  tileX  &&  eyeTileZ <  tileZ
            0b11000000, // eyeTileX == tileX  &&  eyeTileZ <  tileZ
            0b01010000, // eyeTileX <  tileX  &&  eyeTileZ <  tileZ
            0b01100000, // eyeTileX >  tileX  &&  eyeTileZ == tileZ
            0b00000000, // eyeTileX =  tileX  &&  eyeTileZ == tileZ
            0b10010000, // eyeTileX <  tileX  &&  eyeTileZ == tileZ
            0b01010000, // eyeTileX >  tileX  &&  eyeTileZ >  tileZ
            0b00110000, // eyeTileX =  tileX  &&  eyeTileZ >  tileZ
            0b10100000, // eyeTileX <  tileX  &&  eyeTileZ >  tileZ
    };
    static constexpr int32_t BACK_WALL_TYPES[9] = {
            0b01001100, // eyeTileX >  tileX  &&  eyeTileZ <  tileZ
            0b00001000, // eyeTileX == tileX  &&  eyeTileZ <  tileZ
            0b10001001, // eyeTileX <  tileX  &&  eyeTileZ <  tileZ
            0b00000100, // eyeTileX >  tileX  &&  eyeTileZ == tileZ
            0b00000000, // eyeTileX =  tileX  &&  eyeTileZ == tileZ
            0b00000001, // eyeTileX <  tileX  &&  eyeTileZ == tileZ
            0b00100110, // eyeTileX >  tileX  &&  eyeTileZ >  tileZ
            0b00000010, // eyeTileX =  tileX  &&  eyeTileZ >  tileZ
            0b00010011, // eyeTileX <  tileX  &&  eyeTileZ >  tileZ
    };
    static constexpr int32_t WALL_CORNER_TYPE_16_BLOCK_LOC_SPANS[9] = {
            0b0000, // eyeTileX >  tileX  &&  eyeTileZ <  tileZ
            0b0000, // eyeTileX == tileX  &&  eyeTileZ <  tileZ
            0b0010, // eyeTileX <  tileX  &&  eyeTileZ <  tileZ
            0b0000, // eyeTileX >  tileX  &&  eyeTileZ == tileZ
            0b0000, // eyeTileX =  tileX  &&  eyeTileZ == tileZ
            0b0010, // eyeTileX <  tileX  &&  eyeTileZ == tileZ
            0b0001, // eyeTileX >  tileX  &&  eyeTileZ >  tileZ
            0b0001, // eyeTileX =  tileX  &&  eyeTileZ >  tileZ
            0b0000, // eyeTileX <  tileX  &&  eyeTileZ >  tileZ
    };
    static constexpr int32_t WALL_CORNER_TYPE_32_BLOCK_LOC_SPANS[9] = {
            0b0010, // eyeTileX >  tileX  &&  eyeTileZ <  tileZ
            0b0000, // eyeTileX == tileX  &&  eyeTileZ <  tileZ
            0b0000, // eyeTileX <  tileX  &&  eyeTileZ <  tileZ
            0b0010, // eyeTileX >  tileX  &&  eyeTileZ == tileZ
            0b0000, // eyeTileX =  tileX  &&  eyeTileZ == tileZ
            0b0000, // eyeTileX <  tileX  &&  eyeTileZ == tileZ
            0b0000, // eyeTileX >  tileX  &&  eyeTileZ >  tileZ
            0b0100, // eyeTileX =  tileX  &&  eyeTileZ >  tileZ
            0b0100, // eyeTileX <  tileX  &&  eyeTileZ >  tileZ
    };
    static constexpr int32_t WALL_CORNER_TYPE_64_BLOCK_LOC_SPANS[9] = {
            0b0000, // eyeTileX >  tileX  &&  eyeTileZ <  tileZ
            0b0100, // eyeTileX == tileX  &&  eyeTileZ <  tileZ
            0b0100, // eyeTileX <  tileX  &&  eyeTileZ <  tileZ
            0b1000, // eyeTileX >  tileX  &&  eyeTileZ == tileZ
            0b0000, // eyeTileX =  tileX  &&  eyeTileZ == tileZ
            0b0000, // eyeTileX <  tileX  &&  eyeTileZ == tileZ
            0b1000, // eyeTileX >  tileX  &&  eyeTileZ >  tileZ
            0b0000, // eyeTileX =  tileX  &&  eyeTileZ >  tileZ
            0b0000, // eyeTileX <  tileX  &&  eyeTileZ >  tileZ
    };
    static constexpr int32_t WALL_CORNER_TYPE_128_BLOCK_LOC_SPANS[9] = {
            0b0001, // eyeTileX >  tileX  &&  eyeTileZ <  tileZ
            0b0001, // eyeTileX == tileX  &&  eyeTileZ <  tileZ
            0b0000, // eyeTileX <  tileX  &&  eyeTileZ <  tileZ
            0b0000, // eyeTileX >  tileX  &&  eyeTileZ == tileZ
            0b0000, // eyeTileX =  tileX  &&  eyeTileZ == tileZ
            0b1000, // eyeTileX <  tileX  &&  eyeTileZ == tileZ
            0b0000, // eyeTileX >  tileX  &&  eyeTileZ >  tileZ
            0b0000, // eyeTileX =  tileX  &&  eyeTileZ >  tileZ
            0b1000, // eyeTileX <  tileX  &&  eyeTileZ >  tileZ
    };

    static constexpr int32_t MINIMAP_TILE_MASK[13][16] = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1},
        {1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1},
        {0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0},
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1},
        {1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1}
    };
    static constexpr int32_t MINIMAP_TILE_ROTATION_MAP[4][16] = {
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
        {12, 8, 4, 0, 13, 9, 5, 1, 14, 10, 6, 2, 15, 11, 7, 3},
        {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
        {3, 7, 11, 15, 2, 6, 10, 14, 1, 5, 9, 13, 0, 4, 8, 12}
    };

    static constexpr int32_t WALL_DECORATION_INSET_X[4] = {53, -53, -53, 53};
    static constexpr int32_t WALL_DECORATION_INSET_Z[4] = {-53, -53, 53, 53};
    static constexpr int32_t WALL_DECORATION_OUTSET_X[4] = {-45, 45, 45, -45};
    static constexpr int32_t WALL_DECORATION_OUTSET_Z[4] = {45, 45, -45, -45};
    private:
        inline static bool takingInput = false;
        inline static int32_t mouseX = 0;
        inline static int32_t mouseY = 0;

        inline static int32_t activeWallOccluderCount = 0;
        inline static int32_t activeGroundOccluderCount = 0;
        inline static int32_t tilesCulled = 0;

        Array3DIndexed<SceneTile> levelTiles;

        Array3DIndexed<int32_t>& levelHeightmaps;
        Array2DView<uint8_t> visibilityMap;

        inline static std::vector<std::shared_ptr<SceneLoc>> locBuffer = std::vector<std::shared_ptr<SceneLoc>>(100);
        std::vector<std::shared_ptr<SceneLoc>> temporaryLocs = std::vector<std::shared_ptr<SceneLoc>>(5000);
        int32_t temporaryLocCount = 0;

        inline static DoublyLinkedList drawTileQueue;

        Array3DIndexed<int32_t> levelTileOcclusionCycles;

        inline static int32_t viewportCenterX = 0;
        inline static int32_t viewportCenterY = 0;
        inline static int32_t viewportLeft = 0;
        inline static int32_t viewportTop = 0;
        inline static int32_t viewportRight = 0;
        inline static int32_t viewportBottom = 0;

        std::vector<int32_t> mergeIndexA = std::vector<int32_t>(10000);
        std::vector<int32_t> mergeIndexB = std::vector<int32_t>(10000);

        int32_t tmpMergeIndex = 0;
    };

}