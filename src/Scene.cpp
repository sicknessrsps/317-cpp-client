#include "Scene.h"

#include <utility>
#include "Draw3D.h"
#include "Draw2D.h"
#include "LocType.h"
#include "Model.h"
#include "ScopedTimer.h"

namespace SDL_Client {

    static Array4DIndexed<uint8_t> visibilityMatrix(8, 32, 51, 51);

    static constexpr std::array TEXTURE_HSL = {
        41, 39248, 41, 4643, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 43086, 41, 41, 41, 41,
        41, 41, 41, 8602, 41, 28992, 41, 41, 41, 41,
        41, 5056, 41, 41, 41, 7079, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 3131, 41, 41, 41
    };

    Scene::Scene(int32_t maxTileZ, int32_t maxTileX,
        Array3DIndexed<int32_t> &levelHeightmaps, int32_t maxLevel) :
        maxLevel(maxLevel), maxTileX(maxTileX), maxTileZ(maxTileZ), levelHeightmaps(levelHeightmaps)
    {
        levelTiles = Array3DIndexed<SceneTile>(maxLevel, maxTileZ, maxTileX);
        levelTileOcclusionCycles = Array3DIndexed<int32_t>(maxLevel, maxTileX + 1, maxTileZ + 1);
        Reset();
    }

    void Scene::Init(int32_t viewportWidth, int32_t viewportHeight)
    {
        viewportLeft = 0;
        viewportTop = 0;
        Scene::viewportRight = viewportWidth;
        Scene::viewportBottom = viewportHeight;
        viewportCenterX = viewportWidth / 2;
        viewportCenterY = viewportHeight / 2;
        InitVisibilityMatrix();
    }

    void Scene::Draw(int32_t eyeXC, int32_t eyeZC, int32_t eyeYaw, int32_t eyeYC, int32_t topLvl, int32_t eyePitch) {
        //ScopedTimer timer("Draw");

        cycle++;

        sinEyePitch = Draw3D::sin[eyePitch];
        cosEyePitch = Draw3D::cos[eyePitch];
        sinEyeYaw = Draw3D::sin[eyeYaw];
        cosEyeYaw = Draw3D::cos[eyeYaw];

        visibilityMap = visibilityMatrix.planeView((eyePitch - 128) / 32, eyeYaw / 64);

        if (eyeXC < 0) {
            eyeXC = 0;
        } else if (eyeXC >= (maxTileX * 128)) {
            eyeXC = (maxTileX * 128) - 1;
        }
        if (eyeZC < 0) {
            eyeZC = 0;
        } else if (eyeZC >= (maxTileZ * 128)) {
            eyeZC = (maxTileZ * 128) - 1;
        }

        Scene::eyeX = eyeXC;
        Scene::eyeY = eyeYC;
        Scene::eyeZ = eyeZC;

        eyeTileX = eyeXC / 128;
        eyeTileZ = eyeZC / 128;

        Scene::topLevel = topLvl;

        minDrawTileX = eyeTileX - 25;
        minDrawTileZ = eyeTileZ - 25;
        maxDrawTileX = eyeTileX + 25;
        maxDrawTileZ = eyeTileZ + 25;

        if (minDrawTileX < 0) {
            minDrawTileX = 0;
        }

        if (minDrawTileZ < 0) {
            minDrawTileZ = 0;
        }

        if (maxDrawTileX > maxTileX) {
            maxDrawTileX = maxTileX;
        }

        if (maxDrawTileZ > maxTileZ) {
            maxDrawTileZ = maxTileZ;
        }

        UpdateActiveOccluders();

        tilesRemaining = 0;
        for (int32_t level = minLevel; level < maxLevel; level++) {
            auto tiles = levelTiles[level];
            for (int32_t x = minDrawTileX; x < maxDrawTileX; x++) {
                for (int32_t z = minDrawTileZ; z < maxDrawTileZ; z++) {
                    auto& tile = tiles[x][z];

                    if (!tile.initialized) {
                        continue;
                    }

                    if ((tile.drawLevel > topLevel) || (!visibilityMap[(x - eyeTileX) + 25][(z - eyeTileZ) + 25] && ((levelHeightmaps[level][x][z] - eyeY) < 2000))) {
                        tile.visible = false;
                        tile.update = false;
                        tile.checkLocSpans = 0;
                    } else {
                        tile.visible = true;
                        tile.update = true;
                        tile.containsLocs = tile.locCount > 0;
                        tilesRemaining++;
                    }
                }
            }
        }

        for (int32_t level = minLevel; level < maxLevel; level++) {
            auto tiles = levelTiles[level];

            for (int32_t dx = -25; dx <= 0; dx++) {
                int32_t rightTileX = eyeTileX + dx;
                int32_t leftTileX = eyeTileX - dx;

                if ((rightTileX < minDrawTileX) && (leftTileX >= maxDrawTileX)) {
                    continue;
                }

                for (int32_t dz = -25; dz <= 0; dz++) {
                    int32_t forwardTileZ = eyeTileZ + dz;
                    int32_t backwardTileZ = eyeTileZ - dz;

                    if (rightTileX >= minDrawTileX) {
                        if (forwardTileZ >= minDrawTileZ) {
                            auto& tile = tiles[rightTileX][forwardTileZ];
                            if (tile.initialized && tile.visible) {
                                DrawTile(tile, true);
                            }
                        }

                        if (backwardTileZ < maxDrawTileZ) {
                            auto& tile = tiles[rightTileX][backwardTileZ];
                            if (tile.initialized && tile.visible) {
                                DrawTile(tile, true);
                            }
                        }
                    }

                    if (leftTileX < maxDrawTileX) {
                        if (forwardTileZ >= minDrawTileZ) {
                            auto& tile = tiles[leftTileX][forwardTileZ];
                            if (tile.initialized && tile.visible) {
                                DrawTile(tile, true);
                            }
                        }

                        if (backwardTileZ < maxDrawTileZ) {
                            auto& tile = tiles[leftTileX][backwardTileZ];
                            if (tile.initialized && tile.visible) {
                                DrawTile(tile, true);
                            }
                        }
                    }

                    if (tilesRemaining == 0) {
                        takingInput = false;
                        return;
                    }
                }
            }
        }

        for (int32_t level = minLevel; level < maxLevel; level++) {
            auto tiles = levelTiles[level];

            for (int32_t deltaX = -25; deltaX <= 0; deltaX++) {
                int32_t rightTileX = eyeTileX + deltaX;
                int32_t leftTileX = eyeTileX - deltaX;

                if ((rightTileX < minDrawTileX) && (leftTileX >= maxDrawTileX)) {
                    continue;
                }

                for (int32_t deltaZ = -25; deltaZ <= 0; deltaZ++) {
                    int32_t forwardTileZ = eyeTileZ + deltaZ;
                    int32_t backwardTileZ = eyeTileZ - deltaZ;

                    if (rightTileX >= minDrawTileX) {
                        if (forwardTileZ >= minDrawTileZ) {
                            auto& tile = tiles[rightTileX][forwardTileZ];
                            if (tile.initialized && tile.visible) {
                                DrawTile(tile, false);
                            }
                        }

                        if (backwardTileZ < maxDrawTileZ) {
                            auto& tile = tiles[rightTileX][backwardTileZ];
                            if (tile.initialized && tile.visible) {
                                DrawTile(tile, false);
                            }
                        }
                    }
                    if (leftTileX < maxDrawTileX) {
                        if (forwardTileZ >= minDrawTileZ) {
                            auto& tile = tiles[leftTileX][forwardTileZ];
                            if (tile.initialized && tile.visible) {
                                DrawTile(tile, false);
                            }
                        }

                        if (backwardTileZ < maxDrawTileZ) {
                            auto& tile = tiles[leftTileX][backwardTileZ];
                            if (tile.initialized && tile.visible) {
                                DrawTile(tile, false);
                            }
                        }
                    }

                    if (tilesRemaining == 0) {
                        takingInput = false;
                        return;
                    }
                }
            }
        }
        takingInput = false;
    }

    void Scene::SetTile(int32_t level, int32_t x, int32_t z, int32_t shape, int32_t rotation, int32_t textureID,
        int32_t southwestY, int32_t southeastY, int32_t northeastY, int32_t northwestY, int32_t southwestColor1,
        int32_t southeastColor1, int32_t northeastColor1, int32_t northwestColor1, int32_t southwestColor2,
        int32_t southeastColor2, int32_t northeastColor2, int32_t northwestColor2, int32_t backgroundRGB,
        int32_t foregroundRGB) {

        if (shape == 0) {
            SceneTileUnderlay underlay = { southwestColor1, southeastColor1, northeastColor1, northwestColor1, -1, backgroundRGB, false, true };
            for (int32_t l = level; l >= 0; l--) {
                if (!levelTiles[l][x][z].initialized) {
                    levelTiles[l][x][z] = SceneTile(l, x, z);
                }
            }
            levelTiles[level][x][z].underlay = underlay;
        } else if (shape == 1) {
            SceneTileUnderlay underlay = { southwestColor2, southeastColor2, northeastColor2, northwestColor2, textureID, foregroundRGB, (southwestY == southeastY) && (southwestY == northeastY) && (southwestY == northwestY), true };
            for (int32_t l = level; l >= 0; l--) {
                if (!levelTiles[l][x][z].initialized) {
                    levelTiles[l][x][z] = SceneTile(l, x, z);
                }
            }
            levelTiles[level][x][z].underlay = underlay;
        } else {
            SceneTileOverlay overlay = { z, southwestColor2, northwestColor1, northeastY, textureID, northeastColor2, rotation, southwestColor1, backgroundRGB, northeastColor1, northwestY, southeastY, southwestY, shape, northwestColor2, southeastColor2, southeastColor1, x, foregroundRGB };
            for (int32_t l = level; l >= 0; l--) {
                if (!levelTiles[l][x][z].initialized) {
                    levelTiles[l][x][z] = SceneTile(l, x, z);
                }
            }
            levelTiles[level][x][z].overlay = overlay;
        }
    }

    void Scene::SetMinLevel(int32_t level)
    {
        minLevel = level;
        for (int32_t stx = 0; stx < maxTileX; stx++) {
            for (int32_t stz = 0; stz < maxTileZ; stz++) {
                if (!levelTiles[level][stx][stz].initialized) {
                    levelTiles[level][stx][stz] = SceneTile(level, stx, stz);
                }
            }
        }
    }

    void Scene::DrawTile(SceneTile& next, bool checkAdjacent)
    {
        drawTileQueue.pushBack(&next);

        while (true)
        {
            SceneTile* tile = nullptr;

            do {
                tile = static_cast<SceneTile*>(drawTileQueue.pollFront());

                if (tile == nullptr) {
                    return;
                }

            } while (!tile->update);

            int32_t tileX = tile->x;
            int32_t tileZ = tile->z;
            int32_t level = tile->level;
            int32_t occludeLevel = tile->occludeLevel;
            auto tiles = levelTiles[level];

            if (tile->visible)
            {
                if (checkAdjacent) {
                    if (level > 0) {
                        const auto& other = levelTiles[level - 1][tileX][tileZ];
                        if (other.initialized && other.update) continue;
                    }

                    if ((tileX <= eyeTileX) && (tileX > minDrawTileX)) {
                        const auto& other = tiles[tileX - 1][tileZ];
                        if (other.initialized && other.update && (other.visible || ((tile->locSpans & 1) == 0)))
                            continue;
                    }

                    if ((tileX >= eyeTileX) && (tileX < (maxDrawTileX - 1))) {
                        const auto& other = tiles[tileX + 1][tileZ];
                        if (other.initialized && other.update && (other.visible || ((tile->locSpans & 4) == 0))) // access violation
                            continue;
                    }

                    if ((tileZ <= eyeTileZ) && (tileZ > minDrawTileZ)) {
                        const auto& other = tiles[tileX][tileZ - 1];
                        if (other.initialized && other.update && (other.visible || ((tile->locSpans & 8) == 0)))
                            continue;
                    }

                    if ((tileZ >= eyeTileZ) && (tileZ < (maxDrawTileZ - 1))) {
                        const auto& other = tiles[tileX][tileZ + 1];
                        if (other.initialized && other.update && (other.visible || ((tile->locSpans & 2) == 0)))
                            continue;
                    }
                } else {
                    checkAdjacent = true;
                }

                tile->visible = false;

                if (tile->bridge != nullptr) {
                    DrawBridgeTile(tileX, tileZ, *tile->bridge);
                }

                bool tileDrawn = DrawTileUnderlayOrOverlay(*tile, tileX, tileZ, occludeLevel);

                int32_t direction = 0;
                int32_t frontWallTypes = 0;

                auto& wall = tile->wall;
                auto& decor = tile->wallDecoration;

                if ((wall != nullptr) || (decor != nullptr)) {
                    if (eyeTileX == tileX) {
                        direction++;
                    } else if (eyeTileX < tileX) {
                        direction += 2;
                    }

                    if (eyeTileZ == tileZ) {
                        direction += 3;
                    } else if (eyeTileZ > tileZ) {
                        direction += 6;
                    }

                    frontWallTypes = FRONT_WALL_TYPES[direction];
                    tile->backWallTypes = BACK_WALL_TYPES[direction];
                }

                if (wall != nullptr) {
                    if ((wall->typeA & DIRECTION_ALLOW_WALL_CORNER_TYPE[direction]) != 0) {
                        switch (wall->typeA) {
                            case 16:
                                tile->checkLocSpans = 0b0011;
                                tile->blockLocSpans = WALL_CORNER_TYPE_16_BLOCK_LOC_SPANS[direction];
                                tile->inverseBlockLocSpans = 0b0011 - tile->blockLocSpans;
                                break;
                            case 32:
                                tile->checkLocSpans = 0b0110;
                                tile->blockLocSpans = WALL_CORNER_TYPE_32_BLOCK_LOC_SPANS[direction];
                                tile->inverseBlockLocSpans = 0b0110 - tile->blockLocSpans;
                                break;
                            case 64:
                                tile->checkLocSpans = 0b1100;
                                tile->blockLocSpans = WALL_CORNER_TYPE_64_BLOCK_LOC_SPANS[direction];
                                tile->inverseBlockLocSpans = 0b1100 - tile->blockLocSpans;
                                break;
                            case 128:
                                tile->checkLocSpans = 0b1001;
                                tile->blockLocSpans = WALL_CORNER_TYPE_128_BLOCK_LOC_SPANS[direction];
                                tile->inverseBlockLocSpans = 0b1001 - tile->blockLocSpans;
                                break;
                        }
                    } else {
                        tile->checkLocSpans = 0;
                    }

                    if (((wall->typeA & frontWallTypes) != 0) && WallVisible(occludeLevel, tileX, tileZ, wall->typeA)) {
                        wall->entityA->Draw(0, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, wall->x - eyeX, wall->y - eyeY, wall->z - eyeZ, wall->bitset);
                    }

                    if (((wall->typeB & frontWallTypes) != 0) && WallVisible(occludeLevel, tileX, tileZ, wall->typeB)) {
                        wall->entityB->Draw(0, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, wall->x - eyeX, wall->y - eyeY, wall->z - eyeZ, wall->bitset);
                    }
                }

                if ((decor != nullptr) && Visible(occludeLevel, tileX, tileZ, decor->entity->minY)) {
                    DrawWallDecor(frontWallTypes, *decor, true);
                }

                if (tileDrawn) {
                    const auto& groundDecor = tile->groundDecoration;

                    if (groundDecor != nullptr) {
                        groundDecor->entity->Draw(0, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw,
                            groundDecor->x - eyeX, groundDecor->y - eyeY, groundDecor->z - eyeZ, groundDecor->bitset);
                    }

                    const auto& stack = tile->objStack;

                    if ((stack != nullptr) && (stack->offset == 0)) {
                        DrawObjStack(stack, 0);
                    }
                }

                int32_t spans = tile->locSpans;

                if (spans != 0) {
                    if ((tileX < eyeTileX) && ((spans & 0x4) != 0)) {
                        auto& adjacent = tiles[tileX + 1][tileZ];
                        if ((adjacent.initialized) && adjacent.update) {
                            drawTileQueue.pushBack(&adjacent);
                        }
                    }

                    if ((tileZ < eyeTileZ) && ((spans & 0x2) != 0)) {
                        auto& adjacent = tiles[tileX][tileZ + 1];
                        if ((adjacent.initialized) && adjacent.update) {
                            drawTileQueue.pushBack(&adjacent);
                        }
                    }

                    if ((tileX > eyeTileX) && ((spans & 0x1) != 0)) {
                        auto& adjacent = tiles[tileX - 1][tileZ];
                        if ((adjacent.initialized) && adjacent.update) {
                            drawTileQueue.pushBack(&adjacent);
                        }
                    }

                    if ((tileZ > eyeTileZ) && ((spans & 0x8) != 0)) {
                        auto& adjacent = tiles[tileX][tileZ - 1];
                        if ((adjacent.initialized) && adjacent.update) {
                            drawTileQueue.pushBack(&adjacent);
                        }
                    }
                }
            }

            if (tile->checkLocSpans != 0) {
                bool draw = true;
                for (int32_t i = 0; i < tile->locCount; i++) {
                    if (!tile->locs[i]->Drawn() && ((tile->locSpan[i] & tile->checkLocSpans) == tile->blockLocSpans)) {
                        draw = false;
                        break;
                    }
                }

                if (draw) {
                    auto& wall = tile->wall;

                    if (WallVisible(occludeLevel, tileX, tileZ, wall->typeA)) {
                        wall->entityA->Draw(0, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, wall->x - eyeX,
                            wall->y - eyeY, wall->z - eyeZ, wall->bitset);
                    }

                    tile->checkLocSpans = 0;
                }
            }

            if (tile->containsLocs)
            {
                int32_t locCount = tile->locCount;
                tile->containsLocs = false;
                int32_t locBufferSize = 0;

                for (int32_t i = 0; i < locCount; i++) {
                    auto& loc = tile->locs[i];

                    if (loc->Drawn()) {
                        continue;
                    }

                    bool foundVisibleTile = false;

                    for (int32_t x = loc->minSceneTileX; x <= loc->maxSceneTileX; x++) {
                        for (int32_t z = loc->minSceneTileZ; z <= loc->maxSceneTileZ; z++) {
                            auto& other = tiles[x][z];

                            if (!other.initialized) {
                                continue;
                            }

                            if (!other.visible) {
                                if (other.checkLocSpans == 0) {
                                    continue;
                                }

                                int32_t spans = 0;

                                if (x > loc->minSceneTileX) {
                                    spans |= 0b0001;
                                }

                                if (x < loc->maxSceneTileX) {
                                    spans |= 0b0100;
                                }

                                if (z > loc->minSceneTileZ) {
                                    spans |= 0b1000;
                                }

                                if (z < loc->maxSceneTileZ) {
                                    spans |= 0b0010;
                                }

                                if ((spans & other.checkLocSpans) != tile->inverseBlockLocSpans) {
                                    continue;
                                }
                            }

                            tile->containsLocs = true;
                            foundVisibleTile = true;
                            break;
                        }

                        if (foundVisibleTile) {
                            break;
                        }
                    }

                    if (foundVisibleTile) {
                        continue;
                    }

                    locBuffer[locBufferSize++] = loc;

                    int32_t minTileDistanceX = eyeTileX - loc->minSceneTileX;
                    int32_t maxTileDistanceX = loc->maxSceneTileX - eyeTileX;

                    if (maxTileDistanceX > minTileDistanceX) {
                        minTileDistanceX = maxTileDistanceX;
                    }

                    int32_t minTileDistanceZ = eyeTileZ - loc->minSceneTileZ;
                    int32_t maxTileDistanceZ = loc->maxSceneTileZ - eyeTileZ;

                    if (maxTileDistanceZ > minTileDistanceZ) {
                        loc->distance = minTileDistanceX + maxTileDistanceZ;
                    } else {
                        loc->distance = minTileDistanceX + minTileDistanceZ;
                    }
                }

                while (true) {
                    int32_t farthestDistance = -50;
                    int32_t farthestIndex = -1;

                    for (int32_t index = 0; index < locBufferSize; index++) {
                        const auto& loc = locBuffer[index];

                        if (!loc->Drawn()) {
                            if (loc->distance > farthestDistance) {
                                farthestDistance = loc->distance;
                                farthestIndex = index;
                            } else if (loc->distance == farthestDistance) {
                                int32_t dx0 = loc->x - eyeX;
                                int32_t dz0 = loc->z - eyeZ;
                                int32_t dx1 = locBuffer[farthestIndex]->x - eyeX;
                                int32_t dz1 = locBuffer[farthestIndex]->z - eyeZ;

                                if (((dx0 * dx0) + (dz0 * dz0)) > ((dx1 * dx1) + (dz1 * dz1))) {
                                    farthestIndex = index;
                                }
                            }
                        }
                    }

                    if (farthestIndex == -1) {
                        break;
                    }

                    auto& farthest = locBuffer[farthestIndex];
                    farthest->cycle = cycle;

                    if (LocVisible(occludeLevel, farthest->minSceneTileX, farthest->maxSceneTileX,
                                  farthest->minSceneTileZ, farthest->maxSceneTileZ, farthest->entity->minY)) {
                        farthest->entity->Draw(farthest->yaw, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw,
                                              farthest->x - eyeX, farthest->y - eyeY, farthest->z - eyeZ, farthest->bitset);
                                  }

                    for (int32_t x = farthest->minSceneTileX; x <= farthest->maxSceneTileX; x++) {
                        for (int32_t z = farthest->minSceneTileZ; z <= farthest->maxSceneTileZ; z++) {
                            auto& occupied = tiles[x][z];

                            if (occupied.checkLocSpans != 0 || (((x != tileX) || (z != tileZ)) && occupied.update)) {
                                drawTileQueue.pushBack(&occupied);
                            }
                        }
                    }
                }

                if (tile->containsLocs) {
                    continue;
                }
            }

            if (!tile->update || (tile->checkLocSpans != 0)) {
                continue;
            }

            if ((tileX <= eyeTileX) && (tileX > minDrawTileX)) {
                const auto& adjacent = tiles[tileX - 1][tileZ];
                if ((adjacent.initialized) && adjacent.update) {
                    continue;
                }
            }

            if ((tileX >= eyeTileX) && (tileX < (maxDrawTileX - 1))) {
                const auto& adjacent = tiles[tileX + 1][tileZ];
                if ((adjacent.initialized) && adjacent.update) {
                    continue;
                }
            }

            if ((tileZ <= eyeTileZ) && (tileZ > minDrawTileZ)) {
                const auto& adjacent = tiles[tileX][tileZ - 1];
                if ((adjacent.initialized) && adjacent.update) {
                    continue;
                }
            }

            if ((tileZ >= eyeTileZ) && (tileZ < (maxDrawTileZ - 1))) {
                const auto& adjacent = tiles[tileX][tileZ + 1];
                if ((adjacent.initialized) && adjacent.update) {
                    continue;
                }
            }

            tile->update = false;
            tilesRemaining--;

            const auto& stack = tile->objStack;
            if ((stack != nullptr) && (stack->offset != 0)) {
                DrawObjStack(stack, stack->offset);
            }

            if (tile->backWallTypes != 0) {
                auto& decor = tile->wallDecoration;

                if ((decor != nullptr) && Visible(occludeLevel, tileX, tileZ, decor->entity->minY)) {
                    DrawWallDecor(tile->backWallTypes, *decor, false);
                }

                auto& wall = tile->wall;

                if (wall != nullptr) {
                    if (((wall->typeB & tile->backWallTypes) != 0) && WallVisible(occludeLevel, tileX, tileZ, wall->typeB)) {
                        wall->entityB->Draw(0, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, wall->x - eyeX, wall->y - eyeY, wall->z - eyeZ, wall->bitset);
                    }

                    if (((wall->typeA & tile->backWallTypes) != 0) && WallVisible(occludeLevel, tileX, tileZ, wall->typeA)) {
                        wall->entityA->Draw(0, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, wall->x - eyeX, wall->y - eyeY, wall->z - eyeZ, wall->bitset);
                    }
                }
            }

            if (level < (maxLevel - 1)) {
                auto& above = levelTiles[level + 1][tileX][tileZ];
                if ((above.initialized) && above.update) {
                    drawTileQueue.pushBack(&above);
                }
            }

            if (tileX < eyeTileX) {
                auto& adjacent = tiles[tileX + 1][tileZ];
                if ((adjacent.initialized) && adjacent.update) {
                    drawTileQueue.pushBack(&adjacent);
                }
            }

            if (tileZ < eyeTileZ) {
                auto& adjacent = tiles[tileX][tileZ + 1];
                if ((adjacent.initialized) && adjacent.update) {
                    drawTileQueue.pushBack(&adjacent);
                }
            }

            if (tileX > eyeTileX) {
                auto& adjacent = tiles[tileX - 1][tileZ];
                if ((adjacent.initialized) && adjacent.update) {
                    drawTileQueue.pushBack(&adjacent);
                }
            }

            if (tileZ > eyeTileZ) {
                auto& adjacent = tiles[tileX][tileZ - 1];
                if ((adjacent.initialized) && adjacent.update) {
                    drawTileQueue.pushBack(&adjacent);
                }
            }
        }
    }

    void Scene::DrawTileUnderlay(const SceneTileUnderlay& underlay, int32_t level, int32_t sinPitch, int32_t cosPitch,
        int32_t sinYaw, int32_t cosYaw, int32_t tileX, int32_t tileZ)
    {
        int32_t x3;
        int32_t x0 = x3 = (tileX << 7) - eyeX;
        int32_t z1;
        int32_t z0 = z1 = (tileZ << 7) - eyeZ;
        int32_t x2;
        int32_t x1 = x2 = x0 + 128;
        int32_t z3;
        int32_t z2 = z3 = z0 + 128;

        int32_t y0 = levelHeightmaps[level][tileX][tileZ] - eyeY;
        int32_t y1 = levelHeightmaps[level][tileX + 1][tileZ] - eyeY;
        int32_t y2 = levelHeightmaps[level][tileX + 1][tileZ + 1] - eyeY;
        int32_t y3 = levelHeightmaps[level][tileX][tileZ + 1] - eyeY;

        int32_t tmp = ((z0 * sinYaw) + (x0 * cosYaw)) >> 16;
        z0 = ((z0 * cosYaw) - (x0 * sinYaw)) >> 16;
        x0 = tmp;

        tmp = ((y0 * cosPitch) - (z0 * sinPitch)) >> 16;
        z0 = ((y0 * sinPitch) + (z0 * cosPitch)) >> 16;
        y0 = tmp;

        if (z0 < 50) {
            return;
        }

        tmp = ((z1 * sinYaw) + (x1 * cosYaw)) >> 16;
        z1 = ((z1 * cosYaw) - (x1 * sinYaw)) >> 16;
        x1 = tmp;

        tmp = ((y1 * cosPitch) - (z1 * sinPitch)) >> 16;
        z1 = ((y1 * sinPitch) + (z1 * cosPitch)) >> 16;
        y1 = tmp;

        if (z1 < 50) {
            return;
        }

        tmp = ((z2 * sinYaw) + (x2 * cosYaw)) >> 16;
        z2 = ((z2 * cosYaw) - (x2 * sinYaw)) >> 16;
        x2 = tmp;

        tmp = ((y2 * cosPitch) - (z2 * sinPitch)) >> 16;
        z2 = ((y2 * sinPitch) + (z2 * cosPitch)) >> 16;
        y2 = tmp;

        if (z2 < 50) {
            return;
        }

        tmp = ((z3 * sinYaw) + (x3 * cosYaw)) >> 16;
        z3 = ((z3 * cosYaw) - (x3 * sinYaw)) >> 16;
        x3 = tmp;

        tmp = ((y3 * cosPitch) - (z3 * sinPitch)) >> 16;
        z3 = ((y3 * sinPitch) + (z3 * cosPitch)) >> 16;
        y3 = tmp;

        if (z3 < 50) {
            return;
        }

        int32_t px0 = Draw3D::centerX + ((x0 << 9) / z0);
        int32_t py0 = Draw3D::centerY + ((y0 << 9) / z0);
        int32_t px1 = Draw3D::centerX + ((x1 << 9) / z1);
        int32_t py1 = Draw3D::centerY + ((y1 << 9) / z1);
        int32_t px2 = Draw3D::centerX + ((x2 << 9) / z2);
        int32_t py2 = Draw3D::centerY + ((y2 << 9) / z2);
        int32_t px3 = Draw3D::centerX + ((x3 << 9) / z3);
        int32_t py3 = Draw3D::centerY + ((y3 << 9) / z3);

        Draw3D::alpha = 0;

        if ((((px2 - px3) * (py1 - py3)) - ((py2 - py3) * (px1 - px3))) > 0) {
            Draw3D::clipX = (px2 < 0) || (px3 < 0) || (px1 < 0) || (px2 > Draw2D::boundX) || (px3 > Draw2D::boundX) || (px1 > Draw2D::boundX);

            if (takingInput && PointInsideTriangle(mouseX, mouseY, py2, py3, py1, px2, px3, px1)) {
                clickTileX = tileX;
                clickTileZ = tileZ;
            }

            if (underlay.textureID == -1) {
                if (underlay.northeastColor != 12345678) {
                    Draw3D::FillGouraudTriangle(py2, py3, py1, px2, px3, px1, underlay.northeastColor, underlay.northwestColor, underlay.southeastColor);
                }
            } else if (!lowmem) {
                if (underlay.flat) {
                    Draw3D::FillTexturedTriangle(py2, py3, py1, px2, px3, px1, underlay.northeastColor, underlay.northwestColor, underlay.southeastColor, x0, x1, x3, y0, y1, y3, z0, z1, z3, underlay.textureID);
                } else {
                    Draw3D::FillTexturedTriangle(py2, py3, py1, px2, px3, px1, underlay.northeastColor, underlay.northwestColor, underlay.southeastColor, x2, x3, x1, y2, y3, y1, z2, z3, z1, underlay.textureID);
                }
            } else {
                int32_t color = TEXTURE_HSL[underlay.textureID];
                Draw3D::FillGouraudTriangle(py2, py3, py1, px2, px3, px1, MulLightness(color, underlay.northeastColor), MulLightness(color, underlay.northwestColor), MulLightness(color, underlay.southeastColor));
            }
        }

        if ((((px0 - px1) * (py3 - py1)) - ((py0 - py1) * (px3 - px1))) > 0) {
            Draw3D::clipX = (px0 < 0) || (px1 < 0) || (px3 < 0) || (px0 > Draw2D::boundX) || (px1 > Draw2D::boundX) || (px3 > Draw2D::boundX);

            if (takingInput && PointInsideTriangle(mouseX, mouseY, py0, py1, py3, px0, px1, px3)) {
                clickTileX = tileX;
                clickTileZ = tileZ;
            }

            if (underlay.textureID == -1) {
                if (underlay.southwestColor != 12345678) {
                    Draw3D::FillGouraudTriangle(py0, py1, py3, px0, px1, px3, underlay.southwestColor, underlay.southeastColor, underlay.northwestColor);
                }
            } else {
                if (!lowmem) {
                    Draw3D::FillTexturedTriangle(py0, py1, py3, px0, px1, px3, underlay.southwestColor, underlay.southeastColor, underlay.northwestColor, x0, x1, x3, y0, y1, y3, z0, z1, z3, underlay.textureID);
                    return;
                }
                int32_t color = TEXTURE_HSL[underlay.textureID];
                Draw3D::FillGouraudTriangle(py0, py1, py3, px0, px1, px3, MulLightness(color, underlay.southwestColor), MulLightness(color, underlay.southeastColor), MulLightness(color, underlay.northwestColor));
            }
        }
    }

    void Scene::SetDrawLevel(int32_t level, int32_t stx, int32_t stz, int32_t drawLevel)
    {
        const auto& tile = levelTiles[level][stx][stz];
        if (tile.initialized) {
            levelTiles[level][stx][stz].drawLevel = drawLevel;
        }
    }

    void Scene::SetWallDecoration(int32_t type, const std::shared_ptr<Entity>& entity, int32_t level, int32_t tileX,
        int32_t tileZ, int32_t y, int32_t rotation, int32_t offsetX, int32_t offsetZ, int32_t bitset, int8_t info)
    {
        if (entity == nullptr) {
            return;
        }
        SceneWallDecoration decor;
        decor.bitset = bitset;
        decor.info = info;
        decor.x = (tileX * 128) + 64 + offsetX;
        decor.z = (tileZ * 128) + 64 + offsetZ;
        decor.y = y;
        decor.entity = entity;
        decor.type = type;
        decor.rotation = rotation;
        for (int32_t p = level; p >= 0; p--) {
            if (!levelTiles[p][tileX][tileZ].initialized) {
                levelTiles[p][tileX][tileZ] = SceneTile(p, tileX, tileZ);
            }
        }
        levelTiles[level][tileX][tileZ].wallDecoration = std::make_shared<SceneWallDecoration>(decor);
    }

    void Scene::SetWallDecorationOffset(int32_t level, int32_t stx, int32_t stz, int32_t offset)
    {
        auto& tile = levelTiles[level][stx][stz];
        if (!tile.initialized) {
            return;
        }
        auto& decor = tile.wallDecoration;
        if (decor != nullptr) {
            int32_t sx = (stx * 128) + 64;
            int32_t sz = (stz * 128) + 64;
            decor->x = sx + (((decor->x - sx) * offset) / 16);
            decor->z = sz + (((decor->z - sz) * offset) / 16);
        }
    }

    void Scene::Click(int32_t mouseYC, int32_t mouseXC)
    {
        takingInput = true;
        Scene::mouseX = mouseXC;
        Scene::mouseY = mouseYC;
        clickTileX = -1;
        clickTileZ = -1;
    }

    void Scene::AddOccluder(int32_t level, int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY,
        int32_t maxZ, int32_t type)
    {
        SceneOccluder occluder{};
        occluder.minTileX = minX / 128;
        occluder.maxTileX = maxX / 128;
        occluder.minTileZ = minZ / 128;
        occluder.maxTileZ = maxZ / 128;
        occluder.type = type;
        occluder.minX = minX;
        occluder.maxX = maxX;
        occluder.minZ = minZ;
        occluder.maxZ = maxZ;
        occluder.minY = minY;
        occluder.maxY = maxY;
        levelOccluders[level][levelOccluderCount[level]++] = occluder;
    }

    void Scene::AddGroundDecoration(const std::shared_ptr<Entity>& entity, int32_t tileLevel, int32_t tileX, int32_t tileZ,
        int32_t y, int32_t bitset, int8_t info)
    {
        if (entity == nullptr) {
            return;
        }
        SceneGroundDecoration decor;
        decor.entity = entity;
        decor.x = (tileX * 128) + 64;
        decor.z = (tileZ * 128) + 64;
        decor.y = y;
        decor.bitset = bitset;
        decor.info = info;
        if (!levelTiles[tileLevel][tileX][tileZ].initialized) {
            levelTiles[tileLevel][tileX][tileZ] = SceneTile(tileLevel, tileX, tileZ);
        }
        levelTiles[tileLevel][tileX][tileZ].groundDecoration = std::make_shared<SceneGroundDecoration>(decor);
    }

    void Scene::SetWall(int32_t typeA, const std::shared_ptr<Entity>& entityA, int32_t typeB,
        const std::shared_ptr<Entity>& entityB, int32_t level, int32_t tileX, int32_t tileZ, int32_t y, int32_t bitset,
        int8_t info)
    {
        if ((entityA == nullptr) && (entityB == nullptr)) {
            return;
        }
        SceneWall wall;
        wall.bitset = bitset;
        wall.info = info;
        wall.x = (tileX * 128) + 64;
        wall.z = (tileZ * 128) + 64;
        wall.y = y;
        wall.entityA = entityA;
        wall.entityB = entityB;
        wall.typeA = typeA;
        wall.typeB = typeB;
        for (int l = level; l >= 0; l--) {
            if (!levelTiles[l][tileX][tileZ].initialized) {
                levelTiles[l][tileX][tileZ] = SceneTile(l, tileX, tileZ);
            }
        }
        levelTiles[level][tileX][tileZ].wall = std::make_shared<SceneWall>(wall);
    }

    int32_t Scene::GetWallBitset(int32_t level, int32_t x, int32_t z)
    {
        const auto& tile = levelTiles[level][x][z];
        if ((tile.initialized) && (tile.wall != nullptr)) {
            return tile.wall->bitset;
        } else {
            return 0;
        }
    }

    bool Scene::TestPoint(int32_t y, int32_t z, int32_t x)
    {
        int32_t px = ((z * sinEyeYaw) + (x * cosEyeYaw)) >> 16;
        int32_t tmp = ((z * cosEyeYaw) - (x * sinEyeYaw)) >> 16;
        int32_t pz = ((y * sinEyePitch) + (tmp * cosEyePitch)) >> 16;
        int32_t py = ((y * cosEyePitch) - (tmp * sinEyePitch)) >> 16;
        if ((pz < 50) || (pz > 3500)) {
            return false;
        }
        int32_t viewportX = viewportCenterX + ((px << 9) / pz);
        int32_t viewportY = viewportCenterY + ((py << 9) / pz);
        return (viewportX >= viewportLeft) && (viewportX <= viewportRight) && (viewportY >= viewportTop) && (viewportY <= viewportBottom);
    }

    /**
     * Populates the {@link Scene#visibilityMatrix} lookup table which provides a rough approximation for relative tile
     * visibility within a specific range of pitches and yaws.
     */
    void Scene::InitVisibilityMatrix()
    {
        std::vector<int32_t> pitchDistance(9);
        for (int32_t pitchLevel = 0; pitchLevel < 9; pitchLevel++) {
            int32_t angle = 128 + (pitchLevel * 32) + 15;
            int32_t distance = 600 + (angle * 3);
            pitchDistance[pitchLevel] = (distance * Draw3D::sin[angle]) >> 16;
        }

        Array4DIndexed<int8_t> matrix(9, 32, 53, 53);

        for (int32_t pitch = 128; pitch <= 384; pitch += 32) {
            for (int32_t yaw = 0; yaw < 2048; yaw += 64) {
                sinEyePitch = Draw3D::sin[pitch];
                cosEyePitch = Draw3D::cos[pitch];
                sinEyeYaw = Draw3D::sin[yaw];
                cosEyeYaw = Draw3D::cos[yaw];

                int32_t pitchLevel = (pitch - 128) / 32;
                int32_t yawLevel = yaw / 64;

                for (int32_t dx = -26; dx <= 26; dx++) {
                    for (int32_t dz = -26; dz <= 26; dz++) {
                        int32_t x = dx * 128;
                        int32_t z = dz * 128;
                        bool visible = false;
                        for (int32_t y = -500; y <= 800; y += 128) {
                            if (TestPoint(pitchDistance[pitchLevel] + y, z, x)) {
                                visible = true;
                                break;
                            }
                        }
                        matrix[pitchLevel][yawLevel][dx + 25 + 1][dz + 25 + 1] = static_cast<int8_t>(visible);
                    }
                }
            }
        }

        // One final pass to extend the visibility map up to 1 tile in any direction.
        for (int32_t pitchLevel = 0; pitchLevel < 8; ++pitchLevel) {
            for (int32_t yawLevel = 0; yawLevel < 32; ++yawLevel) {
                for (int32_t x = -25; x < 25; ++x) {
                    for (int32_t z = -25; z < 25; ++z) {
                        bool visible = false;

                        for (int32_t dx = -1; dx <= 1; ++dx) {
                            for (int32_t dz = -1; dz <= 1; ++dz) {
                                if (matrix[pitchLevel][yawLevel][x + dx + 26][z + dz + 26] ||
                                    matrix[pitchLevel][(yawLevel + 1) % 31][x + dx + 26][z + dz + 26] ||
                                    matrix[pitchLevel + 1][yawLevel][x + dx + 26][z + dz + 26] ||
                                    matrix[pitchLevel + 1][(yawLevel + 1) % 31][x + dx + 26][z + dz + 26]) {
                                    visible = true;
                                    goto check_area_end; // break out of both inner loops
                                }
                            }
                        }
                        check_area_end:;
                        visibilityMatrix[pitchLevel][yawLevel][x + 25][z + 25] = visible;
                    }
                }
            }
        }
    }

    void Scene::DrawTileOverlay(int32_t tileX, int32_t sinPitch, int32_t sinYaw, const SceneTileOverlay& overlay, int32_t cosPitch, int32_t tileZ, int32_t cosYaw)
    {
        int32_t vertexCount = overlay.vertexX.size();
        for (int32_t v = 0; v < vertexCount; v++) {
            int32_t x = overlay.vertexX[v] - eyeX;
            int32_t y = overlay.vertexY[v] - eyeY;
            int32_t z = overlay.vertexZ[v] - eyeZ;

            int32_t tmp = ((z * sinYaw) + (x * cosYaw)) >> 16;
            z = ((z * cosYaw) - (x * sinYaw)) >> 16;
            x = tmp;

            tmp = ((y * cosPitch) - (z * sinPitch)) >> 16;
            z = ((y * sinPitch) + (z * cosPitch)) >> 16;
            y = tmp;

            if (z < 50) {
                return;
            }

            if (!overlay.triangleTextureIDs.empty()) {
                SceneTileOverlay::tmpViewspaceX[v] = x;
                SceneTileOverlay::tmpViewspaceY[v] = y;
                SceneTileOverlay::tmpViewspaceZ[v] = z;
            }

            SceneTileOverlay::tmpScreenX[v] = Draw3D::centerX + ((x << 9) / z);
            SceneTileOverlay::tmpScreenY[v] = Draw3D::centerY + ((y << 9) / z);
        }

        Draw3D::alpha = 0;
        vertexCount = static_cast<int32_t>(overlay.triangleVertexA.size());
        for (int32_t v = 0; v < vertexCount; v++) {
            int32_t a = overlay.triangleVertexA[v];
            int32_t b = overlay.triangleVertexB[v];
            int32_t c = overlay.triangleVertexC[v];

            int32_t x0 = SceneTileOverlay::tmpScreenX[a];
            int32_t x1 = SceneTileOverlay::tmpScreenX[b];
            int32_t x2 = SceneTileOverlay::tmpScreenX[c];

            int32_t y0 = SceneTileOverlay::tmpScreenY[a];
            int32_t y1 = SceneTileOverlay::tmpScreenY[b];
            int32_t y2 = SceneTileOverlay::tmpScreenY[c];

            if ((((x0 - x1) * (y2 - y1)) - ((y0 - y1) * (x2 - x1))) > 0) {
                Draw3D::clipX = (x0 < 0) || (x1 < 0) || (x2 < 0) || (x0 > Draw2D::boundX) || (x1 > Draw2D::boundX) || (x2 > Draw2D::boundX);

                if (takingInput && PointInsideTriangle(mouseX, mouseY, y0, y1, y2, x0, x1, x2)) {
                    clickTileX = tileX;
                    clickTileZ = tileZ;
                }

                if ((overlay.triangleTextureIDs.empty()) || (overlay.triangleTextureIDs[v] == -1))
                {
                    if (overlay.triangleColorA[v] != 12345678) {
                        Draw3D::FillGouraudTriangle(y0, y1, y2, x0, x1, x2, overlay.triangleColorA[v], overlay.triangleColorB[v], overlay.triangleColorC[v]);
                    }
                } else if (!lowmem) {
                    if (overlay.flat) {
                        Draw3D::FillTexturedTriangle(y0, y1, y2, x0, x1, x2, overlay.triangleColorA[v], overlay.triangleColorB[v], overlay.triangleColorC[v], SceneTileOverlay::tmpViewspaceX[0], SceneTileOverlay::tmpViewspaceX[1], SceneTileOverlay::tmpViewspaceX[3], SceneTileOverlay::tmpViewspaceY[0], SceneTileOverlay::tmpViewspaceY[1], SceneTileOverlay::tmpViewspaceY[3], SceneTileOverlay::tmpViewspaceZ[0], SceneTileOverlay::tmpViewspaceZ[1], SceneTileOverlay::tmpViewspaceZ[3], overlay.triangleTextureIDs[v]);
                    } else {
                        Draw3D::FillTexturedTriangle(y0, y1, y2, x0, x1, x2, overlay.triangleColorA[v], overlay.triangleColorB[v], overlay.triangleColorC[v], SceneTileOverlay::tmpViewspaceX[a], SceneTileOverlay::tmpViewspaceX[b], SceneTileOverlay::tmpViewspaceX[c], SceneTileOverlay::tmpViewspaceY[a], SceneTileOverlay::tmpViewspaceY[b], SceneTileOverlay::tmpViewspaceY[c], SceneTileOverlay::tmpViewspaceZ[a], SceneTileOverlay::tmpViewspaceZ[b], SceneTileOverlay::tmpViewspaceZ[c], overlay.triangleTextureIDs[v]);
                    }
                } else {
                    int32_t k5 = TEXTURE_HSL[overlay.triangleTextureIDs[v]];
                    Draw3D::FillGouraudTriangle(y0, y1, y2, x0, x1, x2, MulLightness(k5, overlay.triangleColorA[v]), MulLightness(k5, overlay.triangleColorB[v]), MulLightness(k5, overlay.triangleColorC[v]));
                }
            }
        }
    }

    bool Scene::DrawTileUnderlayOrOverlay(SceneTile& tile, int32_t x, int32_t z, int32_t level)
    {
        if (tile.underlay.initialized) {
            if (TileVisible(level, x, z)) {
                DrawTileUnderlay(tile.underlay, level, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, x, z);
                return true;
            }
        } else if ((tile.overlay.initialized) && TileVisible(level, x, z)) {
            DrawTileOverlay(x, sinEyePitch, sinEyeYaw, tile.overlay, cosEyePitch, z, cosEyeYaw);
            return true;
        }
        return false;
    }

    int32_t Scene::MulLightness(int32_t hsl, int32_t lightness)
    {
        lightness = 127 - lightness;
        lightness = (lightness * (hsl & 0x7f)) / 160;
        if (lightness < 2) {
            lightness = 2;
        } else if (lightness > 126) {
            lightness = 126;
        }
        return (hsl & 0xff80) + lightness;
    }

    bool Scene::TileVisible(int32_t level, int32_t x, int32_t z)
    {
        int32_t levelCycle = levelTileOcclusionCycles[level][x][z];

        if (levelCycle == -Scene::cycle) {
            return true;
        }

        if (levelCycle == Scene::cycle) {
            return false;
        }

        int32_t sx = x << 7;
        int32_t sz = z << 7;

        if (Occluded(sx + 1, levelHeightmaps[level][x][z], sz + 1) && Occluded((sx + 128) - 1, levelHeightmaps[level][x + 1][z], sz + 1) && Occluded((sx + 128) - 1, levelHeightmaps[level][x + 1][z + 1], (sz + 128) - 1) && Occluded(sx + 1, levelHeightmaps[level][x][z + 1], (sz + 128) - 1)) {
            levelTileOcclusionCycles[level][x][z] = Scene::cycle;
            tilesCulled++;
            return false;
        } else {
            levelTileOcclusionCycles[level][x][z] = -Scene::cycle;
            return true;
        }
        return true;
    }

    void Scene::UpdateActiveOccluders()
    {
        activeOccluderCount = 0;
        activeGroundOccluderCount = 0;
        activeWallOccluderCount = 0;
        tilesCulled = 0;

        int32_t count = levelOccluderCount[topLevel];
        auto occluders = levelOccluders[topLevel];

        for (int32_t i = 0; i < count; i++) {
            SceneOccluder& occluder = occluders[i];

            if (occluder.type == TYPE_WALL_X) {
                int32_t x = (occluder.minTileX - eyeTileX) + 25;

                if ((x < 0) || (x > 50)) {
                    continue;
                }

                // Think of min/maxZ as the relative Z value in our visibility map, the +25 is because
                // the visibility map origin is at 25,25
                int32_t minZ = (occluder.minTileZ - eyeTileZ) + 25;
                int32_t maxZ = (occluder.maxTileZ - eyeTileZ) + 25;

                if (minZ < 0) {
                    minZ = 0;
                }

                if (maxZ > 50) {
                    maxZ = 50;
                }

                bool ok = false;

                // checks if we can at least see one tile in the forward direction starting from our occluder
                while (minZ <= maxZ) {
                    if (visibilityMap[x][minZ++]) {
                        ok = true;
                        break;
                    }
                }

                if (!ok) {
                    continue;
                }

                int32_t deltaMinX = eyeX - occluder.minX;

                if (deltaMinX > 32) {
                    occluder.mode = 1;
                } else {
                    if (deltaMinX >= -32) {
                        continue;
                    }
                    occluder.mode = 2;
                    deltaMinX = -deltaMinX;
                }

                occluder.minDeltaZ = ((occluder.minZ - eyeZ) << 10) / deltaMinX;
                occluder.maxDeltaZ = ((occluder.maxZ - eyeZ) << 10) / deltaMinX;
                occluder.minDeltaY = ((occluder.minY - eyeY) << 10) / deltaMinX;
                occluder.maxDeltaY = ((occluder.maxY - eyeY) << 10) / deltaMinX;
                activeOccluders[activeOccluderCount++] = occluder;
                activeWallOccluderCount++;
                continue;
            }

            if (occluder.type == TYPE_WALL_Z) {
                int32_t distanceMinTileZ = (occluder.minTileZ - eyeTileZ) + 25;

                if ((distanceMinTileZ < 0) || (distanceMinTileZ > 50)) {
                    continue;
                }

                int32_t distanceMinTileX = (occluder.minTileX - eyeTileX) + 25;

                if (distanceMinTileX < 0) {
                    distanceMinTileX = 0;
                }

                int32_t distanceMaxTileX = (occluder.maxTileX - eyeTileX) + 25;

                if (distanceMaxTileX > 50) {
                    distanceMaxTileX = 50;
                }

                bool ok = false;

                while (distanceMinTileX <= distanceMaxTileX) {
                    if (visibilityMap[distanceMinTileX++][distanceMinTileZ]) {
                        ok = true;
                        break;
                    }
                }

                if (!ok) {
                    continue;
                }

                int32_t deltaMinZ = eyeZ - occluder.minZ;

                if (deltaMinZ > 32) {
                    occluder.mode = 3;
                } else {
                    if (deltaMinZ >= -32) {
                        continue;
                    }
                    occluder.mode = 4;
                    deltaMinZ = -deltaMinZ;
                }

                occluder.minDeltaX = ((occluder.minX - eyeX) << 10) / deltaMinZ;
                occluder.maxDeltaX = ((occluder.maxX - eyeX) << 10) / deltaMinZ;
                occluder.minDeltaY = ((occluder.minY - eyeY) << 10) / deltaMinZ;
                occluder.maxDeltaY = ((occluder.maxY - eyeY) << 10) / deltaMinZ;
                activeOccluders[activeOccluderCount++] = occluder;
                activeWallOccluderCount++;
            } else if (occluder.type == TYPE_GROUND) {
                int32_t deltaMaxY = occluder.minY - eyeY;

                if (deltaMaxY <= 128) {
                    continue;
                }

                int32_t deltaMinTileZ = (occluder.minTileZ - eyeTileZ) + 25;

                if (deltaMinTileZ < 0) {
                    deltaMinTileZ = 0;
                }

                int32_t deltaMaxTileZ = (occluder.maxTileZ - eyeTileZ) + 25;

                if (deltaMaxTileZ > 50) {
                    deltaMaxTileZ = 50;
                }

                if (deltaMinTileZ <= deltaMaxTileZ) {
                    int32_t deltaMinTileX = (occluder.minTileX - eyeTileX) + 25;

                    if (deltaMinTileX < 0) {
                        deltaMinTileX = 0;
                    }

                    int32_t deltaMaxTileX = (occluder.maxTileX - eyeTileX) + 25;

                    if (deltaMaxTileX > 50) {
                        deltaMaxTileX = 50;
                    }

                    bool ok = false;

                    for (int32_t x = deltaMinTileX; x <= deltaMaxTileX && !ok; x++) {
                        for (int32_t z = deltaMinTileZ; z <= deltaMaxTileZ; z++) {
                            if (visibilityMap[x][z]) {
                                ok = true;
                                break; // breaks inner loop, outer loop sees ok==true and stops
                            }
                        }
                    }

                    if (ok) {
                        occluder.mode = 5;
                        occluder.minDeltaX = ((occluder.minX - eyeX) << 10) / deltaMaxY;
                        occluder.maxDeltaX = ((occluder.maxX - eyeX) << 10) / deltaMaxY;
                        occluder.minDeltaZ = ((occluder.minZ - eyeZ) << 10) / deltaMaxY;
                        occluder.maxDeltaZ = ((occluder.maxZ - eyeZ) << 10) / deltaMaxY;
                        activeOccluders[activeOccluderCount++] = occluder;
                        activeGroundOccluderCount++;
                    }
                }
            }
        }
    }

    void Scene::Reset()
    {
        levelTiles.fill();

        for (int32_t l = 0; l < LEVEL_COUNT; l++) {
            for (int32_t j1 = 0; j1 < levelOccluderCount[l]; j1++) {
                levelOccluders[l][j1] = SceneOccluder{};
            }
            levelOccluderCount[l] = 0;
        }

        for (int32_t i = 0; i < temporaryLocCount; i++) {
            temporaryLocs[i] = nullptr;
        }

        temporaryLocCount = 0;
        std::ranges::fill(locBuffer, nullptr);

    }

    bool Scene::Add(const std::shared_ptr<Entity>& entity, int32_t level, int32_t tileX, int32_t tileZ, int32_t y, int32_t width,
        int32_t length, int32_t yaw, int32_t bitset, int8_t info)
    {
        if (entity == nullptr) {
            return true;
        } else {
            int32_t sceneX = (tileX * 128) + (64 * width);
            int32_t sceneZ = (tileZ * 128) + (64 * length);
            return Add(entity, level, tileX, tileZ, width, length, sceneX, sceneZ, y, yaw, bitset, info, false);
        }
    }

    bool Scene::Add(const std::shared_ptr<Entity>& entity, int32_t level, int32_t tileX, int32_t tileZ, int32_t tileSizeX, int32_t tileSizeZ, int32_t x, int32_t z, int32_t y,
        int32_t yaw, int32_t bitset, int8_t info, bool temporary)
    {

        for (int32_t tx = tileX; tx < (tileX + tileSizeX); tx++) {
            for (int32_t tz = tileZ; tz < (tileZ + tileSizeZ); tz++) {
                if ((tx < 0) || (tz < 0) || (tx >= maxTileX) || (tz >= maxTileZ)) {
                    return false;
                }
                auto& tile = levelTiles[level][tx][tz];
                if ((tile.initialized) && (tile.locCount >= 5)) {
                    return false;
                }
            }
        }
        SceneLoc loc;
        loc.bitset = bitset;
        loc.info = info;
        loc.level = level;
        loc.x = x;
        loc.z = z;
        loc.y = y;
        loc.entity = entity;
        loc.yaw = yaw;
        loc.minSceneTileX = tileX;
        loc.minSceneTileZ = tileZ;
        loc.maxSceneTileX = (tileX + tileSizeX) - 1;
        loc.maxSceneTileZ = (tileZ + tileSizeZ) - 1;

        for (int32_t tx = tileX; tx < (tileX + tileSizeX); tx++) {
            for (int32_t tz = tileZ; tz < (tileZ + tileSizeZ); tz++) {
                int32_t spans = 0;

                if (tx > tileX) {
                    spans |= 0b0001;
                }

                if (tx < ((tileX + tileSizeX) - 1)) {
                    spans |= 0b0100;
                }

                if (tz > tileZ) {
                    spans |= 0b1000;
                }

                if (tz < ((tileZ + tileSizeZ) - 1)) {
                    spans |= 0b0010;
                }

                for (int32_t p = level; p >= 0; p--) {
                    if (!levelTiles[p][tx][tz].initialized) {
                        levelTiles[p][tx][tz] = SceneTile(p, tx, tz);
                    }
                }
                auto& tile = levelTiles[level][tx][tz];
                tile.locs[tile.locCount] = std::make_shared<SceneLoc>(loc);
                tile.locSpan[tile.locCount] = spans;
                tile.locSpans |= spans;
                tile.locCount++;
            }
        }

        if (temporary) {
            temporaryLocs[temporaryLocCount++] = std::make_shared<SceneLoc>(loc);;
        }

        return true;
    }

    bool Scene::Occluded(int32_t x, int32_t y, int32_t z)
    {
        for (int32_t i = 0; i < activeOccluderCount; i++) {
            SceneOccluder& occluder = activeOccluders[i];

            if (occluder.mode == 1) {
                int32_t dx = occluder.minX - x;
                if (dx <= 0) {
                    continue;
                }

                int32_t minZ = occluder.minZ + ((occluder.minDeltaZ * dx) >> 10);
                int32_t maxZ = occluder.maxZ + ((occluder.maxDeltaZ * dx) >> 10);
                int32_t minY = occluder.minY + ((occluder.minDeltaY * dx) >> 10);
                int32_t maxY = occluder.maxY + ((occluder.maxDeltaY * dx) >> 10);
                if ((z >= minZ) && (z <= maxZ) && (y >= minY) && (y <= maxY)) {
                    return true;
                }
            } else if (occluder.mode == 2) {
                int32_t dx = x - occluder.minX;
                if (dx <= 0) {
                    continue;
                }
                int32_t minZ = occluder.minZ + ((occluder.minDeltaZ * dx) >> 10);
                int32_t macZ = occluder.maxZ + ((occluder.maxDeltaZ * dx) >> 10);
                int32_t minY = occluder.minY + ((occluder.minDeltaY * dx) >> 10);
                int32_t maxY = occluder.maxY + ((occluder.maxDeltaY * dx) >> 10);
                if ((z >= minZ) && (z <= macZ) && (y >= minY) && (y <= maxY)) {
                    return true;
                }
            } else if (occluder.mode == 3) {
                int32_t dz = occluder.minZ - z;
                if (dz <= 0) {
                    continue;
                }
                int32_t minX = occluder.minX + ((occluder.minDeltaX * dz) >> 10);
                int32_t maxX = occluder.maxX + ((occluder.maxDeltaX * dz) >> 10);
                int32_t minY = occluder.minY + ((occluder.minDeltaY * dz) >> 10);
                int32_t maxY = occluder.maxY + ((occluder.maxDeltaY * dz) >> 10);
                if ((x >= minX) && (x <= maxX) && (y >= minY) && (y <= maxY)) {
                    return true;
                }
            } else if (occluder.mode == 4) {
                int32_t dz = z - occluder.minZ;
                if (dz <= 0) {
                    continue;
                }
                int32_t minX = occluder.minX + ((occluder.minDeltaX * dz) >> 10);
                int32_t maxX = occluder.maxX + ((occluder.maxDeltaX * dz) >> 10);
                int32_t minY = occluder.minY + ((occluder.minDeltaY * dz) >> 10);
                int32_t maxY = occluder.maxY + ((occluder.maxDeltaY * dz) >> 10);
                if ((x >= minX) && (x <= maxX) && (y >= minY) && (y <= maxY)) {
                    return true;
                }
            } else if (occluder.mode == 5) {
                int32_t dy = y - occluder.minY;
                if (dy <= 0) {
                    continue;
                }
                int32_t minX = occluder.minX + ((occluder.minDeltaX * dy) >> 10);
                int32_t maxX = occluder.maxX + ((occluder.maxDeltaX * dy) >> 10);
                int32_t minZ = occluder.minZ + ((occluder.minDeltaZ * dy) >> 10);
                int32_t maxZ = occluder.maxZ + ((occluder.maxDeltaZ * dy) >> 10);
                if ((x >= minX) && (x <= maxX) && (z >= minZ) && (z <= maxZ)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool Scene::PointInsideTriangle(int32_t x, int32_t y, int32_t y0, int32_t y1, int32_t y2, int32_t x0, int32_t x1,
                                    int32_t x2)
    {
        if ((y < y0) && (y < y1) && (y < y2)) {
            return false;
        }
        if ((y > y0) && (y > y1) && (y > y2)) {
            return false;
        }
        if ((x < x0) && (x < x1) && (x < x2)) {
            return false;
        }
        if ((x > x0) && (x > x1) && (x > x2)) {
            return false;
        }
        int32_t i2 = ((y - y0) * (x1 - x0)) - ((x - x0) * (y1 - y0));
        int32_t j2 = ((y - y2) * (x0 - x2)) - ((x - x2) * (y0 - y2));
        int32_t k2 = ((y - y1) * (x2 - x1)) - ((x - x1) * (y2 - y1));
        return ((i2 * k2) > 0) && ((k2 * j2) > 0);
    }

    bool Scene::LocVisible(int32_t level,
                       int32_t tileMinX, int32_t tileMaxX,
                       int32_t tileMinZ, int32_t tileMaxZ,
                       int32_t y)
    {
        if ((tileMinX == tileMaxX) && (tileMinZ == tileMaxZ)) {
            if (TileVisible(level, tileMinX, tileMinZ)) {
                return true;
            }

            int32_t x = tileMinX << 7;
            int32_t z = tileMinZ << 7;

            return !Occluded(x + 1,  levelHeightmaps[level][tileMinX][tileMinZ]           - y, z + 1) ||
                   !Occluded((x + 128) - 1, levelHeightmaps[level][tileMinX + 1][tileMinZ]   - y, z + 1) ||
                   !Occluded((x + 128) - 1, levelHeightmaps[level][tileMinX + 1][tileMinZ + 1] - y, (z + 128) - 1) ||
                   !Occluded(x + 1,  levelHeightmaps[level][tileMinX][tileMinZ + 1]       - y, (z + 128) - 1);
        }

        // Make sure we clamp the loops to the scene's valid bounds.
        for (int32_t stx = tileMinX; stx <= tileMaxX && stx < this->maxTileX; stx++) {
            for (int32_t stz = tileMinZ; stz <= tileMaxZ && stz < this->maxTileZ; stz++) {
                if (levelTileOcclusionCycles[level][stx][stz] == -cycle) {
                    return true;
                }
            }
        }

        int32_t x0 = (tileMinX << 7) + 1;
        int32_t z0 = (tileMinZ << 7) + 2;
        int32_t y0 = levelHeightmaps[level][tileMinX][tileMinZ] - y;

        if (!Occluded(x0, y0, z0)) return true;

        int32_t x1 = (tileMaxX << 7) - 1;
        if (!Occluded(x1, y0, z0)) return true;

        int32_t z1 = (tileMaxZ << 7) - 1;
        if (!Occluded(x0, y0, z1)) return true;

        return !Occluded(x1, y0, z1);
    }

    /**
     * Merges touching normals of all Locs (Walls, Ground Decorations, etc) and then reapplies their lighting.
     */
    void Scene::BuildModels(int32_t lightAmbient, int32_t lightAttenuation, int32_t lightSrcX, int32_t lightSrcY,
        int32_t lightSrcZ)
    {
        auto lightMagnitude = static_cast<int32_t>(std::sqrt((lightSrcX * lightSrcX) + (lightSrcY * lightSrcY) + (lightSrcZ * lightSrcZ)));
        int32_t attenuation = (lightAttenuation * lightMagnitude) >> 8;
        for (int32_t level = 0; level < maxLevel; level++) {
            for (int32_t tileX = 0; tileX < maxTileX; tileX++) {
                for (int32_t tileZ = 0; tileZ < maxTileZ; tileZ++) {
                    const auto& tile = levelTiles[level][tileX][tileZ];

                    if (!tile.initialized) {
                        continue;
                    }

                    const auto& wall = tile.wall;

                    if ((wall != nullptr) && (wall->entityA != nullptr) && (!wall->entityA->vertexNormal.empty())) {
                        const auto& modelA = std::static_pointer_cast<Model>(wall->entityA);
                        MergeLocNormals(level, 1, 1, tileX, tileZ, *modelA);

                        if ((wall->entityB != nullptr) && (!wall->entityB->vertexNormal.empty())) {
                            const auto& modelB = std::static_pointer_cast<Model>(wall->entityB);
                            MergeLocNormals(level, 1, 1, tileX, tileZ, *modelB);
                            MergeNormals(*modelA, *modelB, 0, 0, 0, false);
                            modelB->ApplyLighting(lightAmbient, attenuation, lightSrcX, lightSrcY, lightSrcZ);
                        }

                        modelA->ApplyLighting(lightAmbient, attenuation, lightSrcX, lightSrcY, lightSrcZ);
                    }

                    for (int32_t i = 0; i < tile.locCount; i++) {
                        const auto& loc = tile.locs[i];

                        if ((loc != nullptr) && (loc->entity != nullptr) && (!loc->entity->vertexNormal.empty())) {
                            const auto& modelLoc = std::static_pointer_cast<Model>(loc->entity);
                            MergeLocNormals(level, (loc->maxSceneTileX - loc->minSceneTileX) + 1, (loc->maxSceneTileZ - loc->minSceneTileZ) + 1, tileX, tileZ, *modelLoc);
                            modelLoc->ApplyLighting(lightAmbient, attenuation, lightSrcX, lightSrcY, lightSrcZ);
                        }
                    }

                    const auto& decoration = tile.groundDecoration;
                    if ((decoration != nullptr) && (!decoration->entity->vertexNormal.empty())) {
                        const auto& modelDec = std::static_pointer_cast<Model>(decoration->entity);
                        MergeGroundDecorationNormals(tileX, level, *modelDec, tileZ);
                        modelDec->ApplyLighting(lightAmbient, attenuation, lightSrcX, lightSrcY, lightSrcZ);
                    }
                }
            }
        }
    }

    int32_t Scene::GetWallDecorationBitset(int32_t level, int32_t x, int32_t z)
    {
        const auto& tile = levelTiles[level][x][z];
        if ((!tile.initialized) || (tile.wallDecoration == nullptr)) {
            return 0;
        } else {
            return tile.wallDecoration->bitset;
        }
    }

    std::shared_ptr<SceneWallDecoration> Scene::GetWallDecoration(int32_t level, int32_t x, int32_t z)
    {
        const auto& tile = levelTiles[level][x][z];
        if (tile.initialized) {
            return tile.wallDecoration;
        } else {
            return nullptr;
        }
    }

    int32_t Scene::GetLocBitset(int32_t level, int32_t x, int32_t z)
    {
        auto& tile = levelTiles[level][x][z];
        if (!tile.initialized) {
            return 0;
        }
        for (int32_t l = 0; l < tile.locCount; l++) {
            const auto& loc = tile.locs[l];
            if ((((loc->bitset >> 29) & 3) == 2) && (loc->minSceneTileX == x) && (loc->minSceneTileZ == z)) {
                return loc->bitset;
            }
        }
        return 0;
    }

    std::shared_ptr<SceneLoc> Scene::GetLoc(int32_t level, int32_t x, int32_t z)
    {
        auto tile = levelTiles[level][x][z];
        if (!tile.initialized) {
            return nullptr;
        }
        for (int32_t l = 0; l < tile.locCount; l++) {
            const auto& loc = tile.locs[l];
            if ((loc->bitset >> 29 & 3) == 2 && (loc->minSceneTileX == x) && (loc->minSceneTileZ == z)) {
                return loc;
            }
        }
        return nullptr;
    }

    int32_t Scene::GetGroundDecorationBitset(int32_t level, int32_t x, int32_t z)
    {
        auto& tile = levelTiles[level][x][z];
        if ((!tile.initialized) || (tile.groundDecoration == nullptr)) {
            return 0;
        } else {
            return tile.groundDecoration->bitset;
        }
    }

    std::shared_ptr<SceneGroundDecoration> Scene::GetGroundDecoration(int32_t z, int32_t x, int32_t level)
    {
        const auto& tile = levelTiles[level][x][z];
        if (!tile.initialized && (tile.groundDecoration != nullptr)) {
            return tile.groundDecoration;
        } else {
            return nullptr;
        }
    }

    int32_t Scene::GetInfo(int32_t level, int32_t x, int32_t z, int32_t bitset)
    {
        auto& tile = levelTiles[level][x][z];

        if (!tile.initialized) {
            return -1;
        }

        if ((tile.wall != nullptr) && (tile.wall->bitset == bitset)) {
            return tile.wall->info & 0xff;
        }

        if ((tile.wallDecoration != nullptr) && (tile.wallDecoration->bitset == bitset)) {
            return tile.wallDecoration->info & 0xff;
        }

        if ((tile.groundDecoration != nullptr) && (tile.groundDecoration->bitset == bitset)) {
            return tile.groundDecoration->info & 0xff;
        }

        for (int32_t i = 0; i < tile.locCount; i++) {
            if (tile.locs[i]->bitset == bitset) {
                return tile.locs[i]->info & 0xff;
            }
        }
        return -1;
    }

    std::shared_ptr<SceneWall> Scene::GetWall(int32_t level, int32_t x, int32_t z)
    {
        auto tile = levelTiles[level][x][z];
        if (tile.initialized) {
            return tile.wall;
        } else {
            return nullptr;
        }
    }

    void Scene::RemoveObjStack(int32_t level, int32_t x, int32_t z)
    {
        auto& tile = levelTiles[level][x][z];
        if (tile.initialized) {
            tile.objStack = nullptr;
        }
    }

    void Scene::AddObjStack(const std::shared_ptr<Entity>& topObj, const std::shared_ptr<Entity>& bottomObj, const std::shared_ptr<Entity>& middleObj,
                        int32_t level, int32_t stx, int32_t stz, int32_t y, int32_t bitset)
    {
        SceneObjStack stack;
        stack.x = (stx * 128) + 64;
        stack.z = (stz * 128) + 64;
        stack.y = y;
        stack.bitset = bitset;
        stack.topObj = topObj;
        stack.bottomObj = bottomObj;
        stack.middleObj = middleObj;

        int32_t stackOffset = 0;

        auto& tile = levelTiles[level][stx][stz];

        if (tile.initialized) {
            for (int32_t l = 0; l < tile.locCount; ++l) {
                auto& loc = tile.locs[l];

                if (auto model = std::dynamic_pointer_cast<Model>(loc->entity)) {
                    int32_t height = model->objRaise;
                    if (height > stackOffset) {
                        stackOffset = height;
                    }
                }
            }
        }

        stack.offset = stackOffset;

        if (!levelTiles[level][stx][stz].initialized) {
            levelTiles[level][stx][stz] = SceneTile(level, stx, stz);
        }
        levelTiles[level][stx][stz].objStack = std::make_shared<SceneObjStack>(stack);
    }

    bool Scene::AddTemporary(const std::shared_ptr<Entity>& entity, int32_t level, int32_t x, int32_t z, int32_t y, int32_t yaw,
        int32_t bitset, bool forwardPadding, int32_t padding)
    {
        if (entity == nullptr) {
            return true;
        }

        int32_t x0 = x - padding;
        int32_t z0 = z - padding;
        int32_t x1 = x + padding;
        int32_t z1 = z + padding;

        if (forwardPadding) {
            if ((yaw > 640) && (yaw < 1408)) {
                z1 += 128;
            }
            if ((yaw > 1152) && (yaw < 1920)) {
                x1 += 128;
            }
            if ((yaw > 1664) || (yaw < 384)) {
                z0 -= 128;
            }
            if ((yaw > 128) && (yaw < 896)) {
                x0 -= 128;
            }
        }

        x0 /= 128;
        z0 /= 128;
        x1 /= 128;
        z1 /= 128;
        return Add(entity, level, x0, z0, (x1 - x0) + 1, (z1 - z0) + 1, x, z, y, yaw, bitset, 0, true);
    }

    bool Scene::AddTemporary(const std::shared_ptr<Entity>& entity, int32_t level, int32_t minTileX, int32_t minTileZ,
        int32_t maxTileXC, int32_t maxTileZC, int32_t x, int32_t z, int32_t y, int32_t yaw, int32_t bitset)
    {
        if (entity == nullptr) {
            return true;
        } else {
            return Add(entity, level, minTileX, minTileZ, (maxTileXC - minTileX) + 1, (maxTileZC - minTileZ) + 1, x, z, y, yaw, bitset, 0, true);
        }
    }

    bool Scene::AddTemporary(Entity* entity, int32_t level, int32_t x, int32_t z, int32_t y, int32_t yaw,
        int32_t bitset, bool forwardPadding, int32_t padding)
    {
        if (entity == nullptr) {
            return true;
        }
        // Create a non-owning shared_ptr (null deleter) since DoublyLinkedList owns the entity
        return AddTemporary(std::shared_ptr<Entity>(entity, [](Entity*){}), level, x, z, y, yaw, bitset, forwardPadding, padding);
    }

    bool Scene::AddTemporary(Entity* entity, int32_t level, int32_t minTileX, int32_t minTileZ,
        int32_t maxTileXC, int32_t maxTileZC, int32_t x, int32_t z, int32_t y, int32_t yaw, int32_t bitset)
    {
        if (entity == nullptr) {
            return true;
        }
        // Create a non-owning shared_ptr (null deleter) since DoublyLinkedList owns the entity
        return AddTemporary(std::shared_ptr<Entity>(entity, [](Entity*){}), level, minTileX, minTileZ, maxTileXC, maxTileZC, x, z, y, yaw, bitset);
    }

    void Scene::RemoveWall(int32_t x, int32_t level, int32_t z)
    {
        auto& tile = levelTiles[level][x][z];
        if (tile.initialized) {
            tile.wall = nullptr;
        }
    }

    void Scene::RemoveWallDecoration(int32_t level, int32_t x, int32_t z)
    {
        auto& tile = levelTiles[level][x][z];
        if (tile.initialized) {
            tile.wallDecoration = nullptr;
        }
    }

    void Scene::RemoveGroundDecoration(int32_t level, int32_t x, int32_t z)
    {
        auto& tile = levelTiles[level][x][z];
        if (tile.initialized) {
            tile.groundDecoration = nullptr;
        }
    }

    void Scene::RemoveLoc(int32_t level, int32_t x, int32_t z)
    {
        auto& tile = levelTiles[level][x][z];
        if (!tile.initialized) {
            return;
        }
        for (int32_t j1 = 0; j1 < tile.locCount; j1++) {
            auto loc = tile.locs[j1];  // Copy, not reference - array is modified during RemoveLoc
            if ((((loc->bitset >> 29) & 3) == 2) && (loc->minSceneTileX == x) && (loc->minSceneTileZ == z)) {
                RemoveLoc(loc);
                return;
            }
        }
    }

    void Scene::RemoveLoc(const std::shared_ptr<SceneLoc>& loc)
    {
        int removedCount = 0;
        for (int32_t tx = loc->minSceneTileX; tx <= loc->maxSceneTileX; tx++) {
            for (int32_t tz = loc->minSceneTileZ; tz <= loc->maxSceneTileZ; tz++) {
                auto& tile = levelTiles[loc->level][tx][tz];

                if (!tile.initialized) {
                    continue;
                }

                for (int32_t i = 0; i < tile.locCount; i++) {
                    if (*tile.locs[i] != *loc) {
                        continue;
                    }


                    removedCount++;
                    tile.locCount--;

                    for (int32_t j = i; j < tile.locCount; j++) {
                        tile.locs[j] = tile.locs[j + 1];
                        tile.locSpan[j] = tile.locSpan[j + 1];
                    }
                    tile.locs[tile.locCount] = std::make_shared<SceneLoc>();
                    break;
                }

                tile.locSpans = 0;

                for (int32_t j1 = 0; j1 < tile.locCount; j1++) {
                    tile.locSpans |= tile.locSpan[j1];
                }
            }
        }
    }

    void Scene::ClearTemporaryLocs()
    {
        for (int32_t i = 0; i < temporaryLocCount; i++) {
            auto& loc = temporaryLocs[i];
            RemoveLoc(loc);
            temporaryLocs[i] = std::make_shared<SceneLoc>();
        }
        temporaryLocCount = 0;
    }

    void Scene::MergeLocNormals(int32_t level, int32_t tileSizeX, int32_t tileSizeZ, int32_t tileX, int32_t tileZ,
                                Model& model)
    {
        bool allowFaceRemoval = true;
        int32_t minTileX = tileX;
        int32_t endTileX = tileX + tileSizeX;
        int32_t minTileZ = tileZ - 1;
        int32_t endTileZ = tileZ + tileSizeZ;

        for (int32_t l = level; l <= (level + 1); l++) {
            if (l == maxLevel) {
                continue;
            }

            for (int32_t x = minTileX; x <= endTileX; x++) {
                if ((x < 0) || (x >= this->maxTileX)) {
                    continue;
                }

                for (int32_t z = minTileZ; z <= endTileZ; z++) {
                    // use scene bounds (member variables), not the local endTileZ
                    if ((z < 0) || (z >= this->maxTileZ) ||
                        (allowFaceRemoval && (x < endTileX) && (z < endTileZ) && ((z >= tileZ) || (x == tileX)))) {
                        continue;
                    }

                    const auto& tile = levelTiles[l][x][z];

                    if (!tile.initialized) {
                        continue;
                    }

                    int32_t offsetY = ((levelHeightmaps[l][x][z] + levelHeightmaps[l][x + 1][z] + levelHeightmaps[l][x][z + 1] + levelHeightmaps[l][x + 1][z + 1]) / 4)
                                      - ((levelHeightmaps[level][tileX][tileZ] + levelHeightmaps[level][tileX + 1][tileZ] + levelHeightmaps[level][tileX][tileZ + 1] + levelHeightmaps[level][tileX + 1][tileZ + 1]) / 4);

                    const auto& wall = tile.wall;

                    int32_t offsetX = ((x - tileX) * 128) + ((1 - tileSizeX) * 64);
                    int32_t offsetZ = ((z - tileZ) * 128) + ((1 - tileSizeZ) * 64);

                    if ((wall != nullptr) && (wall->entityA != nullptr) && (!wall->entityA->vertexNormal.empty())) {
                        const auto& modelA = std::static_pointer_cast<Model>(wall->entityA);
                        MergeNormals(model, *modelA, offsetX, offsetY, offsetZ, allowFaceRemoval);
                    }

                    if ((wall != nullptr) && (wall->entityB != nullptr) && (!wall->entityB->vertexNormal.empty())) {
                        const auto& modelB = std::static_pointer_cast<Model>(wall->entityB);
                        MergeNormals(model, *modelB, offsetX, offsetY, offsetZ, allowFaceRemoval);
                    }

                    for (int32_t i = 0; i < tile.locCount; i++) {
                        auto& loc = tile.locs[i];

                        if ((loc->entity != nullptr) && (!loc->entity->vertexNormal.empty())) {
                            int32_t locTileSizeX = (loc->maxSceneTileX - loc->minSceneTileX) + 1;
                            int32_t locTileSizeZ = (loc->maxSceneTileZ - loc->minSceneTileZ) + 1;
                            const auto& locModel = std::static_pointer_cast<Model>(loc->entity);
                            MergeNormals(model, *locModel,
                                         ((loc->minSceneTileX - tileX) * 128) + ((locTileSizeX - tileSizeX) * 64),
                                         offsetY,
                                         ((loc->minSceneTileZ - tileZ) * 128) + ((locTileSizeZ - tileSizeZ) * 64),
                                         allowFaceRemoval);
                        }
                    }
                }
            }

            minTileX--;
            allowFaceRemoval = false;
        }
    }

    void Scene::MergeNormals(Model& modelA, Model& modelB, int32_t offsetX, int32_t offsetY, int32_t offsetZ,
        bool allowFaceRemoval)
    {
        tmpMergeIndex++;
        int32_t merged = 0;
        for (int32_t vertexA = 0; vertexA < modelA.vertexCount; vertexA++) {
            auto& normalA = modelA.vertexNormal[vertexA];
            auto& originalNormalA = modelA.vertexNormalOriginal[vertexA];

            // undefined normal
            if (originalNormalA.w == 0) {
                continue;
            }

            const int32_t y = modelA.vertexY[vertexA] - offsetY;

            if (y > modelB.maxY) {
                continue;
            }

            const int32_t x = modelA.vertexX[vertexA] - offsetX;

            if ((x < modelB.minX) || (x > modelB.maxX)) {
                continue;
            }

            const int32_t z = modelA.vertexZ[vertexA] - offsetZ;

            if ((z < modelB.minZ) || (z > modelB.maxZ)) {
                continue;
            }

            for (int32_t vertexB = 0; vertexB < modelB.vertexCount; vertexB++) {
                auto& normalB = modelB.vertexNormal[vertexB];
                auto& originalNormalB = modelB.vertexNormalOriginal[vertexB];

                if ((x == modelB.vertexX[vertexB]) && (z == modelB.vertexZ[vertexB]) && (y == modelB.vertexY[vertexB]) && (originalNormalB.w != 0)) {

                    normalA.x += originalNormalB.x;
                    normalA.y += originalNormalB.y;
                    normalA.z += originalNormalB.z;
                    normalA.w += originalNormalB.w;

                    normalB.x += originalNormalA.x;
                    normalB.y += originalNormalA.y;
                    normalB.z += originalNormalA.z;
                    normalB.w += originalNormalA.w;

                    merged++;
                    mergeIndexA[vertexA] = tmpMergeIndex;
                    mergeIndexB[vertexB] = tmpMergeIndex;
                }
            }
        }

        if ((merged < 3) || !allowFaceRemoval) {
            return;
        }

        // if every vertex of a given face had their normals merged, clear the face info causing that face not to draw.
        for (int32_t i = 0; i < modelA.faceCount; i++) {
            if ((mergeIndexA[modelA.faceVertexA[i]] == tmpMergeIndex) && (mergeIndexA[modelA.faceVertexB[i]] == tmpMergeIndex) && (mergeIndexA[modelA.faceVertexC[i]] == tmpMergeIndex)) {
                modelA.faceInfo[i] = -1;
            }
        }

        // same as above but for model B
        for (int32_t i = 0; i < modelB.faceCount; i++) {
            if ((mergeIndexB[modelB.faceVertexA[i]] == tmpMergeIndex) && (mergeIndexB[modelB.faceVertexB[i]] == tmpMergeIndex) && (mergeIndexB[modelB.faceVertexC[i]] == tmpMergeIndex)) {
                modelB.faceInfo[i] = -1;
            }
        }
    }

    void Scene::MergeGroundDecorationNormals(int32_t tileX, int32_t level, Model& model, int32_t tileZ)
    {
        if (tileX < maxTileX) {
            auto& tile = levelTiles[level][tileX + 1][tileZ];
            if ((tile.initialized) && (tile.groundDecoration != nullptr) && (!tile.groundDecoration->entity->vertexNormal.empty())) {
                const auto& locModel = std::static_pointer_cast<Model>(tile.groundDecoration->entity);
                MergeNormals(model, *locModel, 128, 0, 0, true);
            }
        }

        if (tileZ < maxTileX) {
            auto& tile = levelTiles[level][tileX][tileZ + 1];
            if ((tile.initialized) && (tile.groundDecoration != nullptr) && (!tile.groundDecoration->entity->vertexNormal.empty())) {
                const auto& locModel = std::static_pointer_cast<Model>(tile.groundDecoration->entity);
                MergeNormals(model, *locModel, 0, 0, 128, true);
            }
        }

        if ((tileX < maxTileX) && (tileZ < maxTileZ)) {
            auto& tile = levelTiles[level][tileX + 1][tileZ + 1];
            if ((tile.initialized) && (tile.groundDecoration != nullptr) && (!tile.groundDecoration->entity->vertexNormal.empty())) {
                const auto& locModel = std::static_pointer_cast<Model>(tile.groundDecoration->entity);
                MergeNormals(model, *locModel, 128, 0, 128, true);
            }
        }

        if ((tileX < maxTileX) && (tileZ > 0)) {
            auto& tile = levelTiles[level][tileX + 1][tileZ - 1];
            if ((tile.initialized) && (tile.groundDecoration != nullptr) && (!tile.groundDecoration->entity->vertexNormal.empty())) {
                const auto& locModel = std::static_pointer_cast<Model>(tile.groundDecoration->entity);
                MergeNormals(model, *locModel, 128, 0, -128, true);
            }
        }
    }

    bool Scene::WallVisible(int32_t level, int32_t tileX, int32_t tileZ, int32_t type)
    {
        if (TileVisible(level, tileX, tileZ)) {
            return true;
        }

        int32_t sceneX = tileX << 7;
        int32_t sceneZ = tileZ << 7;
        int32_t sceneY = levelHeightmaps[level][tileX][tileZ] - 1;
        int32_t y0 = sceneY - 120;
        int32_t y1 = sceneY - 230;
        int32_t y2 = sceneY - 238;

        if (type < 16) {
            if (type == 1) {
                if (sceneX > eyeX) {
                    if (!Occluded(sceneX, sceneY, sceneZ)) {
                        return true;
                    }
                    if (!Occluded(sceneX, sceneY, sceneZ + 128)) {
                        return true;
                    }
                }
                if (level > 0) {
                    if (!Occluded(sceneX, y0, sceneZ)) {
                        return true;
                    }
                    if (!Occluded(sceneX, y0, sceneZ + 128)) {
                        return true;
                    }
                }
                if (!Occluded(sceneX, y1, sceneZ)) {
                    return true;
                }
                return !Occluded(sceneX, y1, sceneZ + 128);
            } else if (type == 2) {
                if (sceneZ < eyeZ) {
                    if (!Occluded(sceneX, sceneY, sceneZ + 128)) {
                        return true;
                    }
                    if (!Occluded(sceneX + 128, sceneY, sceneZ + 128)) {
                        return true;
                    }
                }
                if (level > 0) {
                    if (!Occluded(sceneX, y0, sceneZ + 128)) {
                        return true;
                    }
                    if (!Occluded(sceneX + 128, y0, sceneZ + 128)) {
                        return true;
                    }
                }
                if (!Occluded(sceneX, y1, sceneZ + 128)) {
                    return true;
                }
                return !Occluded(sceneX + 128, y1, sceneZ + 128);
            } else if (type == 4) {
                if (sceneX < eyeX) {
                    if (!Occluded(sceneX + 128, sceneY, sceneZ)) {
                        return true;
                    }
                    if (!Occluded(sceneX + 128, sceneY, sceneZ + 128)) {
                        return true;
                    }
                }
                if (level > 0) {
                    if (!Occluded(sceneX + 128, y0, sceneZ)) {
                        return true;
                    }
                    if (!Occluded(sceneX + 128, y0, sceneZ + 128)) {
                        return true;
                    }
                }
                if (!Occluded(sceneX + 128, y1, sceneZ)) {
                    return true;
                }
                return !Occluded(sceneX + 128, y1, sceneZ + 128);
            } else if (type == 8) {
                if (sceneZ > eyeZ) {
                    if (!Occluded(sceneX, sceneY, sceneZ)) {
                        return true;
                    }
                    if (!Occluded(sceneX + 128, sceneY, sceneZ)) {
                        return true;
                    }
                }
                if (level > 0) {
                    if (!Occluded(sceneX, y0, sceneZ)) {
                        return true;
                    }
                    if (!Occluded(sceneX + 128, y0, sceneZ)) {
                        return true;
                    }
                }
                if (!Occluded(sceneX, y1, sceneZ)) {
                    return true;
                }
                return !Occluded(sceneX + 128, y1, sceneZ);
            }
        }

        if (!Occluded(sceneX + 64, y2, sceneZ + 64)) {
            return true;
        }

        if (type == 16) {
            return !Occluded(sceneX, y1, sceneZ + 128);
        } else if (type == 32) {
            return !Occluded(sceneX + 128, y1, sceneZ + 128);
        } else if (type == 64) {
            return !Occluded(sceneX + 128, y1, sceneZ);
        } else if (type == 128) {
            return !Occluded(sceneX, y1, sceneZ);
        } else {
            LOG_WARN("Warning unsupported wall type");
            return false;
        }
    }

    bool Scene::Visible(int32_t level, int32_t tileX, int32_t tileZ, int32_t y)
    {
        if (TileVisible(level, tileX, tileZ)) {
            return true;
        }
        int32_t x = tileX << 7;
        int32_t z = tileZ << 7;
        return !Occluded(x + 1, levelHeightmaps[level][tileX][tileZ] - y, z + 1) || !Occluded((x + 128) - 1, levelHeightmaps[level][tileX + 1][tileZ] - y, z + 1) || !Occluded((x + 128) - 1, levelHeightmaps[level][tileX + 1][tileZ + 1] - y, (z + 128) - 1) || !Occluded(x + 1, levelHeightmaps[level][tileX][tileZ + 1] - y, (z + 128) - 1);
    }

    void Scene::DrawWallDecor(int32_t allowWallTypes, SceneWallDecoration& decor, bool front) const
    {
        if ((decor.type & allowWallTypes) != 0) {
            decor.entity->Draw(decor.rotation, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, decor.x - eyeX, decor.y - eyeY, decor.z - eyeZ, decor.bitset);
        } else if ((decor.type & 0x300) != 0) {
            int32_t x = decor.x - eyeX;
            int32_t y = decor.y - eyeY;
            int32_t z = decor.z - eyeZ;
            int32_t rotation = decor.rotation;

            int32_t nearestX;
            int32_t nearestZ;

            if ((rotation == 1) || (rotation == 2)) {
                nearestX = -x;
            } else {
                nearestX = x;
            }

            if ((rotation == 2) || (rotation == 3)) {
                nearestZ = -z;
            } else {
                nearestZ = z;
            }

            if ((decor.type & 0x100) != 0) {
                bool draw = false;

                if (front && (nearestZ < nearestX)) {
                    draw = true;
                } else if (!front && (nearestZ >= nearestX)) {
                    draw = true;
                }

                if (draw) {
                    int32_t drawX = x + WALL_DECORATION_INSET_X[rotation];
                    int32_t drawZ = z + WALL_DECORATION_INSET_Z[rotation];
                    decor.entity->Draw((rotation * 512) + 256, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, drawX, y, drawZ, decor.bitset);
                }
            }

            if ((decor.type & 0x200) != 0) {
                bool draw = false;

                if (front && (nearestZ > nearestX)) {
                    draw = true;
                } else if (!front && (nearestZ <= nearestX)) {
                    draw = true;
                }

                if (draw) {
                    int32_t drawX = x + WALL_DECORATION_OUTSET_X[rotation];
                    int32_t drawZ = z + WALL_DECORATION_OUTSET_Z[rotation];
                    decor.entity->Draw(((rotation * 512) + 1280) & 0x7ff, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, drawX, y, drawZ, decor.bitset);
                }
            }
        }
    }

    void Scene::DrawObjStack(std::shared_ptr<SceneObjStack> stack, int32_t offset) const
    {
        if (stack->bottomObj != nullptr) {
            stack->bottomObj->Draw(0, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, stack->x - eyeX,
                stack->y - eyeY - offset, stack->z - eyeZ, stack->bitset);
        }
        if (stack->middleObj != nullptr) {
            stack->middleObj->Draw(0, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, stack->x - eyeX,
                stack->y - eyeY - offset, stack->z - eyeZ, stack->bitset);
        }
        if (stack->topObj != nullptr) {
            stack->topObj->Draw(0, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, stack->x - eyeX,
                stack->y - eyeY - offset, stack->z - eyeZ, stack->bitset); // Gets called
        }
    }

    void Scene::SetBridge(int32_t stx, int32_t stz)
    {
        // Store a COPY of ground before we modify the array
        SceneTile groundCopy = levelTiles[0][stx][stz];

        for (int32_t level = 0; level < 3; level++) {
            levelTiles[level][stx][stz] = levelTiles[level + 1][stx][stz];
            auto& above = levelTiles[level][stx][stz];

            if (!above.initialized) {
                continue;
            }

            above.level--;

            for (int32_t i = 0; i < above.locCount; i++) {
                auto& loc = above.locs[i];

                if ((((loc->bitset >> 29) & 3) == 2) && (loc->minSceneTileX == stx) && (loc->minSceneTileZ == stz)) {
                    loc->level--;
                }
            }
        }

        if (!levelTiles[0][stx][stz].initialized) {
            levelTiles[0][stx][stz] = SceneTile(0, stx, stz);
        }

        levelTiles[0][stx][stz].bridge = std::make_shared<SceneTile>(groundCopy);
        levelTiles[3][stx][stz] = SceneTile();
    }

    void Scene::DrawBridgeTile(int32_t tileX, int32_t tileZ, SceneTile& bridge)
    {

        DrawTileUnderlayOrOverlay(bridge, tileX, tileZ, 0);

        auto& wall = bridge.wall;

        if (wall != nullptr) {
            wall->entityA->Draw(0, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, wall->x - eyeX, wall->y - eyeY, wall->z - eyeZ, wall->bitset);
        }

        for (int32_t i = 0; i < bridge.locCount; i++) {
            auto& loc = bridge.locs[i];
            if (loc != nullptr) {
                loc->entity->Draw(loc->yaw, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, loc->x - eyeX, loc->y - eyeY, loc->z - eyeZ, loc->bitset);
            }
        }
    }

    void Scene::DrawMinimapTile(std::vector<int32_t>& dst, int32_t offset, int32_t step, int32_t level, int32_t x, int32_t z)
    {
        const auto& tile = levelTiles[level][x][z];

        if (!tile.initialized) {
            return;
        }

        const auto& underlay = tile.underlay;
        if (underlay.initialized) {
            int32_t rgb = underlay.rgb;

            if (rgb == 0) {
                return;
            }

            // Add opaque alpha for ARGB8888 format
            int32_t argb = 0xFF000000 | rgb;
            for (int32_t k1 = 0; k1 < 4; k1++) {
                dst[offset] = argb;
                dst[offset + 1] = argb;
                dst[offset + 2] = argb;
                dst[offset + 3] = argb;
                offset += step;
            }
            return;
        }

        const auto& overlay = tile.overlay;

        if (!overlay.initialized) {
            return;
        }

        int32_t shape = overlay.shape;
        int32_t angle = overlay.rotation;
        // Add opaque alpha for ARGB8888 format
        int32_t background = (overlay.backgroundRGB != 0) ? (0xFF000000 | overlay.backgroundRGB) : 0;
        int32_t foreground = 0xFF000000 | overlay.foregroundRGB;
        const int32_t* mask = MINIMAP_TILE_MASK[shape];
        const int32_t* rotation = MINIMAP_TILE_ROTATION_MAP[angle];
        int32_t off = 0;

        if (background != 0) {
            for (int32_t i = 0; i < 4; i++) {
                dst[offset] = (mask[rotation[off++]] != 0) ? foreground : background;
                dst[offset + 1] = (mask[rotation[off++]] != 0) ? foreground : background;
                dst[offset + 2] = (mask[rotation[off++]] != 0) ? foreground : background;
                dst[offset + 3] = (mask[rotation[off++]] != 0) ? foreground : background;
                offset += step;
            }
            return;
        }

        for (int32_t i = 0; i < 4; i++) {
            if (mask[rotation[off++]] != 0) {
                dst[offset] = foreground;
            }
            if (mask[rotation[off++]] != 0) {
                dst[offset + 1] = foreground;
            }
            if (mask[rotation[off++]] != 0) {
                dst[offset + 2] = foreground;
            }
            if (mask[rotation[off++]] != 0) {
                dst[offset + 3] = foreground;
            }
            offset += step;
        }
    }

    void Scene::Unload()
    {
        locBuffer.clear();
        levelOccluderCount.clear();
        levelOccluders.fill();
        drawTileQueue.clear();
        visibilityMatrix.fill();
        //visibilityMap.fill();
    }
}
