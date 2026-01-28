#include "SceneBuilder.h"
#include "Draw3D.h"
#include "FloType.h"
#include "LocEntity.h"
#include "MapUtil.h"
#include "ScopedTimer.h"

namespace SDL_Client {
    SceneBuilder::SceneBuilder(Array3DIndexed<int8_t> &levelTileFlags, int32_t maxTileZ,
    int32_t maxTileX, Array3DIndexed<int32_t> &levelHeightmap)
    : levelTileFlags(levelTileFlags),
      levelHeightmap(levelHeightmap),
      maxTileZ(maxTileZ),
      maxTileX(maxTileX)
    {
        levelTileUnderlayIDs.resize(4, maxTileX, maxTileZ);
        levelTileOverlayIDs.resize(4, maxTileX, maxTileZ);
        levelTileOverlayShape.resize(4, maxTileX, maxTileZ);
        levelTileOverlayRotation.resize(4, maxTileX, maxTileZ);

        levelShademap.resize(4,maxTileX + 1, maxTileZ + 1);
        levelOccludemap.resize(4,maxTileX + 1, maxTileZ + 1);
        levelLightmap.resize(maxTileX + 1, maxTileZ + 1);

        blendChroma.resize(maxTileZ);
        blendSaturation.resize(maxTileZ);
        blendLightness.resize(maxTileZ);
        blendLuminance.resize(maxTileZ);
        blendMagnitude.resize(maxTileZ);
    }

    void SceneBuilder::ReadTiles(const std::vector<int8_t>& data, int32_t offsetZ, int32_t offsetX, int32_t originX,
        int32_t originZ, const std::vector<std::shared_ptr<CollisionMap>>& collisionMaps) {

        for (int32_t level = 0; level < 4; level++) {
            for (int32_t x = 0; x < 64; x++) {
                for (int32_t z = 0; z < 64; z++) {
                    if (((offsetX + x) > 0) && ((offsetX + x) < 103) && ((offsetZ + z) > 0) && ((offsetZ + z) < 103)) {
                        collisionMaps[level]->flags[offsetX + x][offsetZ + z] &= ~CollisionMap::FLAG_UNINITIALIZED;
                    }
                }
            }
        }
        Buffer in(data);
        for (int32_t level = 0; level < 4; level++) {
            for (int32_t x = 0; x < 64; x++) {
                for (int32_t z = 0; z < 64; z++) {
                    ReadTiles(in, originX, originZ, level, x + offsetX, z + offsetZ, 0);
                }
            }
        }
    }

    void SceneBuilder::ReadChunkTiles(const std::vector<std::shared_ptr<CollisionMap>>& collisionMaps,
        const std::vector<int8_t>& data, int32_t chunkX, int32_t chunkZ, int32_t mapLevel, int32_t chunkRotation,
        int32_t originX, int32_t originZ, int32_t level)
    {
        for (int32_t dx = 0; dx < 8; dx++) {
            for (int32_t dz = 0; dz < 8; dz++) {
                if (((originX + dx) > 0) && ((originX + dx) < 103) && ((originZ + dz) > 0) && ((originZ + dz) < 103)) {
                    collisionMaps[level]->flags[originX + dx][originZ + dz] &= ~CollisionMap::FLAG_UNINITIALIZED;
                }
            }
        }
        Buffer in(data);
        for (int32_t l = 0; l < 4; l++) {
            for (int32_t x = 0; x < 64; x++) {
                for (int32_t z = 0; z < 64; z++) {
                    if ((l == mapLevel) && (x >= chunkX) && (x < (chunkX + 8)) && (z >= chunkZ) && (z < (chunkZ + 8))) {
                        ReadTiles(in, 0, 0, level, originX + MapUtil::RotateX(x & 0x7, z & 0x7, chunkRotation),
                            originZ + MapUtil::RotateZ(x & 0x7, z & 0x7, chunkRotation), chunkRotation);
                    } else {
                        ReadTiles(in, 0, 0, 0, -1, -1, 0);
                    }
                }
            }
        }
    }

    void SceneBuilder::Build(const std::vector<std::shared_ptr<CollisionMap>>& levelCollisionMaps, Scene &scene) {
        ApplyBlockFlags(levelCollisionMaps);
        //randomize();
        for (int32_t level = 0; level < 4; level++) {
            BuildLandscapeLighting(level);
            BuildTiles(scene, level);
            UpdateDrawLevels(scene, level);
        }

        scene.BuildModels(64, 768, -50, -10, -50);

        BuildBridges(scene);
        BuildOccluders();
    }

    void SceneBuilder::ReadTiles(Buffer &in, int32_t originX, int32_t originZ, int32_t level, int32_t x,
                                 int32_t z, int32_t mapRotation) {

        if (x >= 0 && x < 104 && z >= 0 && z < 104) {
            levelTileFlags[level][x][z] = 0;

            while (true) {
                int32_t type = in.ReadU8();

                if (type == 0) {
                    if (level == 0) {
                        levelHeightmap[0][x][z] = -Perlin(932731 + x + originX, 556238 + z + originZ) * 8;
                    } else {
                        levelHeightmap[level][x][z] = levelHeightmap[level - 1][x][z] - 240;
                    }
                    break;
                }

                if (type == 1) {
                    int32_t height = in.ReadU8();
                    if (height == 1) height = 0;

                    if (level == 0) {
                        levelHeightmap[0][x][z] = -height * 8;
                    } else {
                        levelHeightmap[level][x][z] = levelHeightmap[level - 1][x][z] - (height * 8);
                    }
                    break;
                }

                if (type <= 49) {
                    levelTileOverlayIDs[level][x][z] = in.Read8();
                    levelTileOverlayShape[level][x][z] = static_cast<int8_t>((type - 2) / 4);
                    levelTileOverlayRotation[level][x][z] =
                        static_cast<int8_t>(((type - 2) + mapRotation) & 0x3);
                } else if (type <= 81) {
                    levelTileFlags[level][x][z] = static_cast<int8_t>(type - 49);
                } else {
                    levelTileUnderlayIDs[level][x][z] = static_cast<int8_t>(type - 81);
                }
            }
        } else {
            // Out-of-bounds fallback (skip input data correctly)
            while (true) {
                int32_t type = in.ReadU8();
                if (type == 0) break;
                if (type == 1) {
                    in.ReadU8();
                    break;
                }
                if (type <= 49) {
                    in.ReadU8();
                }
            }
        }
    }

    void SceneBuilder::BuildTiles(Scene &scene, int32_t level) {
        for (int32_t z = 0; z < maxTileZ; z++) {
            blendChroma[z] = 0;
            blendSaturation[z] = 0;
            blendLightness[z] = 0;
            blendLuminance[z] = 0;
            blendMagnitude[z] = 0;
        }

         for (int32_t x0 = -5; x0 < (maxTileX + 5); x0++) {
             for (int32_t z0 = 0; z0 < maxTileZ; z0++) {
                 int32_t x1 = x0 + 5;

                 if ((x1 >= 0) && (x1 < maxTileX)) {
                     int32_t floID = levelTileUnderlayIDs[level][x1][z0] & 0xff;

                     if (floID > 0) {
                         const FloType& flo = FloType::instances[floID - 1];
                         blendChroma[z0] += flo.chroma;
                         blendSaturation[z0] += flo.saturation;
                         blendLightness[z0] += flo.lightness;
                         blendLuminance[z0] += flo.luminance;
                         blendMagnitude[z0]++;
                     }
                 }

                 int32_t x2 = x0 - 5;

                 if ((x2 >= 0) && (x2 < maxTileX)) {
                     int32_t floID = levelTileUnderlayIDs[level][x2][z0] & 0xff;

                     if (floID > 0) {
                        const FloType& flo = FloType::instances[floID - 1];
                         blendChroma[z0] -= flo.chroma;
                         blendSaturation[z0] -= flo.saturation;
                         blendLightness[z0] -= flo.lightness;
                         blendLuminance[z0] -= flo.luminance;
                         blendMagnitude[z0]--;
                     }
                 }
             }

             if ((x0 >= 1) && (x0 < (maxTileX - 1))) {
                 int32_t hueAccumulator = 0;
                 int32_t saturationAccumulator = 0;
                 int32_t lightnessAccumulator = 0;
                 int32_t luminanceAccumulator = 0;
                 int32_t magnitudeAccumulator = 0;

                 for (int32_t z0 = -5; z0 < (maxTileZ + 5); z0++) {
                     int32_t dz1 = z0 + 5;

                     if ((dz1 >= 0) && (dz1 < maxTileZ)) {
                         hueAccumulator += blendChroma[dz1];
                         saturationAccumulator += blendSaturation[dz1];
                         lightnessAccumulator += blendLightness[dz1];
                         luminanceAccumulator += blendLuminance[dz1];
                         magnitudeAccumulator += blendMagnitude[dz1];
                     }

                     int32_t dz2 = z0 - 5;

                     if ((dz2 >= 0) && (dz2 < maxTileZ)) {
                         hueAccumulator -= blendChroma[dz2];
                         saturationAccumulator -= blendSaturation[dz2];
                         lightnessAccumulator -= blendLightness[dz2];
                         luminanceAccumulator -= blendLuminance[dz2];
                         magnitudeAccumulator -= blendMagnitude[dz2];
                     }

                     if ((z0 < 1) || (z0 >= (maxTileZ - 1)) || (lowmem && ((levelTileFlags[0][x0][z0] & 0x2) == 0) && (((levelTileFlags[level][x0][z0] & 0x10) != 0) || (GetDrawLevel(level, x0, z0) != curLevel)))) {
                         continue;
                     }

                     if (level < minLevel) {
                         minLevel = level;
                     }

                     int32_t underlayID = levelTileUnderlayIDs[level][x0][z0] & 0xff;
                     int32_t overlayID = levelTileOverlayIDs[level][x0][z0] & 0xff;

                     if ((underlayID == 0) && (overlayID == 0)) {
                         continue;
                     }

                     int32_t heightSW = levelHeightmap[level][x0][z0];
                     int32_t heightSE = levelHeightmap[level][x0 + 1][z0];
                     int32_t heightNE = levelHeightmap[level][x0 + 1][z0 + 1];
                     int32_t heightNW = levelHeightmap[level][x0][z0 + 1];

                     int32_t lightSW = levelLightmap[x0][z0];
                     int32_t lightSE = levelLightmap[x0 + 1][z0];
                     int32_t lightNE = levelLightmap[x0 + 1][z0 + 1];
                     int32_t lightNW = levelLightmap[x0][z0 + 1];

                     int32_t baseColor = -1;
                     int32_t tintColor = -1;

                     if (underlayID > 0) {
                         int32_t hue = (hueAccumulator * 256) / luminanceAccumulator;
                         int32_t saturation = saturationAccumulator / magnitudeAccumulator;
                         int32_t lightness = lightnessAccumulator / magnitudeAccumulator;

                         baseColor = DecimateHSL(hue, saturation, lightness);

                         //hue = (hue + randomHueOffset) & 0xff;
                         //lightness += randomLightnessOffset;

                         if (lightness < 0) {
                             lightness = 0;
                         } else if (lightness > 255) {
                             lightness = 255;
                         }

                         tintColor = DecimateHSL(hue, saturation, lightness);
                     }

                     if (level > 0) {
                         bool occludes = (underlayID != 0) || (levelTileOverlayShape[level][x0][z0] == 0);

                         if ((overlayID > 0) && !FloType::instances[overlayID - 1].occludes) {
                             occludes = false;
                         }

                         // occludes && flat
                         if (occludes && (heightSW == heightSE) && (heightSW == heightNE) && (heightSW == heightNW)) {
                             levelOccludemap[level][x0][z0] |= 0b100100100100;
                         }
                     }

                     int32_t shadeColor = 0;

                     if (baseColor != -1) {
                         shadeColor = Draw3D::palette[MulHSL(tintColor, 96)];
                     }

                     if (overlayID == 0) {
                         scene.SetTile(level, x0, z0, 0, 0, -1, heightSW, heightSE, heightNE, heightNW, MulHSL(baseColor, lightSW), MulHSL(baseColor, lightSE), MulHSL(baseColor, lightNE), MulHSL(baseColor, lightNW), 0, 0, 0, 0, shadeColor, 0);
                     } else {
                         int32_t shape = levelTileOverlayShape[level][x0][z0] + 1;
                         int8_t rotation = levelTileOverlayRotation[level][x0][z0];
                         const FloType& flo = FloType::instances[overlayID - 1];
                         int32_t textureID = flo.textureID;
                         int32_t rgb;
                         int32_t hsl;

                         if (textureID >= 0) {
                             rgb = Draw3D::GetAverageTextureRGB(textureID);
                             hsl = -1;
                         } else if (flo.rgb == 16711935) {
                             rgb = 0;
                             hsl = -2;
                             textureID = -1;
                         } else {
                             hsl = DecimateHSL(flo.hue, flo.saturation, flo.lightness);
                             rgb = Draw3D::palette[AdjustLightness(flo.hsl, 96)];
                         }

                         scene.SetTile(level, x0, z0, shape, rotation, textureID, heightSW, heightSE,
                             heightNE, heightNW, MulHSL(baseColor, lightSW),
                             MulHSL(baseColor, lightSE),
                             MulHSL(baseColor, lightNE), MulHSL(baseColor, lightNW),
                             AdjustLightness(hsl, lightSW), AdjustLightness(hsl, lightSE),
                             AdjustLightness(hsl, lightNE), AdjustLightness(hsl, lightNW),
                             shadeColor, rgb);
                     }
                 }
             }
        }

    }

    void SceneBuilder::BuildLandscapeLighting(int32_t level) {
        const auto& shademap = levelShademap[level];
        int32_t lightAmbient = 96;
        int32_t lightAttenuation = 768;
        int32_t lightX = -50;
        int32_t lightY = -10;
        int32_t lightZ = -50;
        int32_t lightMagnitude = (lightAttenuation * static_cast<int32_t>(std::sqrt((lightX * lightX) + (lightY * lightY) + (lightZ * lightZ)))) >> 8;

        for (int32_t z = 1; z < (maxTileZ - 1); z++) {
            for (int32_t x = 1; x < (maxTileX - 1); x++) {
                const int32_t dx = levelHeightmap[level][x + 1][z] - levelHeightmap[level][x - 1][z];
                const int32_t dz = levelHeightmap[level][x][z + 1] - levelHeightmap[level][x][z - 1];
                const auto len = static_cast<int32_t>(std::sqrt((dx * dx) + 65536 + (dz * dz)));
                const int32_t normalX = (dx << 8) / len;
                const int32_t normalY = 65536 / len;
                const int32_t normalZ = (dz << 8) / len;
                const int32_t light = lightAmbient + (((lightX * normalX) + (lightY * normalY) + (lightZ * normalZ)) / lightMagnitude);
                const int32_t shade = (shademap[x - 1][z] >> 2) + (shademap[x + 1][z] >> 3) + (shademap[x][z - 1] >> 2) + (shademap[x][z + 1] >> 3) + (shademap[x][z] >> 1);
                levelLightmap[x][z] = light - shade;
            }
        }
    }

    void SceneBuilder::UpdateDrawLevels(Scene& scene, int32_t level) const
    {
        for (int32_t stz = 1; stz < (maxTileZ - 1); stz++) {
            for (int32_t stx = 1; stx < (maxTileX - 1); stx++) {
                scene.SetDrawLevel(level, stx, stz, GetDrawLevel(level, stx, stz));
            }
        }
    }

    void SceneBuilder::BuildOccluders()
    {
        int32_t wall0 = 0b001; // this flag is set by walls with rotation 0 or 2
        int32_t wall1 = 0b010; // this flag is set by walls with rotation 1 or 3
        int32_t floor = 0b100; // this flag is set by floors which are flat

        for (int32_t topLevel = 0; topLevel < 4; topLevel++) {
            if (topLevel > 0) {
                wall0 <<= 3;
                wall1 <<= 3;
                floor <<= 3;
            }

            for (int32_t level = 0; level <= topLevel; level++) {
                for (int32_t tileZ = 0; tileZ <= maxTileZ; tileZ++) {
                    for (int32_t tileX = 0; tileX <= maxTileX; tileX++) {
                        BuildWallOccludersX(wall0, topLevel, level, tileX, tileZ);
                        BuildWallOccludersZ(wall1, topLevel, level, tileX, tileZ);
                        BuildFloorOccluders(floor, topLevel, level, tileX, tileZ);
                    }
                }
            }
        }
    }

    void SceneBuilder::BuildBridges(Scene& scene) const
    {
        for (int32_t x = 0; x < maxTileX; x++) {
            for (int32_t z = 0; z < maxTileZ; z++) {
                if ((levelTileFlags[1][x][z] & 0x2) == 2) {
                    scene.SetBridge(x, z);
                }
            }
        }
    }

    void SceneBuilder::BuildFloorOccluders(int32_t floor, int32_t topLevel, int32_t level, int32_t tileX, int32_t tileZ)
    {
        if ((levelOccludemap[level][tileX][tileZ] & floor) != 0) {
            int32_t minTileX = tileX;
            int32_t maxTileX = tileX;
            int32_t minTileZ = tileZ;
            int32_t maxTileZ = tileZ;

            for (/**/; minTileZ > 0; minTileZ--) {
                if ((levelOccludemap[level][tileX][minTileZ - 1] & floor) == 0) {
                    break;
                }
            }
            for (/**/; maxTileZ < this->maxTileZ; maxTileZ++) {
                if ((levelOccludemap[level][tileX][maxTileZ + 1] & floor) == 0) {
                    break;
                }
            }

            // find_min_tile_x:
            for (/**/; minTileX > 0; minTileX--) {
                bool shouldBreak = false;
                for (int32_t z = minTileZ; z <= maxTileZ; z++) {
                    if ((levelOccludemap[level][minTileX - 1][z] & floor) == 0) {
                        shouldBreak = true;
                        break;
                    }
                }
                if (shouldBreak) {
                    break;
                }
            }

            // find_max_tile_x:
            for (/**/; maxTileX < this->maxTileX; maxTileX++) {
                bool shouldBreak = false;
                for (int32_t z = minTileZ; z <= maxTileZ; z++) {
                    if ((levelOccludemap[level][maxTileX + 1][z] & floor) == 0) {
                        shouldBreak = true;
                        break;
                    }
                }
                if (shouldBreak) {
                    break;
                }
            }

            if ((((maxTileX - minTileX) + 1) * ((maxTileZ - minTileZ) + 1)) >= 8) {
                int32_t y = levelHeightmap[level][minTileX][minTileZ];

                Scene::AddOccluder(topLevel, minTileX * 128, y, minTileZ * 128,
                                 (maxTileX * 128) + 128, y, (maxTileZ * 128) + 128,
                                 TYPE_GROUND);

                for (int32_t x = minTileX; x <= maxTileX; x++) {
                    for (int32_t z = minTileZ; z <= maxTileZ; z++) {
                        levelOccludemap[level][x][z] &= ~floor;
                    }
                }
            }
        }
    }


    static int32_t counter = 0;

    void SceneBuilder::AddLoc(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind, int32_t rotation, int32_t level, int32_t x,
        int32_t z)
    {
        if (lowmem && ((levelTileFlags[0][x][z] & 0x2) == 0) && (((levelTileFlags[level][x][z] & 0x10) != 0) || (GetDrawLevel(level, x, z) != curLevel))) {
            return;
        }
        if (level < minLevel) {
            minLevel = level;
        }

        int32_t heightmapSW = levelHeightmap[level][x][z];
        int32_t heightmapSE = levelHeightmap[level][x + 1][z];
        int32_t heightmapNE = levelHeightmap[level][x + 1][z + 1];
        int32_t heightmapNW = levelHeightmap[level][x][z + 1];
        int32_t heightmapAverage = (heightmapSW + heightmapSE + heightmapNE + heightmapNW) >> 2;

        const auto& type = LocType::Get(locID);
        int32_t bitset = x + (z << 7) + (locID << 14) + 0x40000000;

        if (!type->interactable) {
            bitset += 0x80000000;
        }

        const auto info = static_cast<int8_t>((rotation << 6) + kind);

        if (kind == 22) {
             AddGroundDecoration(scene, collision, locID, rotation, level, x, z, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage, *type, bitset, info);
         } else if ((kind == 10) || (kind == 11)) {
             AddLoc(scene, collision, locID, kind, rotation, level, x, z, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage, *type, bitset, info);
         } else if (kind >= 12) {
             AddRoof(scene, collision, locID, kind, rotation, level, x, z, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage, *type, bitset, info);
         } else if (kind == 0) {
             AddWall(scene, collision, locID, kind, rotation, level, x, z, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage, *type, bitset, info);
         } else if (kind == 1) {
             AddWallCornerDiagonal(scene, collision, locID, kind, rotation, level, x, z, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage, *type, bitset, info);
         } else if (kind == 2) {
             AddFullWall(scene, collision, locID, kind, rotation, level, x, z, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage, *type, bitset, info);
         } else if (kind == 3) {
             AddWallSquareCorner(scene, collision, locID, kind, rotation, level, x, z, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage, *type, bitset, info);
         } else if (kind == 9) {
             AddRoofOrDiagonalWall(scene, collision, locID, kind, rotation, level, x, z, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage, *type, bitset, info);
         } else {
             if (type->adjustToTerrain) {
                 if (rotation == 1) {
                     int32_t tmp = heightmapNW;
                     heightmapNW = heightmapNE;
                     heightmapNE = heightmapSE;
                     heightmapSE = heightmapSW;
                     heightmapSW = tmp;
                 } else if (rotation == 2) {
                     int32_t tmp = heightmapNW;
                     heightmapNW = heightmapSE;
                     heightmapSE = tmp;
                     tmp = heightmapNE;
                     heightmapNE = heightmapSW;
                     heightmapSW = tmp;
                 } else if (rotation == 3) {
                     int32_t tmp = heightmapNW;
                     heightmapNW = heightmapSW;
                     heightmapSW = heightmapSE;
                     heightmapSE = heightmapNE;
                     heightmapNE = tmp;
                 }
             }

             if (kind == 4) {
                 AddWallDecor(scene, Scene::ROTATION_WALL_TYPE[rotation], *type, locID, bitset, info, level, x, z, rotation * 512, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage);
             } else if (kind == 5) {
                 AddWallDecorOffset(scene, rotation, z, x, locID, level, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage, *type, bitset, info);
             } else if (kind == 6) {
                 AddWallDecor(scene, 0x100, *type, locID, bitset, info, level, x, z, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage);
             } else if (kind == 7) {
                 AddWallDecor(scene, 0x200, *type, locID, bitset, info, level, x, z, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage);
             } else if (kind == 8) {
                 AddWallDecor(scene, 0x300, *type, locID, bitset, info, level, x, z, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, heightmapAverage);
             }
         }

    }

    void SceneBuilder::BuildWallOccludersX(int32_t wall0, int32_t topLevel, int32_t level, int32_t tileX, int32_t tileZ)
    {
         if ((levelOccludemap[level][tileX][tileZ] & wall0) != 0) {
            int32_t minTileZ = tileZ;
            int32_t maxTileZ = tileZ;
            int32_t minLevel = level;
            int32_t maxLevel = level;

            for (/**/; minTileZ > 0; minTileZ--) {
                if ((levelOccludemap[level][tileX][minTileZ - 1] & wall0) == 0) {
                    break;
                }
            }

            for (/**/; maxTileZ < this->maxTileZ; maxTileZ++) {
                if ((levelOccludemap[level][tileX][maxTileZ + 1] & wall0) == 0) {
                    break;
                }
            }

            // find_min_level:
            for (/**/; minLevel > 0; minLevel--) {
                bool shouldBreak = false;
                for (int32_t z = minTileZ; z <= maxTileZ; z++) {
                    if ((levelOccludemap[minLevel - 1][tileX][z] & wall0) == 0) {
                        shouldBreak = true;
                        break;
                    }
                }
                if (shouldBreak) {
                    break;
                }
            }

            // find_max_level:
            for (/**/; maxLevel < topLevel; maxLevel++) {
                bool shouldBreak = false;
                for (int32_t z = minTileZ; z <= maxTileZ; z++) {
                    if ((levelOccludemap[maxLevel + 1][tileX][z] & wall0) == 0) {
                        shouldBreak = true;
                        break;
                    }
                }
                if (shouldBreak) {
                    break;
                }
            }

            int32_t area = ((maxLevel + 1) - minLevel) * ((maxTileZ - minTileZ) + 1);

            if (area >= 8) {
                int32_t minY = levelHeightmap[maxLevel][tileX][minTileZ] - 240;
                int32_t maxY = levelHeightmap[minLevel][tileX][minTileZ];

                Scene::AddOccluder(topLevel, tileX * 128, minY, minTileZ * 128,
                                 tileX * 128, maxY, (maxTileZ * 128) + 128,
                                 TYPE_WALL_X);

                for (int32_t l = minLevel; l <= maxLevel; l++) {
                    for (int32_t z = minTileZ; z <= maxTileZ; z++) {
                        levelOccludemap[l][tileX][z] &= ~wall0;
                    }
                }
            }
        }
    }

    void SceneBuilder::BuildWallOccludersZ(int32_t wall1, int32_t topLevel, int32_t level, int32_t tileX, int32_t tileZ)
    {
        if ((levelOccludemap[level][tileX][tileZ] & wall1) != 0) {
            int32_t minTileX = tileX;
            int32_t maxTileX = tileX;
            int32_t minLevel = level;
            int32_t maxLevel = level;

            for (/**/; minTileX > 0; minTileX--) {
                if ((levelOccludemap[level][minTileX - 1][tileZ] & wall1) == 0) {
                    break;
                }
            }
            for (/**/; maxTileX < this->maxTileX; maxTileX++) {
                if ((levelOccludemap[level][maxTileX + 1][tileZ] & wall1) == 0) {
                    break;
                }
            }

            // find_min_level:
            for (/**/; minLevel > 0; minLevel--) {
                bool shouldBreak = false;
                for (int32_t x = minTileX; x <= maxTileX; x++) {
                    if ((levelOccludemap[minLevel - 1][x][tileZ] & wall1) == 0) {
                        shouldBreak = true;
                        break;
                    }
                }
                if (shouldBreak) {
                    break;
                }
            }

            // find_max_level:
            for (/**/; maxLevel < topLevel; maxLevel++) {
                bool shouldBreak = false;
                for (int32_t x = minTileX; x <= maxTileX; x++) {
                    if ((levelOccludemap[maxLevel + 1][x][tileZ] & wall1) == 0) {
                        shouldBreak = true;
                        break;
                    }
                }
                if (shouldBreak) {
                    break;
                }
            }

            int32_t area = ((maxLevel + 1) - minLevel) * ((maxTileX - minTileX) + 1);

            if (area >= 8) {
                int32_t minY = levelHeightmap[maxLevel][minTileX][tileZ] - 240;
                int32_t maxY = levelHeightmap[minLevel][minTileX][tileZ];

                Scene::AddOccluder(topLevel, minTileX * 128, minY, tileZ * 128,
                                 (maxTileX * 128) + 128, maxY, tileZ * 128,
                                 TYPE_WALL_Z);

                for (int32_t l = minLevel; l <= maxLevel; l++) {
                    for (int32_t x = minTileX; x <= maxTileX; x++) {
                        levelOccludemap[l][x][tileZ] &= ~wall1;
                    }
                }
            }
        }
    }

    void SceneBuilder::AddLoc(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind, int32_t rotation, int32_t level, int32_t x,
                              int32_t z, int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW,
                              int32_t heightmapAverage, LocType& type, int32_t bitset, int8_t info)
    {
        std::shared_ptr<Entity> entity;

        if (type.seqID == -1 && type.overrideTypeIDs.empty()) {
            entity = type.GetModel(10, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, rotation, 10, heightmapSE, heightmapNE, heightmapSW, heightmapNW, type.seqID, true);
        }

        if (entity != nullptr) {
            int32_t yaw = 0;

            if (kind == 11) {
                yaw += 256;
            }

            int32_t width;
            int32_t height;

            if ((rotation == 1) || (rotation == 3)) {
                width = type.sizeZ;
                height = type.sizeX;
            } else {
                width = type.sizeX;
                height = type.sizeZ;
            }

            if (scene.Add(entity, level, x, z, heightmapAverage, width, height, yaw, bitset, info) && type.castShadow) {
                std::shared_ptr<Model> model;

                if (const auto& m = std::dynamic_pointer_cast<Model>(entity)) {
                    model = m;
                } else {
                    model = type.GetModel(10, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, -1);
                }

                if (model != nullptr) {
                    for (int32_t dx = 0; dx <= width; dx++) {
                        for (int32_t dz = 0; dz <= height; dz++) {
                            int32_t shade = model->radius / 4;

                            if (shade > 30) {
                                shade = 30;
                            }

                            if (shade > levelShademap[level][x + dx][z + dz]) {
                                levelShademap[level][x + dx][z + dz] = static_cast<int8_t>(shade);
                            }
                        }
                    }
                }
            }
        }
        if (type.solid && (collision != nullptr)) {
            collision->Add(type.blocksProjectiles, type.sizeX, type.sizeZ, x, z, rotation);
        }
    }

    void SceneBuilder::AddLoc(Scene& scene, int32_t rotation, int32_t z, int32_t kind,
        const std::shared_ptr<CollisionMap>& collision, int32_t x, int32_t locID, int32_t level, int32_t heightSW,
        int32_t heightSE, int32_t heightNE, int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info)
    {
        std::shared_ptr<Entity> entity;

        if ((loc.seqID == -1) && (loc.overrideTypeIDs.empty())) {
            entity = loc.GetModel(10, rotation, heightSW, heightSE, heightNE, heightNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, rotation, 10, heightSE, heightNE, heightSW, heightNW, loc.seqID, true);
        }

        if (entity != nullptr) {
            int32_t angle = 0;

            if (kind == 11) {
                angle += 256;
            }

            int32_t width;
            int32_t length;

            if (rotation == 1 || rotation == 3) {
                width = loc.sizeZ;
                length = loc.sizeX;
            } else {
                width = loc.sizeX;
                length = loc.sizeZ;
            }

            scene.Add(entity, level, x, z, y, width, length, angle, bitset, info);
        }
        if (loc.solid) {
            collision->Add(loc.blocksProjectiles, loc.sizeX, loc.sizeZ, x, z, rotation);
        }
    }

    void SceneBuilder::AddGroundDecoration(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t rotation, int32_t level, int32_t x,
                                           int32_t z, int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW,
                                           int32_t heightmapAverage, LocType& type, int32_t bitset, int8_t info)
    {
        if (!lowmem || type.interactable || type.important)
        {
            std::shared_ptr<Entity> entity;

            if ((type.seqID == -1) && (type.overrideTypeIDs.empty())) {
                entity = type.GetModel(22, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, -1);
            } else {
                entity = std::make_shared<LocEntity>(locID, rotation, 22, heightmapSE, heightmapNE, heightmapSW, heightmapNW, type.seqID, true);
            }

            scene.AddGroundDecoration(entity, level, x, z, heightmapAverage, bitset, info);

            if (type.solid && type.interactable && (collision != nullptr)) {
                collision->AddSolid(x, z);
            }
        }
    }

    void SceneBuilder::AddGroundDecoration(Scene& scene, int32_t rotation, int32_t z,
        const std::shared_ptr<CollisionMap>& collision, int32_t x, int32_t locID, int32_t level, int32_t heightSW,
        int32_t heightSE, int32_t heightNE, int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info)
    {
        std::shared_ptr<Entity> entity;

        if ((loc.seqID == -1) && (loc.overrideTypeIDs.empty())) {
            entity = loc.GetModel(22, rotation, heightSW, heightSE, heightNE, heightNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, rotation, 22, heightSE, heightNE, heightSW, heightNW, loc.seqID, true);
        }

        scene.AddGroundDecoration(entity, level, x, z, y, bitset, info);

        if (loc.solid && loc.interactable) {
            collision->AddSolid(x, z);
        }
    }

    void SceneBuilder::AddRoof(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind, int32_t rotation, int32_t level, int32_t x,
                               int32_t z, int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW,
                               int32_t heightmapAverage, LocType& type, int32_t bitset, int8_t info)
    {
        std::shared_ptr<Entity> entity;

        if ((type.seqID == -1) && (type.overrideTypeIDs.empty())) {
            entity = type.GetModel(kind, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, rotation, kind, heightmapSE, heightmapNE, heightmapSW, heightmapNW, type.seqID, true);
        }

        scene.Add(entity, level, x, z, heightmapAverage, 1, 1, 0, bitset, info);

        if ((kind >= 12) && (kind <= 17) && (kind != 13) && (level > 0)) {
            levelOccludemap[level][x][z] |= 0b100100100100;
        }

        if (type.solid && collision != nullptr) {
            collision->Add(type.blocksProjectiles, type.sizeX, type.sizeZ, x, z, rotation);
        }
    }

    void SceneBuilder::AddWall(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind, int32_t rotation, int32_t level, int32_t x,
        int32_t z, int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW,
        int32_t heightmapAverage, LocType& type, int32_t bitset, int8_t info)
    {
        std::shared_ptr<Entity> entity;

        if ((type.seqID == -1) && (type.overrideTypeIDs.empty())) {
            entity = type.GetModel(0, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, rotation, 0, heightmapSE, heightmapNE, heightmapSW, heightmapNW, type.seqID, true);
        }

        scene.SetWall(Scene::ROTATION_WALL_TYPE[rotation], entity, 0, nullptr, level, x, z, heightmapAverage, bitset, info);

        if (rotation == 0) {
            if (type.castShadow) {
                levelShademap[level][x][z] = static_cast<int8_t>(50);
                levelShademap[level][x][z + 1] = static_cast<int8_t>(50);
            }
            if (type.occludes) {
                levelOccludemap[level][x][z] |= 0b001001001001;
            }
        } else if (rotation == 1) {
            if (type.castShadow) {
                levelShademap[level][x][z + 1] = static_cast<int8_t>(50);
                levelShademap[level][x + 1][z + 1] = static_cast<int8_t>(50);
            }
            if (type.occludes) {
                levelOccludemap[level][x][z + 1] |= 0b010010010010;
            }
        } else if (rotation == 2) {
            if (type.castShadow) {
                levelShademap[level][x + 1][z] = static_cast<int8_t>(50);
                levelShademap[level][x + 1][z + 1] = static_cast<int8_t>(50);
            }
            if (type.occludes) {
                levelOccludemap[level][x + 1][z] |= 0b001001001001;
            }
        } else if (rotation == 3) {
            if (type.castShadow) {
                levelShademap[level][x][z] = static_cast<int8_t>(50);
                levelShademap[level][x + 1][z] = static_cast<int8_t>(50);
            }
            if (type.occludes) {
                levelOccludemap[level][x][z] |= 0b010010010010;
            }
        }

        if (type.solid && (collision != nullptr)) {
            collision->AddWall(x, z, kind, rotation, type.blocksProjectiles);
        }

        if (type.decorOffset != 16) {
            scene.SetWallDecorationOffset(level, x, z, type.decorOffset);
        }
    }

    void SceneBuilder::AddWallL(Scene& scene, int32_t rotation, int32_t z, int32_t kind,
        const std::shared_ptr<CollisionMap>& collision, int32_t x, int32_t locID, int32_t level, int32_t heightSW,
        int32_t heightSE, int32_t heightNE, int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info)
    {
        int32_t nextRotation = (rotation + 1) & 0x3;
        std::shared_ptr<Entity> locA;
        std::shared_ptr<Entity> locB;

        if ((loc.seqID == -1) && (loc.overrideTypeIDs.empty())) {
            locA = loc.GetModel(2, 4 + rotation, heightSW, heightSE, heightNE, heightNW, -1);
            locB = loc.GetModel(2, nextRotation, heightSW, heightSE, heightNE, heightNW, -1);
        } else {
            locA = std::make_shared<LocEntity>(locID, 4 + rotation, 2, heightSE, heightNE, heightSW, heightNW, loc.seqID, true);
            locB = std::make_shared<LocEntity>(locID, nextRotation, 2, heightSE, heightNE, heightSW, heightNW, loc.seqID, true);
        }

        scene.SetWall(Scene::ROTATION_WALL_TYPE[rotation], locA, Scene::ROTATION_WALL_TYPE[nextRotation], locB, level, x, z, y, bitset, info);

        if (loc.solid) {
            collision->AddWall(x, z, kind, rotation, loc.blocksProjectiles);
        }
    }

    void SceneBuilder::AddWallCornerDiagonal(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind, int32_t rotation, int32_t level,
                                             int32_t x, int32_t z, int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW,
                                             int32_t heightmapAverage, LocType& type, int32_t bitset, int8_t info)
    {
        std::shared_ptr<Entity> entity;

        if ((type.seqID == -1) && (type.overrideTypeIDs.empty())) {
            entity = type.GetModel(1, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, rotation, 1, heightmapSE, heightmapNE, heightmapSW, heightmapNW, type.seqID, true);
        }

        scene.SetWall(Scene::ROTATION_WALL_CORNER_TYPE[rotation], entity, 0, nullptr, level, x, z, heightmapAverage, bitset, info);

        if (type.castShadow) {
            if (rotation == 0) {
                levelShademap[level][x][z + 1] = static_cast<int8_t>(50);
            } else if (rotation == 1) {
                levelShademap[level][x + 1][z + 1] = static_cast<int8_t>(50);
            } else if (rotation == 2) {
                levelShademap[level][x + 1][z] = static_cast<int8_t>(50);
            } else if (rotation == 3) {
                levelShademap[level][x][z] = static_cast<int8_t>(50);
            }
        }

        if (type.solid && (collision != nullptr)) {
            collision->AddWall(x, z, kind, rotation, type.blocksProjectiles);
        }
    }

    void SceneBuilder::AddWallCornerDiagonal(Scene& scene, int32_t rotation, int32_t z, int32_t kind,
        const std::shared_ptr<CollisionMap>& collision, int32_t x, int32_t locID, int32_t level, int32_t heightSW,
        int32_t heightSE, int32_t heightNE, int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info)
    {
        std::shared_ptr<Entity> entity;

        if ((loc.seqID == -1) && (loc.overrideTypeIDs.empty())) {
            entity = loc.GetModel(1, rotation, heightSW, heightSE, heightNE, heightNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, rotation, 1, heightSE, heightNE, heightSW, heightNW, loc.seqID, true);
        }

        scene.SetWall(Scene::ROTATION_WALL_CORNER_TYPE[rotation], entity, 0, nullptr, level, x, z, y, bitset, info);

        if (loc.solid) {
            collision->AddWall(x, z, kind, rotation, loc.blocksProjectiles);
        }
    }

    void SceneBuilder::AddFullWall(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind, int32_t rotation, int32_t level,
                                   int32_t x, int32_t z, int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW,
                                   int32_t heightmapAverage, LocType& type, int32_t bitset, int8_t info)
    {
        int32_t nextRotation = (rotation + 1) & 0x3;
        std::shared_ptr<Entity> entityA;
        std::shared_ptr<Entity> entityB;

        if ((type.seqID == -1) && (type.overrideTypeIDs.empty())) {
            entityA = type.GetModel(2, 4 + rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, -1);
            entityB = type.GetModel(2, nextRotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, -1);
        } else {
            entityA = std::make_shared<LocEntity>(locID, 4 + rotation, 2, heightmapSE, heightmapNE, heightmapSW, heightmapNW, type.seqID, true);
            entityB = std::make_shared<LocEntity>(locID, nextRotation, 2, heightmapSE, heightmapNE, heightmapSW, heightmapNW, type.seqID, true);
        }

        scene.SetWall(Scene::ROTATION_WALL_TYPE[rotation], entityA, Scene::ROTATION_WALL_TYPE[nextRotation], entityB, level, x, z, heightmapAverage, bitset, info);

        if (type.occludes) {
            if (rotation == 0) {
                levelOccludemap[level][x][z] |= 0b001001001001;
                levelOccludemap[level][x][z + 1] |= 0b010010010010;
            } else if (rotation == 1) {
                levelOccludemap[level][x][z + 1] |= 0b010010010010;
                levelOccludemap[level][x + 1][z] |= 0b001001001001;
            } else if (rotation == 2) {
                levelOccludemap[level][x + 1][z] |= 0b001001001001;
                levelOccludemap[level][x][z] |= 0b010010010010;
            } else if (rotation == 3) {
                levelOccludemap[level][x][z] |= 0b010010010010;
                levelOccludemap[level][x][z] |= 0b001001001001;
            }
        }

        if (type.solid && collision != nullptr) {
            collision->AddWall(x, z, kind, rotation, type.blocksProjectiles);
        }

        if (type.decorOffset != 16) {
            scene.SetWallDecorationOffset(level, x, z, type.decorOffset);
        }
    }

    void SceneBuilder::ApplyBlockFlags(const std::vector<std::shared_ptr<CollisionMap>>& levelCollisionMaps) const
    {
        for (int32_t level = 0; level < 4; level++) {
            for (int32_t x = 0; x < 104; x++) {
                for (int32_t z = 0; z < 104; z++) {
                    // solid
                    if ((levelTileFlags[level][x][z] & 0x1) == 1) {
                        int trueLevel = level;

                        // bridge
                        if ((levelTileFlags[1][x][z] & 0x2) == 2) {
                            trueLevel--;
                        }

                        if (trueLevel >= 0) {
                            levelCollisionMaps[trueLevel]->AddSolid(x, z);
                        }
                    }
                }
            }
        }
    }

    void SceneBuilder::AddWallSquareCorner(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind, int32_t rotation, int32_t level,
                                           int32_t x, int32_t z, int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW,
                                           int32_t heightmapAverage, LocType& type, int32_t bitset, int8_t info)
    {
        std::shared_ptr<Entity> entity;

        if ((type.seqID == -1) && (type.overrideTypeIDs.empty())) {
            entity = type.GetModel(3, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, rotation, 3, heightmapSE, heightmapNE, heightmapSW, heightmapNW, type.seqID, true);
        }

        scene.SetWall(Scene::ROTATION_WALL_CORNER_TYPE[rotation], entity, 0, nullptr,
                      level, x, z, heightmapAverage, bitset, info);

        if (type.castShadow) {
            if (rotation == 0) {
                levelShademap[level][x][z + 1] = static_cast<int8_t>(50);
            } else if (rotation == 1) {
                levelShademap[level][x + 1][z + 1] = static_cast<int8_t>(50);
            } else if (rotation == 2) {
                levelShademap[level][x + 1][z] = static_cast<int8_t>(50);
            } else if (rotation == 3) {
                levelShademap[level][x][z] = static_cast<int8_t>(50);
            }
        }

        if (type.solid && collision != nullptr) {
            collision->AddWall(x, z, kind, rotation, type.blocksProjectiles);
        }
    }

    void SceneBuilder::AddWallSquareCorner(Scene& scene, int32_t rotation, int32_t z, int32_t kind,
        const std::shared_ptr<CollisionMap>& collision, int32_t x, int32_t locID, int32_t level, int32_t heightSW,
        int32_t heightSE, int32_t heightNE, int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info)
    {
        std::shared_ptr<Entity> entity;

        if ((loc.seqID == -1) && (loc.overrideTypeIDs.empty())) {
            entity = loc.GetModel(3, rotation, heightSW, heightSE, heightNE, heightNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, rotation, 3, heightSE, heightNE, heightSW, heightNW, loc.seqID, true);
        }

        scene.SetWall(Scene::ROTATION_WALL_CORNER_TYPE[rotation], entity, 0, nullptr, level, x, z, y, bitset, info);

        if (loc.solid) {
            collision->AddWall(x, z, kind, rotation, loc.blocksProjectiles);
        }
    }

    void SceneBuilder::AddRoofOrDiagonalWall(Scene& scene, const std::shared_ptr<CollisionMap>& collision, int32_t locID, int32_t kind, int32_t rotation, int32_t level,
                                             int32_t x, int32_t z, int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW,
                                             int32_t heightmapAverage, LocType& type, int32_t bitset, int8_t info)
    {
        std::shared_ptr<Entity> entity;

        if ((type.seqID == -1) && (type.overrideTypeIDs.empty())) {
            entity = type.GetModel(kind, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, rotation, kind,
                heightmapSE, heightmapNE, heightmapSW, heightmapNW, type.seqID, true);
        }

        scene.Add(entity, level, x, z, heightmapAverage, 1, 1, 0, bitset, info);

        if (type.solid && collision != nullptr) {
            collision->Add(type.blocksProjectiles, type.sizeX, type.sizeZ, x, z, rotation);
        }
    }

    void SceneBuilder::AddRoofOrDiagonalWall(Scene& scene, int32_t rotation, int32_t z, int32_t kind,
        const std::shared_ptr<CollisionMap>& collision, int32_t x, int32_t locID, int32_t level, int32_t heightSW,
        int32_t heightSE, int32_t heightNE, int32_t heightNW, int32_t y, LocType& loc, int32_t bitset, int8_t info)
    {
        std::shared_ptr<Entity> entity;

        if ((loc.seqID == -1) && (loc.overrideTypeIDs.empty())) {
            entity = loc.GetModel(kind, rotation, heightSW, heightSE, heightNE, heightNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, rotation, kind, heightSE, heightNE, heightSW, heightNW, loc.seqID, true);
        }

        scene.Add(entity, level, x, z, y, 1, 1, 0, bitset, info);

        if (loc.solid) {
            collision->Add(loc.blocksProjectiles, loc.sizeX, loc.sizeZ, x, z, rotation);
        }
    }

    void SceneBuilder::AddWallDecor(Scene& scene, int32_t decorType, LocType& type,
                                    int32_t locID, int32_t locBitset, int8_t locInfo, int32_t level, int32_t x, int32_t z, int32_t rotation,
                                    int32_t heightmapSW, int32_t heightmapSE, int32_t heightmapNE, int32_t heightmapNW, int32_t heightmapAverage)
    {
        std::shared_ptr<Entity> entity;

        if ((type.seqID == -1) && (type.overrideTypeIDs.empty())) {
            entity = type.GetModel(4, 0, heightmapSW, heightmapSE, heightmapNE, heightmapNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, 0, 4, heightmapSE, heightmapNE, heightmapSW, heightmapNW, type.seqID, true);
        }

        scene.SetWallDecoration(decorType, entity, level, x, z, heightmapAverage,
                                rotation, 0, 0, locBitset, locInfo);
    }

    void SceneBuilder::AddWallDecorOffset(Scene& scene, int32_t rotation, int32_t z, int32_t x, int32_t locID,
        int32_t level, int32_t heightSW, int32_t heightSE, int32_t heightNE, int32_t heightNW, int32_t y,
        LocType& loc, int32_t bitset, int8_t info)
    {
        int32_t offset = 16;
        int32_t wallBitset = scene.GetWallBitset(level, x, z);

        if (wallBitset > 0) {
            offset = LocType::Get((wallBitset >> 14) & 0x7fff)->decorOffset;
        }

        std::shared_ptr<Entity> entity;

        if ((loc.seqID == -1) && (loc.overrideTypeIDs.empty())) {
            entity = loc.GetModel(4, 0, heightSW, heightSE, heightNE, heightNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, 0, 4, heightSE, heightNE, heightSW, heightNW, loc.seqID, true);
        }

        scene.SetWallDecoration(Scene::ROTATION_WALL_TYPE[rotation], entity, level, x, z, y,
                                rotation * 512,
                                WALL_DECORATION_ROTATION_FORWARD_X[rotation] * offset,
                                WALL_DECORATION_ROTATION_FORWARD_Z[rotation] * offset,
                                bitset, info);
    }

    /**
     * Flattens the perimeter of the provided area.
     *
     * @param tileX     the area x.
     * @param tileZ     the area z.
     * @param tileSizeX the area size x.
     * @param tileSizeZ the area size z.
     */
    void SceneBuilder::StitchHeightmap(int32_t tileX, int32_t tileZ, int32_t tileSizeX, int32_t tileSizeZ)
    {
        for (int32_t z = tileZ; z <= (tileZ + tileSizeZ); z++) {
            for (int32_t x = tileX; x <= (tileX + tileSizeX); x++) {
                if ((x < 0) || (x >= maxTileX) || (z < 0) || (z >= maxTileZ)) {
                    continue;
                }

                levelShademap[0][x][z] = static_cast<int8_t>(127);

                if ((x == tileX) && (x > 0)) {
                    levelHeightmap[0][x][z] = levelHeightmap[0][x - 1][z];
                }

                if ((x == (tileX + tileSizeX)) && (x < (maxTileX - 1))) {
                    levelHeightmap[0][x][z] = levelHeightmap[0][x + 1][z];
                }

                if ((z == tileZ) && (z > 0)) {
                    levelHeightmap[0][x][z] = levelHeightmap[0][x][z - 1];
                }

                if ((z == (tileZ + tileSizeZ)) && (z < (maxTileZ - 1))) {
                    levelHeightmap[0][x][z] = levelHeightmap[0][x][z + 1];
                }
            }
        }
    }

    void SceneBuilder::ReadLocs(const std::vector<std::shared_ptr<CollisionMap>>& collisionMaps, Scene& scene, int32_t originX, int32_t originZ, const std::vector<int8_t>& data)
    {
        Buffer in(data);
        int32_t locID = -1;
        for (; ; ) {
            int32_t deltaID = in.ReadUSmart();

            if (deltaID == 0) {
                break;
            }

            locID += deltaID;
            int32_t locData = 0;

            for (; ; ) {
                int32_t deltaData = in.ReadUSmart();

                if (deltaData == 0) {
                    break;
                }

                locData += deltaData - 1;
                int32_t locZ = locData & 0x3f;
                int32_t locX = (locData >> 6) & 0x3f;
                int32_t locLevel = locData >> 12;
                int32_t locInfo = in.ReadU8();
                int32_t locKind = locInfo >> 2;
                int32_t locRotation = locInfo & 0x3;
                int32_t x = locX + originX;
                int32_t z = locZ + originZ;

                if ((x > 0) && (z > 0) && (x < 103) && (z < 103)) {
                    int32_t collisionLevel = locLevel;

                    if ((levelTileFlags[1][x][z] & 0x2) == 2) {
                        collisionLevel--;
                    }

                    std::shared_ptr<CollisionMap> collisionMap;

                    if (collisionLevel >= 0) {
                        collisionMap = collisionMaps[collisionLevel];
                    }

                    AddLoc(scene, collisionMap, locID, locKind, locRotation, locLevel, x, z);
                }
            }
        }
    }

    void SceneBuilder::ReadChunkLocs(const std::vector<std::shared_ptr<CollisionMap>>& collisionMaps, Scene& scene,
        int32_t mapLevel, int32_t mapRotation, int32_t mapChunkX, int32_t mapChunkZ, int32_t originX, int32_t originZ,
        const std::vector<int8_t>& data, int32_t level)
    {
        Buffer in(data);
        int32_t locID = -1;

        while (true) {
            int32_t deltaID = in.ReadUSmart();

            if (deltaID == 0) {
                break;
            }

            locID += deltaID;

            int32_t locData = 0;

            while (true) {
                int deltaData = in.ReadUSmart();

                if (deltaData == 0) {
                    break;
                }

                locData += deltaData - 1;

                int32_t locZ = locData & 0x3f;
                int32_t locX = (locData >> 6) & 0x3f;
                int32_t locLevel = locData >> 12;
                int32_t locInfo = in.ReadU8();
                int32_t locKind = locInfo >> 2;
                int32_t locRotation = locInfo & 0x3;

                if ((locLevel != mapLevel) || (locX < mapChunkX) || (locX >= (mapChunkX + 8)) || (locZ < mapChunkZ) || (locZ >= (mapChunkZ + 8))) {
                    continue;
                }

                const auto& loc = LocType::Get(locID);
                int32_t x = originX + MapUtil::RotateLocX(locX & 0x7, locZ & 0x7, loc->sizeX, loc->sizeZ, mapRotation);
                int32_t z = originZ + MapUtil::RotateLocZ(locX & 0x7, locZ & 0x7, loc->sizeX, loc->sizeZ, mapRotation);

                if ((x <= 0) || (z <= 0) || (x >= 103) || (z >= 103)) {
                    continue;
                }

                int32_t collisionLevel = locLevel;

                if ((levelTileFlags[1][x][z] & 0x2) == 2) {
                    collisionLevel--;
                }

                std::shared_ptr<CollisionMap> collisionMap;

                if (collisionLevel >= 0) {
                    collisionMap = collisionMaps[collisionLevel];
                }

                AddLoc(scene, collisionMap, locID, locKind, (locRotation + mapRotation) & 0x3, level, x, z);
            }
        }
    }

    void SceneBuilder::PrefetchLocs(std::shared_ptr<Buffer> buffer, OnDemand& onDemand)
    {
        int32_t locID = -1;

        for (; ; ) {
            int32_t deltaID = buffer->ReadUSmart();

            if (deltaID == 0) {
                break;
            }

            locID += deltaID;
            const auto& type = LocType::Get(locID);
            type->Prefetch(onDemand);

            while (buffer->ReadUSmart() != 0) {
                buffer->ReadU8();
            }
        }
    }

    /**
     * Used to determine if a LocType's models have been loaded.
     *
     * @param locID the loc type id.
     * @param kind  the loc kind.
     * @return <code>true</code> if the loc models are ready.
     */
    bool SceneBuilder::IsLocReady(int32_t locID, int32_t kind)
    {
        const auto& type = LocType::Get(locID);
        if (kind == 11) {
            kind = 10;
        }
        if ((kind >= 5) && (kind <= 8)) {
            kind = 4;
        }
        return type->Validate(kind);
    }

    void SceneBuilder::AddLoc(Scene& scene, int32_t rotation, int32_t z, int32_t type, int32_t tileLevel, const std::shared_ptr<CollisionMap>& collision,
        Array3DIndexed<int32_t>& levelHeightmap, int32_t x, int32_t locID, int32_t level)
    {
        int32_t heightSW = levelHeightmap[tileLevel][x][z];
        int32_t heightSE = levelHeightmap[tileLevel][x + 1][z];
        int32_t heightNE = levelHeightmap[tileLevel][x + 1][z + 1];
        int32_t heightNW = levelHeightmap[tileLevel][x][z + 1];
        int32_t y = (heightSW + heightSE + heightNE + heightNW) >> 2;

        const auto& loc = LocType::Get(locID);
        int32_t bitset = x + (z << 7) + (locID << 14) + 0x40000000;

        if (!loc->interactable) {
            bitset += 0x80000000;
        }

        auto info = static_cast<int8_t>((rotation << 6) + type);

        if (type == LocType::TYPE_GROUND_DECOR) {
            AddGroundDecoration(scene, rotation, z, collision, x, locID, level, heightSW, heightSE, heightNE, heightNW, y, *loc, bitset, info);
        } else if ((type == LocType::TYPE_CENTREPIECE) || (type == LocType::TYPE_CENTREPIECE_DIAGONAL)) {
            AddLoc(scene, rotation, z, type, collision, x, locID, level, heightSW, heightSE, heightNE, heightNW, y, *loc, bitset, info);
        } else if (type >= LocType::TYPE_ROOF_STRAIGHT) {
            AddRoofOrDiagonalWall(scene, rotation, z, type, collision, x, locID, level, heightSW, heightSE, heightNE, heightNW, y, *loc, bitset, info);
        } else if (type == LocType::TYPE_WALL_STRAIGHT) {
            AddWallStraight(scene, rotation, z, type, collision, x, locID, level, heightSW, heightSE, heightNE, heightNW, y, *loc, bitset, info);
        } else if (type == LocType::TYPE_WALL_CORNER_DIAGONAL) {
            AddWallCornerDiagonal(scene, rotation, z, type, collision, x, locID, level, heightSW, heightSE, heightNE, heightNW, y, *loc, bitset, info);
        } else if (type == LocType::TYPE_WALL_L) {
            AddWallL(scene, rotation, z, type, collision, x, locID, level, heightSW, heightSE, heightNE, heightNW, y, *loc, bitset, info);
        } else if (type == LocType::TYPE_WALL_SQUARE_CORNER) {
            AddWallSquareCorner(scene, rotation, z, type, collision, x, locID, level, heightSW, heightSE, heightNE, heightNW, y, *loc, bitset, info);
        } else if (type == LocType::TYPE_WALL_DIAGONAL) {
            AddRoofOrDiagonalWall(scene, rotation, z, type, collision, x, locID, level, heightSW, heightSE, heightNE, heightNW, y, *loc, bitset, info);
        } else {
            if (loc->adjustToTerrain) {
                if (rotation == 1) {
                    int32_t tmp = heightNW;
                    heightNW = heightNE;
                    heightNE = heightSE;
                    heightSE = heightSW;
                    heightSW = tmp;
                } else if (rotation == 2) {
                    int32_t tmp = heightNW;
                    heightNW = heightSE;
                    heightSE = tmp;
                    tmp = heightNE;
                    heightNE = heightSW;
                    heightSW = tmp;
                } else if (rotation == 3) {
                    int32_t tmp = heightNW;
                    heightNW = heightSW;
                    heightSW = heightSE;
                    heightSE = heightNE;
                    heightNE = tmp;
                }
            }

            if (type == LocType::TYPE_WALLDECOR_STRAIGHT) {
                AddWallDecor(scene, Scene::ROTATION_WALL_TYPE[rotation], *loc, locID, bitset, info, level, x, z, rotation * 512, heightSW, heightSE, heightNE, heightNW, y);
            } else if (type == LocType::TYPE_WALLDECOR_STRAIGHT_OFFSET) {
                AddWallDecorOffset(scene, rotation, z, x, locID, level, heightSW, heightSE, heightNE, heightNW, y, *loc, bitset, info);
            } else if (type == LocType::TYPE_WALLDECOR_DIAGONAL_NOOFFSET) {
                AddWallDecor(scene, 0x100, *loc, locID, bitset, info, level, x, z, rotation, heightSW, heightSE, heightNE, heightNW, y);
            } else if (type == LocType::TYPE_WALLDECOR_DIAGONAL_OFFSET) {
                AddWallDecor(scene, 0x200, *loc, locID, bitset, info, level, x, z, rotation, heightSW, heightSE, heightNE, heightNW, y);
            } else if (type == LocType::TYPE_WALLDECOR_DIAGONAL_BOTH) {
                AddWallDecor(scene, 0x300, *loc, locID, bitset, info, level, x, z, rotation, heightSW, heightSE, heightNE, heightNW, y);
            }
        }
    }

    /**
     * Reads the Locs from the provided data and determines if their models are available. The origin coordinate is used
     * to determine if a loc would be excluded from the scene, therefor not required to be validated. The chain of calls
     * eventually invokes {@link Model#validate(int)} which causes the {@link OnDemand} to do its job.
     *
     * @param data    the data
     * @param originX the region origin x in the scene.
     * @param originZ the region origin z in the scene.
     * @return <code>true</code> if all locs are valid.
     */
    bool SceneBuilder::ValidateLocs(const std::vector<int8_t>& data, int32_t originX, int32_t originZ)
    {
        bool ok = true;
        Buffer buffer(data);
        int32_t locID = -1;

        for (; ; ) {
            int32_t deltaID = buffer.ReadUSmart();
            if (deltaID == 0) {
                break;
            }

            locID += deltaID;

            int32_t pos = 0;
            bool skip = false;

            // this loop is for the same Loc ID.
            for (; ; ) {
                if (skip) {
                    if (buffer.ReadUSmart() == 0) {
                        break;
                    }
                    buffer.ReadU8();
                } else {
                    int32_t deltaPos = buffer.ReadUSmart();

                    if (deltaPos == 0) {
                        break;
                    }

                    pos += deltaPos - 1;

                    int32_t z = pos & 0x3f;
                    int32_t x = (pos >> 6) & 0x3f;

                    int32_t kind = buffer.ReadU8() >> 2;
                    int32_t localX = x + originX;
                    int32_t localZ = z + originZ;

                    if ((localX > 0) && (localZ > 0) && (localX < 103) && (localZ < 103)) {
                        const auto& type = LocType::Get(locID);

                        if ((kind != 22) || !lowmem || type->interactable || type->important) {
                            ok &= type->Validate();
                            skip = true; // Skip the remaining locs of this ID because we only need to validate the model of one.
                        }
                    }
                }
            }
        }
        return ok;
    }

    void SceneBuilder::AddWallStraight(Scene& scene, int32_t rotation, int32_t z, int32_t kind, const std::shared_ptr<CollisionMap>& collision, int32_t x,
                                       int32_t locID, int32_t level, int32_t heightSW, int32_t heightSE, int32_t heightNE, int32_t heightNW, int32_t y,
                                       LocType& loc, int32_t bitset, int8_t info)
    {
        std::shared_ptr<Entity> entity;

        if ((loc.seqID == -1) && (loc.overrideTypeIDs.empty())) {
            entity = loc.GetModel(0, rotation, heightSW, heightSE, heightNE, heightNW, -1);
        } else {
            entity = std::make_shared<LocEntity>(locID, rotation, 0, heightSE, heightNE, heightSW, heightNW, loc.seqID, true);
        }

        scene.SetWall(Scene::ROTATION_WALL_TYPE[rotation], entity, 0, nullptr, level, x, z, y, bitset, info);

        if (loc.solid) {
            collision->AddWall(x, z, kind, rotation, loc.blocksProjectiles);
        }
    }

    int32_t SceneBuilder::GetDrawLevel(int32_t level, int32_t stx, int32_t stz) const
    {
        if ((levelTileFlags[level][stx][stz] & 0x8) != 0) {
            return 0;
        }
        if ((level > 0) && ((levelTileFlags[1][stx][stz] & 0x2) != 0)) {
            return level - 1;
        }
        return level;
    }

    int32_t SceneBuilder::AdjustLightness(const int32_t hsl, int32_t scalar) {
        if (hsl == -2) {
            return 12345678;
        }

        if (hsl == -1) {
            if (scalar < 0) {
                scalar = 0;
            } else if (scalar > 127) {
                scalar = 127;
            }
            scalar = 127 - scalar;
            return scalar;
        }

        scalar = (scalar * (hsl & 0x7f)) / 128;

        if (scalar < 2) {
            scalar = 2;
        } else if (scalar > 126) {
            scalar = 126;
        }

        return (hsl & 0xff80) + scalar;
    }

    int32_t SceneBuilder::DecimateHSL(int32_t hue, int32_t saturation, int32_t lightness) {
        if (lightness > 179) {
            saturation /= 2;
        }
        if (lightness > 192) {
            saturation /= 2;
        }
        if (lightness > 217) {
            saturation /= 2;
        }
        if (lightness > 243) {
            saturation /= 2;
        }
        return ((hue / 4) << 10) + ((saturation / 32) << 7) + (lightness / 2);
    }

    int32_t SceneBuilder::MulHSL(int32_t hsl, int32_t lightness) {
        if (hsl == -1) {
            return 12345678;
        }

        lightness = (lightness * (hsl & 0x7f)) / 128;

        if (lightness < 2) {
            lightness = 2;
        } else if (lightness > 126) {
            lightness = 126;
        }

        return (hsl & 0xff80) + lightness;
    }

    int32_t SceneBuilder::Perlin(int32_t x, int32_t z) {
        int32_t value = (Perlin(x + 45365, z + 91923, 4) - 128) + ((Perlin(x + 10294, z + 37821, 2) - 128) >> 1) + ((Perlin(x, z, 1) - 128) >> 2);
        value = static_cast<int32_t>(static_cast<double>(value) * 0.3) + 35;

        if (value < 10) {
            value = 10;
        } else if (value > 60) {
            value = 60;
        }

        return value;
    }

    int32_t SceneBuilder::Perlin(int32_t x, int32_t z, int32_t scale) {
        const int32_t intX = x / scale;
        const int32_t intZ = z / scale;
        const int32_t fracX = x & (scale - 1);
        const int32_t fracZ = z & (scale - 1);
        const int32_t v1 = SmoothNoise(intX, intZ);
        const int32_t v2 = SmoothNoise(intX + 1, intZ);
        const int32_t v3 = SmoothNoise(intX, intZ + 1);
        const int32_t v4 = SmoothNoise(intX + 1, intZ + 1);
        const int32_t i1 = Interpolate(v1, v2, fracX, scale);
        const int32_t i2 = Interpolate(v3, v4, fracX, scale);
        return Interpolate(i1, i2, fracZ, scale);
    }

    int32_t SceneBuilder::SmoothNoise(int32_t x, int32_t y) {
        const int32_t corners = Noise(x - 1, y - 1) + Noise(x + 1, y - 1) + Noise(x - 1, y + 1) + Noise(x + 1, y + 1);
        const int32_t sides = Noise(x - 1, y) + Noise(x + 1, y) + Noise(x, y - 1) + Noise(x, y + 1);
        const int32_t center = Noise(x, y);
        return (corners / 16) + (sides / 8) + (center / 4);
    }

    int32_t SceneBuilder::Noise(int32_t x, int32_t y) {
        int32_t n = x + (y * 57);
        n = (n << 13) ^ n;
        return ((((n * ((n * n * 15731) + 789221)) + 1376312589) & 0x7fffffff) >> 19) & 0xff;
    }

    int32_t SceneBuilder::Interpolate(int32_t a, int32_t b, int32_t x, int32_t scale) {
        const int32_t f = (65536 - Draw3D::cos[(x * 1024) / scale]) >> 1;
        return ((a * (65536 - f)) >> 16) + ((b * f) >> 16);
    }
}
