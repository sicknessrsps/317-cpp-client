#pragma once
#include "PCH.h"
#include "Buffer.h"
#include "Scene.h"
#include "LocType.h"
#include "Model.h"
#include "CollisionMap.h"

namespace SDL_Client {
    class SceneBuilder {
    public:
        SceneBuilder(
            Array3DIndexed<int8_t>& levelTileFlags,
            int32_t maxTileZ,
            int32_t maxTileX,
            Array3DIndexed<int32_t>& levelHeightmap
        );
        void ReadTiles(const std::vector<int8_t>& data, int32_t offsetZ, int32_t offsetX, int32_t originX,
            int32_t originZ, const std::vector<std::shared_ptr<CollisionMap>>& collisionMaps);
        void ReadChunkTiles(const std::vector<std::shared_ptr<CollisionMap>>& collisionMaps,
            const std::vector<int8_t>& data, int32_t chunkX, int32_t chunkZ, int32_t mapLevel, int32_t chunkRotation,
            int32_t originX, int32_t originZ, int32_t level);
        void Build(const std::vector<std::shared_ptr<CollisionMap>>& levelCollisionMaps, Scene& scene);
        void StitchHeightmap(int32_t tileX, int32_t tileZ, int32_t tileSizeX, int32_t tileSizeZ);
        void ReadLocs(const std::vector<std::shared_ptr<CollisionMap>>& collisionMaps, Scene& scene,
            int32_t originX, int32_t originZ, const std::vector<int8_t>& data);
        void ReadChunkLocs(const std::vector<std::shared_ptr<CollisionMap>>& collisionMaps, Scene& scene,
            int32_t mapLevel, int32_t mapRotation, int32_t mapChunkX, int32_t mapChunkZ, int32_t originX,
            int32_t originZ, const std::vector<int8_t>& data, int32_t level);
        static void PrefetchLocs(std::shared_ptr<Buffer> buffer, OnDemand& onDemand);
        static bool IsLocReady(int32_t locID, int32_t kind);
        static void AddLoc(Scene& scene, int32_t rotation, int32_t z, int32_t type, int32_t tileLevel,
            const std::shared_ptr<CollisionMap>& collision, Array3DIndexed<int32_t>& levelHeightmap,
            int32_t x, int32_t locID, int32_t level);
        static bool ValidateLocs(const std::vector<int8_t>& data, int32_t originX, int32_t originZ);
    private:
        static void AddWallStraight(Scene& scene, int32_t rotation, int32_t z, int32_t kind,
            const std::shared_ptr<CollisionMap>& collision, int32_t x, int32_t locID, int32_t level, int32_t heightSW,
            int32_t heightSE, int32_t heightNE, int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info);
        void ReadTiles(Buffer& in, int32_t originX, int32_t originZ, int32_t level, int32_t x, int32_t z, int32_t mapRotation);
        void BuildTiles(Scene& scene, int32_t level);
        void BuildLandscapeLighting(int32_t level);
        void UpdateDrawLevels(Scene& scene, int32_t level) const;
        void BuildOccluders();
        void BuildBridges(Scene& scene) const;
        void BuildFloorOccluders(int32_t floor, int32_t topLevel, int32_t level, int32_t tileX, int32_t tileZ);
        void AddLoc(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind,
            int32_t rotation, int32_t level, int32_t x, int32_t z);
        void BuildWallOccludersX(int32_t wall0, int32_t topLevel, int32_t level, int32_t tileX, int32_t tileZ);
        void BuildWallOccludersZ(int32_t wall1, int32_t topLevel, int32_t level, int32_t tileX, int32_t tileZ);
        void AddLoc(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind,
            int32_t rotation, int32_t level, int32_t x, int32_t z,
            int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW,
            int32_t heightmapAverage, LocType& loc, int32_t bitset, int8_t info);
        static void AddLoc(Scene& scene, int32_t rotation, int32_t z, int32_t kind, const std::shared_ptr<CollisionMap>& collision,
            int32_t x, int32_t locID, int32_t level, int32_t heightSW, int32_t heightSE, int32_t heightNE,
            int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info);
        static void AddGroundDecoration(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t rotation,
            int32_t level, int32_t x, int32_t z, int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE,
            int32_t heightmapNW, int32_t heightmapAverage, LocType& type, int32_t bitset, int8_t info);
        static void AddGroundDecoration(Scene& scene, int32_t rotation, int32_t z, const std::shared_ptr<CollisionMap>& collision,
            int32_t x, int32_t locID, int32_t level, int32_t heightSW, int32_t heightSE, int32_t heightNE,
            int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info);
        void AddRoof(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind, int32_t rotation,
            int32_t level, int32_t x, int32_t z, int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE,
            int32_t heightmapNW, int32_t heightmapAverage, LocType& type, int32_t bitset, int8_t info);
        void AddWall(Scene& scene, const std::shared_ptr<CollisionMap>& collision,
            int32_t locID, int32_t kind, int32_t rotation, int32_t level, int32_t x, int32_t z, int32_t heightmapSW,
            int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW, int32_t heightmapAverage,
            LocType& type, int32_t bitset, int8_t info);
        static void AddWallL(Scene& scene, int32_t rotation, int32_t z, int32_t kind, const std::shared_ptr<CollisionMap>& collision,
            int32_t x, int32_t locID, int32_t level, int32_t heightSW, int32_t heightSE, int32_t heightNE,
            int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info);
        void AddWallCornerDiagonal(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind, int32_t rotation,
            int32_t level, int32_t x, int32_t z, int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW,
            int32_t heightmapAverage, LocType& type, int32_t bitset, int8_t info);
        static void AddWallCornerDiagonal(Scene& scene, int32_t rotation, int32_t z, int32_t kind,
            const std::shared_ptr<CollisionMap>& collision, int32_t x, int32_t locID, int32_t level, int32_t heightSW,
            int32_t heightSE, int32_t heightNE, int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info);
        void AddFullWall(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind, int32_t rotation,
            int32_t level, int32_t x, int32_t z, int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE,
            int32_t heightmapNW, int32_t heightmapAverage, LocType& type, int32_t bitset, int8_t info);
        void ApplyBlockFlags(const std::vector<std::shared_ptr<CollisionMap>>& levelCollisionMaps) const;
        void AddWallSquareCorner(Scene& scene, const std::shared_ptr<CollisionMap>& collision,
                         int32_t locID, int32_t kind, int32_t rotation, int32_t level,
                         int32_t x, int32_t z, int32_t heightmapSW, int32_t heightmapSE,
                         int32_t heightmapNE, int32_t heightmapNW, int32_t heightmapAverage,
                         LocType& type, int32_t bitset, int8_t info);
        static void AddWallSquareCorner(Scene& scene, int32_t rotation, int32_t z, int32_t kind,
            const std::shared_ptr<CollisionMap>& collision, int32_t x, int32_t locID, int32_t level, int32_t heightSW,
            int32_t heightSE, int32_t heightNE, int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info);
        static void AddRoofOrDiagonalWall(Scene& scene, const std::shared_ptr<CollisionMap>& collision,
                           int32_t locID, int32_t kind, int32_t rotation, int32_t level,
                           int32_t x, int32_t z, int32_t heightmapSW, int32_t heightmapSE,
                           int32_t heightmapNE, int32_t heightmapNW, int32_t heightmapAverage,
                           LocType& type, int32_t bitset, int8_t info);
        static void AddRoofOrDiagonalWall(Scene& scene, int32_t rotation, int32_t z, int32_t kind,
            const std::shared_ptr<CollisionMap>& collision, int32_t x, int32_t locID, int32_t level, int32_t heightSW,
            int32_t heightSE, int32_t heightNE, int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info);
        static void AddWallDecor(Scene& scene, int32_t decorType, LocType& type,
                      int32_t locID, int32_t locBitset, int8_t locInfo, int32_t level,
                      int32_t x, int32_t z, int32_t rotation,
                      int32_t heightmapSW, int32_t heightmapSE,
                      int32_t heightmapNE, int32_t heightmapNW, int32_t heightmapAverage);
        static void AddWallDecorOffset(Scene& scene, int32_t rotation, int32_t z, int32_t x, int32_t locID,
                        int32_t level, int32_t heightSW, int32_t heightSE, int32_t heightNE,
                        int32_t heightNW, int32_t y, LocType& loc,
                        int32_t bitset, int8_t info);
        [[nodiscard]] int32_t GetDrawLevel(int32_t level, int32_t stx, int32_t stz) const;
        static int32_t AdjustLightness(int32_t hsl, int32_t scalar);
        static int32_t DecimateHSL(int32_t hue, int32_t saturation, int32_t lightness);
        static int32_t MulHSL(int32_t hsl, int32_t lightness);
        static int32_t Perlin(int32_t x, int32_t z);
        static int32_t Perlin(int32_t x, int32_t z, int32_t scale);
        static int32_t SmoothNoise(int32_t x, int32_t y);
        static int32_t Noise(int32_t x, int32_t y);
        static int32_t Interpolate(int32_t a, int32_t b, int32_t x, int32_t scale);
    public:
        inline static bool lowmem = false;
        inline static int32_t minLevel = 99;
        inline static int32_t curLevel = 0;
    private:
        Array3DIndexed<int8_t>& levelTileFlags;
        Array3DIndexed<int32_t>& levelHeightmap;

        Array3DIndexed<int8_t> levelTileUnderlayIDs;
        Array3DIndexed<int8_t> levelTileOverlayIDs;
        Array3DIndexed<int8_t> levelTileOverlayShape;
        Array3DIndexed<int8_t> levelTileOverlayRotation;
        Array3DIndexed<int8_t> levelShademap;
        Array3DIndexed<int8_t> levelOccludemap;

        Array2DIndexed<int32_t> levelLightmap;

        std::vector<int32_t> blendChroma;
        std::vector<int32_t> blendSaturation;
        std::vector<int32_t> blendLightness;
        std::vector<int32_t> blendLuminance;
        std::vector<int32_t> blendMagnitude;


        int32_t maxTileZ = 0;
        int32_t maxTileX = 0;

        static constexpr int32_t WALL_DECORATION_ROTATION_FORWARD_X[4] = {1, 0, -1, 0};
        static constexpr int32_t WALL_DECORATION_ROTATION_FORWARD_Z[4] = {0, -1, 0, 1};
    };

}
