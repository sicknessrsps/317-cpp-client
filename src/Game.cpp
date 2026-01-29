#include "Game.h"

#include <iostream>

#include "Draw3D.h"
#include "Signlink.h"
#include "Image24.h"
#include "StringUtil.h"
#include "Model.h"
#include "FloType.h"
#include "LocEntity.h"
#include "ScopedTimer.h"
#include "LocType.h"
#include "VarbitType.h"
#include "Packet.h"
#include "IdkType.h"
#include "SeqType.h"
#include "SeqTransform.h"
#include "ChatCompression.h"
#include "SpotAnimType.h"
#include "ProjectileEntity.h"
#include "NPCType.h"
#include "VarpType.h"
#include "SoundTrack.h"

namespace SDL_Client
{
    Game::Game(const GameSpecification& specification)
        :   GameShell(specification), flameLineOffset(256)
    {
        lowmem = specification.lowmem;
        server = specification.serverAddress;
        port = specification.serverPort;
        viewportWidth = 512;
        viewportHeight = 334;
        enableRSA = specification.enableRSA;
    }

    void Game::Update()
    {
        if (errorStarted || errorLoading || errorHost) {
            return;
        }
        loopCycle++;
        if (!ingame) {
            UpdateTitle();
        } else {
            UpdateGame();
        }
        HandleOnDemandRequests();
    }

    void Game::Draw()
    {
        if (errorStarted || errorLoading || errorHost) {
            DrawError();
            return;
        }

        if (!ingame) {
            DrawTitleScreen(hideLoginButtons);
        } else {
            DrawGame();
        }

        dragCycles = 0;
    }

    void Game::Load()
    {
        DrawProgress(20, "Starting up");

        if (started) {
            errorStarted = true;
            return;
        }

        started = true;

        if (!Signlink::cacheDatPath.empty()) {
            for (int32_t i = 0; i < 5; i++) {
                filestores[i] = std::make_shared<FileStore>(500000, Signlink::cacheDatPath, Signlink::cacheIdxPath[i], i + 1);
            }
        }

        if (loadArchiveChecksums)
        {
            LoadArchiveChecksums();
        }

        archiveTitle = LoadArchive(1, "title screen", "title", archiveChecksum[1], 25);

        if (archiveTitle == nullptr) {
            errorLoading = true;
            return;
        }

        fontPlain11 = std::make_shared<BitmapFont>(*archiveTitle, "p11_full", false);
        fontPlain12 = std::make_shared<BitmapFont>(*archiveTitle, "p12_full", false);
        fontBold12 = std::make_shared<BitmapFont>(*archiveTitle, "b12_full", false);
        fontQuill8 = std::make_shared<BitmapFont>(*archiveTitle, "q8_full", true);

        // Signal that title resources (fonts) are ready for main thread to use
        SDL_SetAtomicInt(&titleResourcesReady, 1);

        // Note: LoadTitleBackground() and LoadTitleImages() are called by LoadTitle()
        // on the main thread in DrawLoadingProgress() to avoid race conditions

        archiveConfig = LoadArchive(2, "config", "config", archiveChecksum[2], 30);
        archiveInterface = LoadArchive(3, "interface", "interface", archiveChecksum[3], 35);
        archiveMedia = LoadArchive(4, "2d graphics", "media", archiveChecksum[4], 40);
        archiveTextures = LoadArchive(6, "textures", "textures", archiveChecksum[6], 45);
        archiveWordenc = LoadArchive(7, "chat system", "wordenc", archiveChecksum[7], 50);
        archiveSounds = LoadArchive(8, "sound effects", "sounds", archiveChecksum[8], 55);

        levelTileFlags.resize(4, 104, 104);
        levelHeightmap.resize(4, 105, 105);
        scene = std::make_unique<Scene>(104, 104, levelHeightmap, 4);
        for (int32_t level = 0; level < 4; level++)
        {
            levelCollisionMap[level] = std::make_shared<CollisionMap>(104, 104);
        }

        imageMinimap = std::make_unique<Image24>(512, 512);

        archiveVersionlist = LoadArchive(5, "update list", "versionlist", archiveChecksum[4], 60);
        DrawProgress(60, "Connecting to update server");

        ondemand = std::make_unique<OnDemand>();
        ondemand->Load(*archiveVersionlist, this);

        SeqTransform::Init(ondemand->GetSeqFrameCount());
        Model::Init(ondemand->GetFileCount(0), ondemand.get());

        // Initialize AudioManager early so MIDI can play during loading
        if (!lowmem) {
            audioManager = std::make_unique<AudioManager>();
            if (!audioManager->Init()) {
                LOG_ERROR("Failed to initialize audio system");
            }
        }

        if (!lowmem && midiEnabled) {
            song = 0;
            songFading = true;
            ondemand->Request(2, song);

            while (ondemand->Remaining() > 0) {
                HandleOnDemandRequests();
                SDL_Delay(100);
                if (ondemand->failCount > 3) {
                    LOG_ERROR("ondemand fail during music load");
                    return;
                }
            }
        }

        DrawProgress(65, "Requesting animations");

        int32_t total = ondemand->GetFileCount(1);

        for (int32_t i = 0; i < total; i++) {
            if (ondemand->animIndex[i] != 0) {
                ondemand->Request(1, i);
            }
        }
        while (ondemand->Remaining() > 0) {
            int32_t done = total - ondemand->Remaining();
            DrawProgress(65, "Loading animations - " + std::to_string(done * 100 / total) + "%");
            HandleOnDemandRequests();
            SDL_Delay(10);

            if (ondemand->failCount > 3) {
                LOG_ERROR("ondemand fail during animations load");
                return;
            }
        }

        DrawProgress(70, "Requesting models");

        total = ondemand->GetFileCount(0);

        for (int32_t i = 0; i < total; i++) {
            if ((ondemand->GetModelFlags(i) & 1) != 0) {
                ondemand->Request(0, i);
            }
        }

        total = ondemand->Remaining();

        while (ondemand->Remaining() > 0) {
            int32_t done = total - ondemand->Remaining();
            DrawProgress(70, "Loading models - " + std::to_string(done * 100 / total) + "%");
            HandleOnDemandRequests();
            SDL_Delay(10);
        }

        if (filestores[0] != nullptr)
        {
            DrawProgress(75, "Requesting maps");
            ondemand->Request(3, ondemand->GetMapFile(0, 47, 48));
            ondemand->Request(3, ondemand->GetMapFile(1, 47, 48));
            ondemand->Request(3, ondemand->GetMapFile(0, 48, 48));
            ondemand->Request(3, ondemand->GetMapFile(1, 48, 48));
            ondemand->Request(3, ondemand->GetMapFile(0, 49, 48));
            ondemand->Request(3, ondemand->GetMapFile(1, 49, 48));
            ondemand->Request(3, ondemand->GetMapFile(0, 47, 47));
            ondemand->Request(3, ondemand->GetMapFile(1, 47, 47));
            ondemand->Request(3, ondemand->GetMapFile(0, 48, 47));
            ondemand->Request(3, ondemand->GetMapFile(1, 48, 47));
            ondemand->Request(3, ondemand->GetMapFile(0, 48, 148));
            ondemand->Request(3, ondemand->GetMapFile(1, 48, 148));

            total = ondemand->Remaining();

            while (ondemand->Remaining() > 0) {
                int32_t done = total - ondemand->Remaining();
                DrawProgress(75, "Loading maps - " + std::to_string(done * 100 / total) + "%");
                HandleOnDemandRequests();
                SDL_Delay(10);
            }
        }

        total = ondemand->GetFileCount(0);

        for (int32_t modelID = 0; modelID < total; modelID++) {
            int32_t flags = ondemand->GetModelFlags(modelID);
            int8_t priority = 0;
            if ((flags & 8) != 0) {
                priority = 10;
            } else if ((flags & 0x20) != 0) {
                priority = 9;
            } else if ((flags & 0x10) != 0) {
                priority = 8;
            } else if ((flags & 0x40) != 0) {
                priority = 7;
            } else if ((flags & 0x80) != 0) {
                priority = 6;
            } else if ((flags & 2) != 0) {
                priority = 5;
            } else if ((flags & 4) != 0) {
                priority = 4;
            }

            if ((flags & 1) != 0) {
                priority = 3;
            }

            if (priority != 0) {
                ondemand->Prefetch(priority, 0, modelID);
            }
        }

        ondemand->PrefetchMaps(members);

        if (!lowmem) {
            int32_t midiCount = ondemand->GetFileCount(2);
            for (int32_t midi = 1; midi < midiCount; midi++) {
                if (ondemand->HasMidi(midi)) {
                    ondemand->Prefetch(1, 2, midi);
                }
            }
        }

        DrawProgress(80, "Unpacking media");
        imageInvback = std::make_unique<Image8>(*archiveMedia, "invback", 0);
        imageChatback = std::make_unique<Image8>(*archiveMedia, "chatback", 0);
        imageMapback = std::make_unique<Image8>(*archiveMedia, "mapback", 0);
        imageBackbase1 = std::make_unique<Image8>(*archiveMedia, "backbase1", 0);
        imageBackbase2 = std::make_unique<Image8>(*archiveMedia, "backbase2", 0);
        imageBackhmid1 = std::make_unique<Image8>(*archiveMedia, "backhmid1", 0);
        for (int32_t i = 0; i < 13; i++) {
            imageSideicons[i] = std::make_unique<Image8>(*archiveMedia, "sideicons", i);
        }

        imageCompass = std::make_unique<Image24>(*archiveMedia, "compass", 0);
        imageMapedge = std::make_unique<Image24>(*archiveMedia, "mapedge", 0);
        imageMapedge->Crop();
        int32_t mapsceneCount = std::min(100, Image8::Count(*archiveMedia, "mapscene"));
        for (int32_t i = 0; i < mapsceneCount; i++) {
            imageMapscene[i] = std::make_unique<Image8>(*archiveMedia, "mapscene", i);
        }
        int32_t mapfunctionCount = std::min(100, Image24::Count(*archiveMedia, "mapfunction"));
        for (int32_t i = 0; i < mapfunctionCount; i++) {
            imageMapfunction[i] = std::make_unique<Image24>(*archiveMedia, "mapfunction", i);
        }
        int32_t hitmarkCount = std::min(20, Image24::Count(*archiveMedia, "hitmarks"));
        for (int32_t i = 0; i < hitmarkCount; i++) {
            imageHitmarks[i] = std::make_unique<Image24>(*archiveMedia, "hitmarks", i);
        }
        int32_t headiconCount = std::min(20, Image24::Count(*archiveMedia, "headicons_prayer")); // You may need to update this (FileArchive: Not found!)
        for (int32_t i = 0; i < headiconCount; i++) {
            imageHeadicons[i] = std::make_unique<Image24>(*archiveMedia, "headicons_prayer", i);
        }

        imageMapmarker0 = std::make_unique<Image24>(*archiveMedia, "mapmarker", 0);
        imageMapmarker1 = std::make_unique<Image24>(*archiveMedia, "mapmarker", 1);
        for (int32_t i = 0; i < 8; i++) {
            imageCrosses[i] = std::make_unique<Image24>(*archiveMedia, "cross", i);
        }
        imageMapdot0 =  std::make_unique<Image24>(*archiveMedia, "mapdots", 0);
        imageMapdot1 =  std::make_unique<Image24>(*archiveMedia, "mapdots", 1);
        imageMapdot2 =  std::make_unique<Image24>(*archiveMedia, "mapdots", 2);
        imageMapdot3 =  std::make_unique<Image24>(*archiveMedia, "mapdots", 3);
        imageMapdot4 =  std::make_unique<Image24>(*archiveMedia, "mapdots", 4);
        imageScrollbar0 = std::make_unique<Image8>(*archiveMedia, "scrollbar", 0);
        imageScrollbar1 = std::make_unique<Image8>(*archiveMedia, "scrollbar", 1);
        imageRedstone1 =  std::make_unique<Image8>(*archiveMedia,"redstone1", 0);
        imageRedstone2 = std::make_unique<Image8>(*archiveMedia, "redstone2", 0);
        imageRedstone3 = std::make_unique<Image8>(*archiveMedia, "redstone3", 0);
        imageRedstone1h = std::make_unique<Image8>(*archiveMedia, "redstone1", 0);
        imageRedstone1h->FlipHorizontally();
        imageRedstone2h = std::make_unique<Image8>(*archiveMedia, "redstone2", 0);
        imageRedstone2h->FlipHorizontally();
        imageRedstone1v = std::make_unique<Image8>(*archiveMedia, "redstone1", 0);
        imageRedstone1v->FlipVertically();
        imageRedstone2v = std::make_unique<Image8>(*archiveMedia, "redstone2", 0);
        imageRedstone2v->FlipVertically();
        imageRedstone3v = std::make_unique<Image8>(*archiveMedia, "redstone3", 0);
        imageRedstone3v->FlipVertically();
        imageRedstone1hv = std::make_unique<Image8>(*archiveMedia, "redstone1", 0);
        imageRedstone1hv->FlipHorizontally();
        imageRedstone1hv->FlipVertically();
        imageRedstone2hv = std::make_unique<Image8>(*archiveMedia, "redstone2", 0);
        imageRedstone2hv->FlipHorizontally();
        imageRedstone2hv->FlipVertically();
        for (int32_t i = 0; i < 2; i++) {
            imageModIcons[i] =  std::make_unique<Image8>(*archiveMedia, "mod_icons", i);
        }
        areaBackleft1 = std::make_unique<DrawArea>(Image24(*archiveMedia, "backleft1", 0));
        areaBackleft2 = std::make_unique<DrawArea>(Image24(*archiveMedia, "backleft2", 0));
        areaBackright1 = std::make_unique<DrawArea>(Image24(*archiveMedia, "backright1", 0));
        areaBackright2 = std::make_unique<DrawArea>(Image24(*archiveMedia, "backright2", 0));
        areaBacktop1 = std::make_unique<DrawArea>(Image24(*archiveMedia, "backtop1", 0));
        areaBackvmid1 = std::make_unique<DrawArea>(Image24(*archiveMedia, "backvmid1", 0));
        areaBackvmid2 = std::make_unique<DrawArea>(Image24(*archiveMedia, "backvmid2", 0));
        areaBackvmid3 = std::make_unique<DrawArea>(Image24(*archiveMedia, "backvmid3", 0));
        areaBackhmid2 = std::make_unique<DrawArea>(Image24(*archiveMedia, "backhmid2", 0));

        int32_t red = SDL_rand(21) - 10;
        int32_t green = SDL_rand(21) - 10;
        int32_t blue = SDL_rand(21) - 10;
        int32_t value = SDL_rand(41) - 20;

        for (int32_t i = 0; i < 100; i++) {
            if (imageMapfunction[i] != nullptr) {
                imageMapfunction[i]->Translate(red + value, green + value, blue + value);
            }
            if (imageMapscene[i] != nullptr) {
                imageMapscene[i]->Translate(red + value, green + value, blue + value);
            }
        }

        DrawProgress(83, "Unpacking textures");
        Draw3D::UnpackTextures(*archiveTextures);
        Draw3D::SetBrightness(0.8);
        Draw3D::InitPool(20);

        DrawProgress(86, "Unpacking config");
        SeqType::Unpack(*archiveConfig);
        LocType::Unpack(*archiveConfig);
        FloType::Unpack(*archiveConfig);
        ObjType::Unpack(*archiveConfig);
        NPCType::Unpack(*archiveConfig);
        IdkType::Unpack(*archiveConfig);
        SpotAnimType::Unpack(*archiveConfig);
        VarpType::Unpack(*archiveConfig);
        VarbitType::Unpack(*archiveConfig);

        if (!lowmem) {
            DrawProgress(90, "Unpacking sounds");
            Buffer soundsBuffer(archiveSounds->Read("sounds.dat"));
            SoundTrack::Unpack(soundsBuffer);
        }

        DrawProgress(95, "Unpacking interfaces");
        IfType::Unpack(*archiveInterface, {fontPlain11, fontPlain12, fontBold12, fontQuill8}, *archiveMedia);

        DrawProgress(100, "Preparing game engine");
        for (int32_t y = 0; y < 33; y++) {
            int32_t left = 999;
            int32_t right = 0;
            for (int32_t x = 0; x < 34; x++) {
                if (imageMapback->pixels[x + (y * imageMapback->width)] == 0) {
                    if (left == 999) {
                        left = x;
                    }
                    continue;
                }
                if (left == 999) {
                    continue;
                }
                right = x;
                break;
            }
            compassMaskLineOffsets[y] = left;
            compassMaskLineLengths[y] = right - left;
        }

        for (int32_t y = 5; y < 156; y++) {
            int32_t left = 999;
            int32_t right = 0;
            for (int32_t x = 25; x < 172; x++) {
                if ((imageMapback->pixels[x + (y * imageMapback->width)] == 0) && ((x > 34) || (y > 34))) {
                    if (left == 999) {
                        left = x;
                    }
                    continue;
                }
                if (left == 999) {
                    continue;
                }
                right = x;
                break;
            }
            minimapMaskLineOffsets[y - 5] = left - 25;
            minimapMaskLineLengths[y - 5] = right - left;
        }

        Draw3D::Init3D(479, 96);
        areaChatbackOffsets = Draw3D::lineOffset;

        Draw3D::Init3D(190, 261);
        areaSidebarOffsets = Draw3D::lineOffset;

        Draw3D::Init3D(viewportWidth, viewportHeight);
        areaViewportOffsets = Draw3D::lineOffset;

        Scene::Init(viewportWidth, viewportHeight);

        LocEntity::game = this;
        LocType::game = this;
        NPCType::game = this;
    }

    std::unique_ptr<FileArchive> Game::LoadArchive(int32_t fileId, const std::string& caption, const std::string& fileName, int32_t expectedChecksum, int32_t progress) {
        std::vector<int8_t> data;

        // Try to load from local cache first
        if (filestores[0] != nullptr) {
            data = filestores[0]->Read(fileId);
        }

        if (loadArchiveChecksums)
        {
            if (!data.empty()) {
                uint32_t crc = crc32(0L, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size()));
                if (static_cast<int32_t>(crc) != expectedChecksum) {
                    data.clear();
                }
            }
        }

        if (!data.empty()) {
            return std::make_unique<FileArchive>(data);
        }

        // Download via JAGGRAB if not in cache
        DrawProgress(progress, "Requesting " + caption);

        auto jaggrabIn = OpenURL(fileName + std::to_string(expectedChecksum));
        if (jaggrabIn == nullptr) {
            LOG_ERROR("JAGGRAB %s: failed to connect", fileName.c_str());
            return nullptr;
        }

        // Read 6-byte header
        std::vector<int8_t> headerBuffer(6);
        jaggrabIn->Read(headerBuffer, 0, 6);

        if (jaggrabIn->Closed()) {
            LOG_ERROR("JAGGRAB %s: connection closed while reading header", fileName.c_str());
            return nullptr;
        }

        // Parse packed size from header (bytes 3-5)
        int32_t packedSize = ((headerBuffer[3] & 0xff) << 16) |
                             ((headerBuffer[4] & 0xff) << 8) |
                             (headerBuffer[5] & 0xff);

        if (packedSize <= 0 || packedSize > 10000000) {
            LOG_ERROR("JAGGRAB %s: invalid packed size %d", fileName.c_str(), packedSize);
            jaggrabIn->Close();
            return nullptr;
        }

        int32_t fileSize = packedSize + 6;  // Packed size + header
        int32_t totalRead = 6;
        data.resize(fileSize);

        // Copy header to data
        std::copy(headerBuffer.begin(), headerBuffer.end(), data.begin());

        // Read rest of file
        int32_t bytesRead = jaggrabIn->Read(data, 6, packedSize);
        jaggrabIn->Close();

        if (bytesRead != packedSize) {
            LOG_ERROR("JAGGRAB %s: incomplete read %d/%d bytes", fileName.c_str(), bytesRead, packedSize);
            return nullptr;
        }

        // Save to cache
        if (filestores[0] != nullptr) {
            filestores[0]->Write(data, fileId, static_cast<int32_t>(data.size()));
        }

        return std::make_unique<FileArchive>(data);
    }

    void Game::LoadArchiveChecksums()
    {
        int32_t wait = 5;
        int32_t retries = 0;

        archiveChecksum[8] = 0;

        while (archiveChecksum[8] == 0) {
            std::string errorReason = "Unknown problem";
            DrawProgress(20, "Connecting to web server");

            // Generate random cache-busting URL
            std::string url = "crc" + std::to_string(SDL_rand(99999999)) + "-317";

            auto crcConnection = OpenURL(url);
            if (crcConnection == nullptr) {
                errorReason = "connection problem";
                LOG_WARN("LoadArchiveChecksums: failed to connect");
            } else {
                std::vector<int8_t> crcData(40);
                int32_t bytesRead = crcConnection->Read(crcData, 0, 40);
                crcConnection->Close();

                if (bytesRead < 40) {
                    errorReason = "EOF problem";
                    LOG_WARN("LoadArchiveChecksums: incomplete read %d/40 bytes", bytesRead);
                } else {
                    Buffer buffer(crcData);

                    for (int32_t i = 0; i < 9; i++) {
                        archiveChecksum[i] = buffer.Read32();
                    }

                    int32_t expectedChecksum = buffer.Read32();
                    int32_t calculatedChecksum = 1234;

                    for (int32_t i = 0; i < 9; i++) {
                        calculatedChecksum = (calculatedChecksum << 1) + archiveChecksum[i];
                    }

                    if (expectedChecksum != calculatedChecksum) {
                        errorReason = "checksum problem";
                        LOG_WARN("LoadArchiveChecksums: checksum mismatch (expected %d, got %d)", expectedChecksum, calculatedChecksum);
                        archiveChecksum[8] = 0;
                    }
                }
            }

            if (archiveChecksum[8] == 0) {
                retries++;

                for (int32_t remaining = wait; remaining > 0; remaining--) {
                    if (retries >= 10) {
                        DrawProgress(10, "Game updated - please reload page");
                        remaining = 10;
                    } else {
                        DrawProgress(10, errorReason + " - Will retry in " + std::to_string(remaining) + " secs.");
                    }

                    SDL_Delay(1000);
                }

                wait *= 2;
                if (wait > 60) {
                    wait = 60;
                }

                jaggrabEnabled = !jaggrabEnabled;
            }
        }
    }

    /**
     * Prepares all the title screen (flames, background, buttons, etc.) and unloads the game screen
     * components if the title screen hasn't been prepared already.
     *
     */
    void Game::LoadTitle()
    {
        if (imageTitle2 != nullptr)
        {
            return;
        }
        areaChatback = nullptr;
        areaMapback = nullptr;
        areaSidebar = nullptr;
        areaViewport = nullptr;
        areaBackbase1 = nullptr;
        areaBackbase2 = nullptr;
        areaBackhmid1 = nullptr;
        imageTitle0 = std::make_unique<DrawArea>(128, 265);
        Draw2D::Clear();
        imageTitle1 = std::make_unique<DrawArea>(128, 265);
        Draw2D::Clear();
        imageTitle2 = std::make_unique<DrawArea>(509, 171);
        Draw2D::Clear();
        imageTitle3 = std::make_unique<DrawArea>(360, 132);
        Draw2D::Clear();
        imageTitle4 = std::make_unique<DrawArea>(360, 200);
        Draw2D::Clear();
        imageTitle5 = std::make_unique<DrawArea>(202, 238);
        Draw2D::Clear();
        imageTitle6 = std::make_unique<DrawArea>(203, 238);
        Draw2D::Clear();
        imageTitle7 = std::make_unique<DrawArea>(74, 94);
        Draw2D::Clear();
        imageTitle8 = std::make_unique<DrawArea>(75, 94);
        Draw2D::Clear();
        if (archiveTitle != nullptr) {
            LoadTitleBackground();
            LoadTitleImages();
        }
        redrawTitleBackground = true;

    }

    void Game::LoadTitleImages() {
        imageTitlebox = std::make_unique<Image8>(*archiveTitle, "titlebox", 0);
        imageTitlebutton = std::make_unique<Image8>(*archiveTitle, "titlebutton", 0);

        for (int32_t i = 0; i < 12; i++) {
            imageRunes[i] = std::make_unique<Image8>(*archiveTitle, "runes", i);
        }

        imageFlamesLeft = std::make_unique<Image24>(128, 265);
        imageFlamesRight = std::make_unique<Image24>(128, 265);

        memcpy(imageFlamesLeft->surface->pixels, imageTitle0->image->pixels, 33920 * sizeof(int32_t));
        memcpy(imageFlamesRight->surface->pixels, imageTitle1->image->pixels, 33920 * sizeof(int32_t));

        for (int32_t i = 0; i < 64; i++) {
            flameGradient0[i] = i * 0x40000;
        }
        for (int32_t i = 0; i < 64; i++) {
            flameGradient0[i + 64] = 0xff0000 + (0x400 * i);
        }
        for (int32_t i = 0; i < 64; i++) {
            flameGradient0[i + 128] = 0xffff00 + (0x4 * i);
        }
        for (int32_t i = 0; i < 64; i++) {
            flameGradient0[i + 192] = 0xffffff;
        }

        for (int32_t i = 0; i < 64; i++) {
            flameGradient1[i] = i * 1024;
        }
        for (int32_t i = 0; i < 64; i++) {
            flameGradient1[i + 64] = 65280 + (4 * i);
        }
        for (int32_t i = 0; i < 64; i++) {
            flameGradient1[i + 128] = 65535 + (0x40000 * i);
        }
        for (int32_t i = 0; i < 64; i++) {
            flameGradient1[i + 192] = 0xffffff;
        }

        for (int32_t k3 = 0; k3 < 64; k3++) {
            flameGradient2[k3] = k3 * 4;
        }
        for (int32_t l3 = 0; l3 < 64; l3++) {
            flameGradient2[l3 + 64] = 255 + (0x40000 * l3);
        }
        for (int32_t i4 = 0; i4 < 64; i4++) {
            flameGradient2[i4 + 128] = 0xff00ff + (1024 * i4);
        }
        for (int32_t j4 = 0; j4 < 64; j4++) {
            flameGradient2[j4 + 192] = 0xffffff;
        }

        UpdateFlameBuffer(nullptr);

        DrawProgress(10, "Connecting to fileserver");
        SDL_SetAtomicInt(&flameActive, 1);
    }

    void Game::LoadTitleBackground() const {
        Image24 image(archiveTitle->Read("title.dat"), window);

        imageTitle0->Bind();
        image.BlitOpaque(0, 0);

        imageTitle1->Bind();
        image.BlitOpaque(-637, 0);

        imageTitle2->Bind();
        image.BlitOpaque(-128, 0);

        imageTitle3->Bind();
        image.BlitOpaque(-202, -371);

        imageTitle4->Bind();
        image.BlitOpaque(-202, -171);

        imageTitle5->Bind();
        image.BlitOpaque(0, -265);

        imageTitle6->Bind();
        image.BlitOpaque(-562, -265);

        imageTitle7->Bind();
        image.BlitOpaque(-128, -171);

        imageTitle8->Bind();
        image.BlitOpaque(-562, -171);

        image.FlipHorizontal();

        imageTitle0->Bind();
        image.BlitOpaque(382, 0);

        imageTitle1->Bind();
        image.BlitOpaque(-255, 0);

        imageTitle2->Bind();
        image.BlitOpaque(254, 0);

        imageTitle3->Bind();
        image.BlitOpaque(180, -371);

        imageTitle4->Bind();
        image.BlitOpaque(180, -171);

        imageTitle5->Bind();
        image.BlitOpaque(382, -265);

        imageTitle6->Bind();
        image.BlitOpaque(-180, -265);

        imageTitle7->Bind();
        image.BlitOpaque(254, -171);

        imageTitle8->Bind();
        image.BlitOpaque(-180, -171);

        image = Image24(*archiveTitle, "logo", 0);
        imageTitle2->Bind();
        image.Draw(382 - (image.width / 2) - 128, 18);

    }

    void Game::StopFlames() {
        SDL_SetAtomicInt(&flameActive, 0);
    }

    void Game::UpdateFlames() {

        constexpr int32_t height = 256;

        // add fuel to the bottom
        for (int32_t x = 10; x < 117; x++) {
            if (SDL_rand(100) < 50) {
                flameBuffer3[x + ((height - 2) << 7)] = 255;
            }
        }

        // add sparkles of fuel everywhere
        for (int32_t l = 0; l < 100; l++) {
            const int32_t x = SDL_rand(124) + 2;
            const int32_t y = SDL_rand(128) + 128;
            flameBuffer3[x + (y << 7)] = 192;
        }

        // blur that fuel
        for (int32_t y = 1; y < (height - 1); y++) {
            for (int32_t x = 1; x < 127; x++) {
                int32_t pos = x + (y << 7);
                flameBuffer2[pos] = (flameBuffer3[pos - 1] + flameBuffer3[pos + 1] + flameBuffer3[pos - 128] + flameBuffer3[pos + 128]) / 4;
            }
        }

        flameCycle0 += 128;


        if (flameCycle0 > flameBuffer0.size()) {
            flameCycle0 -= flameBuffer0.size();
            auto idx = SDL_rand(12);
            UpdateFlameBuffer(imageRunes[idx]);
        }

        // flamebuffer0 is being used to dilute the fuel
        for (int32_t y = 1; y < (height - 1); y++) {
            for (int32_t x = 1; x < 127; x++) {
                int32_t pos = x + (y << 7);
                int32_t intensity = flameBuffer2[pos + 128] - (flameBuffer0[(pos + flameCycle0) & (flameBuffer0.size() - 1)] / 5);
                if (intensity < 0) {
                    intensity = 0;
                }

                flameBuffer3[pos] = intensity;
            }
        }

        for (int32_t y = 0; y < (height - 1); y++) {
            flameLineOffset[y] = flameLineOffset[y + 1];
        }

        flameLineOffset[height - 1] = static_cast<int32_t>(
            (std::sin(static_cast<double>(loopCycle) / 14.0) * 16.0) +
            (std::sin(static_cast<double>(loopCycle) / 15.0) * 14.0) +
            (std::sin(static_cast<double>(loopCycle) / 16.0) * 12.0));

        if (flameGradientCycle0 > 0) {
            flameGradientCycle0 -= 4;
        }

        if (flameGradientCycle1 > 0) {
            flameGradientCycle1 -= 4;
        }

        if ((flameGradientCycle0 == 0) && (flameGradientCycle1 == 0)) {
            auto rng = SDL_rand(2001);
            if (rng == 0) {
                flameGradientCycle0 = 1024;
            } else if (rng == 1) {
                flameGradientCycle1 = 1024;
            }
        }
    }

    void Game::DrawFlames() {
        int32_t height = 256;

        if (flameGradientCycle0 > 0) {
            for (int32_t i = 0; i < 256; i++) {
                if (flameGradientCycle0 > 768) {
                    flameGradient[i] = Mix(flameGradient0[i], flameGradient1[i], 1024 - flameGradientCycle0);
                } else if (flameGradientCycle0 > 256) {
                    flameGradient[i] = flameGradient1[i];
                } else {
                    flameGradient[i] = Mix(flameGradient1[i], flameGradient0[i], 256 - flameGradientCycle0);
                }
            }
        } else if (flameGradientCycle1 > 0) {
            for (int32_t j = 0; j < 256; j++) {
                if (flameGradientCycle1 > 768) {
                    flameGradient[j] = Mix(flameGradient0[j], flameGradient2[j], 1024 - flameGradientCycle1);
                } else if (flameGradientCycle1 > 256) {
                    flameGradient[j] = flameGradient2[j];
                } else {
                    flameGradient[j] = Mix(flameGradient2[j], flameGradient0[j], 256 - flameGradientCycle1);
                }
            }
        } else {
            std::ranges::copy(flameGradient0, flameGradient.begin());
        }


        if (SDL_MUSTLOCK(imageFlamesLeft->surface)) SDL_LockSurface(imageFlamesLeft->surface);
        if (SDL_MUSTLOCK(imageTitle0->image)) SDL_LockSurface(imageTitle0->image);

        SDL_BlitSurface(imageFlamesLeft->surface, nullptr, imageTitle0->image, nullptr);

        if (SDL_MUSTLOCK(imageFlamesLeft->surface)) SDL_UnlockSurface(imageFlamesLeft->surface);
        if (SDL_MUSTLOCK(imageTitle0->image)) SDL_UnlockSurface(imageTitle0->image);

        int32_t srcOffset = 0;
        int32_t dstOffset = 1152;

        for (int32_t y = 1; y < (height - 1); y++) {
            const int32_t offset = (flameLineOffset[y] * (height - y)) / height;

            int32_t step = 22 + offset;

            if (step < 0) {
                step = 0;
            }

            srcOffset += step;

            for (int32_t x = step; x < 128; x++) {
                uint32_t value = flameBuffer3[srcOffset++];

                if (value != 0) {
                    const uint32_t alpha = value;
                    const uint32_t invAlpha = 256 - value;

                    value = flameGradient[value];
                    auto* pixels = static_cast<int32_t*>(imageTitle0->image->pixels);
                    const uint32_t background = pixels[dstOffset];

                    // Extract RGB components from flame value
                    const uint32_t r0 = ((value >> 16) & 0xFF) * alpha;
                    const uint32_t g0 = ((value >> 8) & 0xFF) * alpha;
                    const uint32_t b0 = (value & 0xFF) * alpha;

                    // Extract RGB components from background
                    const uint32_t r1 = ((background >> 16) & 0xFF) * invAlpha;
                    const uint32_t g1 = ((background >> 8) & 0xFF) * invAlpha;
                    const uint32_t b1 = (background & 0xFF) * invAlpha;

                    // Blend and write with full alpha
                    pixels[dstOffset++] = 0xFF000000 | (((r0 + r1) >> 8) << 16) |
                                          (((g0 + g1) >> 8) << 8) |
                                          ((b0 + b1) >> 8);
                } else {
                    dstOffset++;
                }
            }

            dstOffset += step;
        }

        imageTitle0->Draw(drawSurface, 0, 0);

        if (SDL_MUSTLOCK(imageFlamesRight->surface)) SDL_LockSurface(imageFlamesRight->surface);
        if (SDL_MUSTLOCK(imageTitle1->image)) SDL_LockSurface(imageTitle1->image);

        SDL_BlitSurface(imageFlamesRight->surface, nullptr, imageTitle1->image, nullptr);

        if (SDL_MUSTLOCK(imageFlamesRight->surface)) SDL_UnlockSurface(imageFlamesRight->surface);
        if (SDL_MUSTLOCK(imageTitle1->image)) SDL_UnlockSurface(imageTitle1->image);

        srcOffset = 0;
        dstOffset = 1176;

        for (int32_t y = 1; y < (height - 1); y++) {
            const int32_t offset = (flameLineOffset[y] * (height - y)) / height;
            const int32_t step = 103 - offset;

            dstOffset += offset;

            for (int32_t i4 = 0; i4 < step; i4++) {
                int32_t value = flameBuffer3[srcOffset++];

                if (value != 0) {
                    int32_t alpha = value;
                    int32_t invAlpha = 256 - value;
                    value = flameGradient[value];
                    auto* pixels = static_cast<int32_t*>(imageTitle1->image->pixels);
                    uint32_t background = pixels[dstOffset];

                    // Extract and blend RGB components
                    uint32_t r0 = ((value >> 16) & 0xFF) * alpha;
                    uint32_t g0 = ((value >> 8) & 0xFF) * alpha;
                    uint32_t b0 = (value & 0xFF) * alpha;

                    uint32_t r1 = ((background >> 16) & 0xFF) * invAlpha;
                    uint32_t g1 = ((background >> 8) & 0xFF) * invAlpha;
                    uint32_t b1 = (background & 0xFF) * invAlpha;

                    pixels[dstOffset++] = 0xFF000000 | (((r0 + r1) >> 8) << 16) |
                                          (((g0 + g1) >> 8) << 8) |
                                          ((b0 + b1) >> 8);
                } else {
                    dstOffset++;
                }
            }
            srcOffset += 128 - step;
            dstOffset += 128 - step - offset;
        }

        imageTitle1->Draw(drawSurface, 637, 0);
        // PresentFrame() is called by DrawTitleScreen() on the main thread
    }

    void Game::DrawGame() {

        if (redrawTitleBackground)
        {
            redrawTitleBackground = false;
            areaBackleft1->Draw(drawSurface, 0, 4);
            areaBackleft2->Draw(drawSurface, 0, 357);
            areaBackright1->Draw(drawSurface, 722, 4);
            areaBackright2->Draw(drawSurface, 743, 205);
            areaBacktop1->Draw(drawSurface, 0, 0);
            areaBackvmid1->Draw(drawSurface, 516, 4);
            areaBackvmid2->Draw(drawSurface, 516, 205);
            areaBackvmid3->Draw(drawSurface, 496, 357);
            areaBackhmid2->Draw(drawSurface, 0, 338);
            redrawSidebar = true;
            redrawChatback = true;
            redrawSideicons = true;
            redrawPrivacySettings = true;
            if (sceneState != 2)
            {
                areaViewport->Draw(drawSurface, 4, 4);
                areaMapback->Draw(drawSurface, 550, 4);
            }
            PresentFrame();
        }
        if (sceneState == 2) {
            DrawScene();
        }

        if (menuVisible && (menuArea == 1)) {
            redrawSidebar = true;
        }

        if (sidebarInterfaceID != -1) {
            if (UpdateInterfaceAnimation(delta, sidebarInterfaceID)) {
                redrawSidebar = true;
            }
        }

        if (actionArea == 2) {
            redrawSidebar = true;
        }

        if (objDragArea == 2) {
            redrawSidebar = true;
        }

        if (redrawSidebar) {
            DrawSidebar();
            redrawSidebar = false;
        }

        if (chatInterfaceID == -1) {
            chatInterface.scrollPosition = chatScrollHeight - chatScrollOffset - 77;

            if ((mouseX > 448) && (mouseX < 560) && (mouseY > 332)) {
                HandleScrollInput(463, 77, mouseX - 17, mouseY - 357, chatInterface, 0, false, chatScrollHeight);
            }

            int32_t offset = chatScrollHeight - 77 - chatInterface.scrollPosition;

            if (offset < 0) {
                offset = 0;
            }

            if (offset > (chatScrollHeight - 77)) {
                offset = chatScrollHeight - 77;
            }
            if (chatScrollOffset != offset) {
                chatScrollOffset = offset;
                redrawChatback = true;
            }
        }

        if (chatInterfaceID != -1) {
            if (UpdateInterfaceAnimation(delta, chatInterfaceID)) {
                redrawChatback = true;
            }
        }

        if (actionArea == 3) {
            redrawChatback = true;
        }

        if (objDragArea == 3) {
            redrawChatback = true;
        }

        if (!modalMessage.empty()) {
            redrawChatback = true;
        }

        if (menuVisible && (menuArea == 2)) {
            redrawChatback = true;
        }

        if (redrawChatback) {
            DrawChatback();
            redrawChatback = false;
        }

        if (sceneState == 2) {
            DrawMinimap();
            areaMapback->Draw(drawSurface, 550, 4);
        }

        if (flashingTab != -1) {
            redrawSideicons = true;
        }

        DrawSideicons();
        DrawPrivacySettings();

        delta = 0;
    }

    void Game::DrawScene() {
        sceneCycle++;
        PushPlayers(true);
        PushNPCs(true);
        PushPlayers(false);
        PushNPCs(false);
        PushProjectiles();
        PushSpotanims();

        if (!cutscene) {
            int32_t pitch = orbitCameraPitch;

            if ((cameraPitchClamp / 256) > pitch) {
                pitch = cameraPitchClamp / 256;
            }

            if (cameraModifierEnabled[4] && ((cameraModifierWobbleScale[4] + 128) > pitch)) {
                pitch = cameraModifierWobbleScale[4] + 128;
            }

            int32_t yaw = (orbitCameraYaw /*+ cameraAnticheatAngle*/) & 0x7ff;
            OrbitCamera(600 + (pitch * 3), pitch, orbitCameraX, GetHeightmapY(currentLevel, localPlayer->x, localPlayer->z) - 50, yaw, orbitCameraZ);
        }

        int32_t topLevel = 0;
        if (cutscene) {
            topLevel = GetTopLevelCutscene();
        } else {
            topLevel = GetTopLevel();
        }

        const int32_t cameraXTemp = cameraX;
        const int32_t cameraYTemp = cameraY;
        const int32_t cameraZTemp = cameraZ;
        const int32_t cameraPitchTemp = cameraPitch;
        const int32_t cameraYawTemp = cameraYaw;

        ApplyCameraAdjustments();

        int32_t cycle = Draw3D::cycle;
        Model::checkHover = true;
        Model::pickedCount = 0;
        Model::mouseX = mouseX - 4;
        Model::mouseY = mouseY - 4;
        Draw2D::Clear();

        scene->Draw(cameraX, cameraZ, cameraYaw, cameraY, topLevel, cameraPitch);
        scene->ClearTemporaryLocs();

        Draw2DEntityElements();
        DrawChats();
        DrawTileHint();
        UpdateTextures(cycle);

        DrawPrivateMessages();
        DrawMouseCrosses();
        DrawViewportInterfaces();

        UpdateChatOverride();

        if (!menuVisible) {
            HandleInput();
            DrawTooltip();
        } else if (menuArea == 0) {
            DrawMenu();
        }

        DrawMultizone();
        DrawDebug();
        DrawSystemUpdateTimer();

        areaViewport->Draw(drawSurface, 4, 4);

        PresentFrame();

        cameraX = cameraXTemp;
        cameraY = cameraYTemp;
        cameraZ = cameraZTemp;
        cameraPitch = cameraPitchTemp;
        cameraYaw = cameraYawTemp;
    }

    void Game::DrawDebug()
    {

        if (showOccluders) {
            for (int32_t i = 0; i < Scene::levelOccluderCount[Scene::topLevel]; i++) {
                const auto& occluder = Scene::levelOccluders[Scene::topLevel][i];

                bool active = false;
                for (int32_t j = 0; j < Scene::activeOccluderCount; j++) {
                    if (occluder == Scene::activeOccluders[j]) {
                        active = true;
                        break;
                    }
                }

                if (!active) {
                    continue;
                }

                int32_t color = 0xFF0000;
                int32_t x0 = -1, y0 = -1;
                int32_t x1 = -1, y1 = -1;
                int32_t x2 = -1, y2 = -1;
                int32_t x3 = -1, y3 = -1;

                switch (occluder.type) {
                    case 1: {
                        color = 0x00FF00;
                        Project(occluder.minX, occluder.minY, occluder.minZ);
                        x0 = projectX;
                        y0 = projectY;
                        Project(occluder.minX, occluder.maxY, occluder.minZ);
                        x1 = projectX;
                        y1 = projectY;
                        Project(occluder.minX, occluder.minY, occluder.maxZ);
                        x2 = projectX;
                        y2 = projectY;
                        Project(occluder.minX, occluder.maxY, occluder.maxZ);
                        x3 = projectX;
                        y3 = projectY;
                        break;
                    }
                    case 2: {
                        color = 0x00FF00;
                        Project(occluder.minX, occluder.minY, occluder.minZ);
                        x0 = projectX;
                        y0 = projectY;
                        Project(occluder.maxX, occluder.minY, occluder.minZ);
                        x1 = projectX;
                        y1 = projectY;
                        Project(occluder.minX, occluder.maxY, occluder.minZ);
                        x2 = projectX;
                        y2 = projectY;
                        Project(occluder.maxX, occluder.maxY, occluder.minZ);
                        x3 = projectX;
                        y3 = projectY;
                        break;
                    }
                    case 4: {// Ground on XZ plane
                        color = 0xFFFF00;
                        Project(occluder.minX, occluder.minY, occluder.minZ);
                        x0 = projectX;
                        y0 = projectY;
                        Project(occluder.maxX, occluder.minY, occluder.minZ);
                        x1 = projectX;
                        y1 = projectY;
                        Project(occluder.minX, occluder.minY, occluder.maxZ);
                        x2 = projectX;
                        y2 = projectY;
                        Project(occluder.maxX, occluder.minY, occluder.maxZ);
                        x3 = projectX;
                        y3 = projectY;
                        break;
                    }
                }

                // one of our points failed to project
                if ((x0 == -1) || (x1 == -1) || (x2 == -1) || (x3 == -1)) {
                    continue;
                }

                Draw2D::DrawLine(x0, y0, x1, y1, color);
                Draw2D::DrawLine(x0, y0, x2, y2, color);
                Draw2D::DrawLine(x0, y0, x3, y3, (color & 0xFEFEFE) >> 1);
                Draw2D::DrawLine(x1, y1, x2, y2, (color & 0xFEFEFE) >> 1);
                Draw2D::DrawLine(x1, y1, x3, y3, color);
                Draw2D::DrawLine(x2, y2, x3, y3, color);
            }
        }

        if (showPerformance)
        {
            int32_t x = 507;
            int32_t y = 20;
            int32_t color = 0xffff00;

            if (fps < 15) {
                color = 0xff0000;
            }

            fontPlain11->DrawStringRight(std::format("{} fps", fps), x, y, color);
            y += 13;

            double ft = 0;
            for (double delta : frameTime) {
                ft += delta;
            }
            ft /= frameTime.size();

            fontPlain11->DrawStringRight(std::format("{:.4f} ms", ft), x, y, color);
            y += 13;

            //fontPlain11->DrawStringRight(std::to_string(get_memory_usage_kb()) + " kB", x, y, 0xffff00);
            //y += 13;

            if (showTraffic)
            {
                fontPlain11->DrawStringRight(std::to_string(bytesIn) + " bytes in", x, y, 0xffff00);
                y += 13;

                fontPlain11->DrawStringRight(std::to_string(bytesOut) + " bytes out", x, y, 0xffff00);
                y += 13;

                if ((loopCycle % 50) == 0) {
                    bytesIn = 0;
                    bytesOut = 0;
                }
            }

            if (showOccluders) {
                fontPlain11->DrawStringRight(std::format("{}/{} occluders", Scene::activeOccluderCount,
                    Scene::levelOccluderCount[Scene::topLevel]), x, y, 0xFFFF00);
            }
        }
    }

    void Game::DrawSidebar()
    {
        areaSidebar->Bind();
        Draw3D::lineOffset = areaSidebarOffsets;
        imageInvback->Blit(0, 0);

        if (sidebarInterfaceID != -1) {
            DrawParentInterface(*IfType::instances[sidebarInterfaceID], 0, 0, 0);
        } else if (tabInterfaceID[selectedTab] != -1) {
            DrawParentInterface(*IfType::instances[tabInterfaceID[selectedTab]], 0, 0, 0);
        }

        if (menuVisible && (menuArea == 1)) {
            DrawMenu();
        }

        areaSidebar->Draw(drawSurface, 553, 205);
        areaViewport->Bind();
        Draw3D::lineOffset = areaViewportOffsets;
    }

    void Game::DrawChatback()
    {
        areaChatback->Bind();
        Draw3D::lineOffset = areaChatbackOffsets;
        imageChatback->Blit(0, 0);

        if (showSocialInput) {
            fontBold12->DrawStringCenter(socialMessage, 239, 40, 0);
            fontBold12->DrawStringCenter(socialInput + "*", 239, 60, 128);
        } else if (chatbackInputType == 1) {
            fontBold12->DrawStringCenter("Enter amount:", 239, 40, 0);
            fontBold12->DrawStringCenter(chatbackInput + "*", 239, 60, 128);
        } else if (chatbackInputType == 2) {
            fontBold12->DrawStringCenter("Enter name:", 239, 40, 0);
            fontBold12->DrawStringCenter(chatbackInput + "*", 239, 60, 128);
        } else if (!modalMessage.empty()) {
            fontBold12->DrawStringCenter(modalMessage, 239, 40, 0);
            fontBold12->DrawStringCenter("Click to continue", 239, 60, 128);
        } else if (chatInterfaceID != -1) {
            DrawParentInterface(*IfType::instances[chatInterfaceID], 0, 0, 0);
        } else if (stickyChatInterfaceID != -1) {
            DrawParentInterface(*IfType::instances[stickyChatInterfaceID], 0, 0, 0);
        } else {
            DrawChat();
        }

        if (menuVisible && (menuArea == 2)) {
            DrawMenu();
        }

        areaChatback->Draw(drawSurface, 17, 357);

        areaViewport->Bind();
        Draw3D::lineOffset = areaViewportOffsets;
    }

    void Game::DrawSideicons()
    {
        if (!redrawSideicons) {
            return;
        }
        if (flashingTab != -1 && (flashingTab == selectedTab)) {
            flashingTab = -1;
            out.WriteOp(120);
            out.Write8(selectedTab);
        }
        redrawSideicons = false;
        areaBackhmid1->Bind();
        imageBackhmid1->Blit(0, 0);
        if (sidebarInterfaceID == -1) {
            if (tabInterfaceID[selectedTab] != -1) {
                if (selectedTab == 0) {
                    imageRedstone1->Blit(22, 10);
                }
                if (selectedTab == 1) {
                    imageRedstone2->Blit(54, 8);
                }
                if (selectedTab == 2) {
                    imageRedstone2->Blit(82, 8);
                }
                if (selectedTab == 3) {
                    imageRedstone3->Blit(110, 8);
                }
                if (selectedTab == 4) {
                    imageRedstone2h->Blit(153, 8);
                }
                if (selectedTab == 5) {
                    imageRedstone2h->Blit(181, 8);
                }
                if (selectedTab == 6) {
                    imageRedstone1h->Blit(209, 9);
                }
            }
            if ((tabInterfaceID[0] != -1) && ((flashingTab != 0) || ((loopCycle % 20) < 10))) {
                imageSideicons[0]->Blit(29, 13);
            }
            if ((tabInterfaceID[1] != -1) && ((flashingTab != 1) || ((loopCycle % 20) < 10))) {
                imageSideicons[1]->Blit(53, 11);
            }
            if ((tabInterfaceID[2] != -1) && ((flashingTab != 2) || ((loopCycle % 20) < 10))) {
                imageSideicons[2]->Blit(82, 11);
            }
            if ((tabInterfaceID[3] != -1) && ((flashingTab != 3) || ((loopCycle % 20) < 10))) {
                imageSideicons[3]->Blit(115, 12);
            }
            if ((tabInterfaceID[4] != -1) && ((flashingTab != 4) || ((loopCycle % 20) < 10))) {
                imageSideicons[4]->Blit(153, 13);
            }
            if ((tabInterfaceID[5] != -1) && ((flashingTab != 5) || ((loopCycle % 20) < 10))) {
                imageSideicons[5]->Blit(180, 11);
            }
            if ((tabInterfaceID[6] != -1) && ((flashingTab != 6) || ((loopCycle % 20) < 10))) {
                imageSideicons[6]->Blit(208, 13);
            }
        }
        areaBackhmid1->Draw(drawSurface, 516, 160);
        areaBackbase2->Bind();
        imageBackbase2->Blit(0, 0);
        if (sidebarInterfaceID == -1) {
            if (tabInterfaceID[selectedTab] != -1) {
                if (selectedTab == 7) {
                    imageRedstone1v->Blit(42, 0);
                }
                if (selectedTab == 8) {
                    imageRedstone2v->Blit(74, 0);
                }
                if (selectedTab == 9) {
                    imageRedstone2v->Blit(102, 0);
                }
                if (selectedTab == 10) {
                    imageRedstone3v->Blit(130, 1);
                }
                if (selectedTab == 11) {
                    imageRedstone2hv->Blit(173, 0);
                }
                if (selectedTab == 12) {
                    imageRedstone2hv->Blit(201, 0);
                }
                if (selectedTab == 13) {
                    imageRedstone1hv->Blit(229, 0);
                }
            }
            if ((tabInterfaceID[8] != -1) && ((flashingTab != 8) || ((loopCycle % 20) < 10))) {
                imageSideicons[7]->Blit(74, 2);
            }
            if ((tabInterfaceID[9] != -1) && ((flashingTab != 9) || ((loopCycle % 20) < 10))) {
                imageSideicons[8]->Blit(102, 3);
            }
            if ((tabInterfaceID[10] != -1) && ((flashingTab != 10) || ((loopCycle % 20) < 10))) {
                imageSideicons[9]->Blit(137, 4);
            }
            if ((tabInterfaceID[11] != -1) && ((flashingTab != 11) || ((loopCycle % 20) < 10))) {
                imageSideicons[10]->Blit(174, 2);
            }
            if ((tabInterfaceID[12] != -1) && ((flashingTab != 12) || ((loopCycle % 20) < 10))) {
                imageSideicons[11]->Blit(201, 2);
            }
            if ((tabInterfaceID[13] != -1) && ((flashingTab != 13) || ((loopCycle % 20) < 10))) {
                imageSideicons[12]->Blit(226, 2);
            }
        }
        areaBackbase2->Draw(drawSurface, 496, 466);
        areaViewport->Bind();
    }

    void Game::DrawPrivacySettings()
    {
        if (!redrawPrivacySettings) {
            return;
        }
        redrawPrivacySettings = false;
        areaBackbase1->Bind();
        imageBackbase1->Blit(0, 0);
        fontPlain12->DrawStringTaggableCenter("Public chat", 55, 28, 0xffffff, true);
        if (publicChatSetting == 0) {
            fontPlain12->DrawStringTaggableCenter("On", 55, 41, 65280, true);
        }
        if (publicChatSetting == 1) {
            fontPlain12->DrawStringTaggableCenter("Friends", 55, 41, 0xffff00, true);
        }
        if (publicChatSetting == 2) {
            fontPlain12->DrawStringTaggableCenter("Off", 55, 41, 0xff0000, true);
        }
        if (publicChatSetting == 3) {
            fontPlain12->DrawStringTaggableCenter("Hide", 55, 41, 65535, true);
        }
        fontPlain12->DrawStringTaggableCenter("Private chat", 184, 28, 0xffffff, true);
        if (privateChatSetting == 0) {
            fontPlain12->DrawStringTaggableCenter("On", 184, 41, 65280, true);
        }
        if (privateChatSetting == 1) {
            fontPlain12->DrawStringTaggableCenter("Friends", 184, 41, 0xffff00, true);
        }
        if (privateChatSetting == 2) {
            fontPlain12->DrawStringTaggableCenter("Off", 184, 41, 0xff0000, true);
        }
        fontPlain12->DrawStringTaggableCenter("Trade/compete", 324, 28, 0xffffff, true);
        if (tradeChatSetting == 0) {
            fontPlain12->DrawStringTaggableCenter("On", 324, 41, 65280, true);
        }
        if (tradeChatSetting == 1) {
            fontPlain12->DrawStringTaggableCenter("Friends", 324, 41, 0xffff00, true);
        }
        if (tradeChatSetting == 2) {
            fontPlain12->DrawStringTaggableCenter("Off", 324, 41, 0xff0000, true);
        }
        fontPlain12->DrawStringTaggableCenter("Report abuse", 458, 33, 0xffffff, true);
        areaBackbase1->Draw(drawSurface, 0, 453);
        areaViewport->Bind();
    }

    void Game::DrawMinimap()
    {
        areaMapback->Bind();

        if (minimapState == 2) {
            const auto& mapback = imageMapback->pixels;
            const auto& pixels = Draw2D::Pixels();
            for (int32_t i = 0; i < mapback.size(); i++) {
                if (mapback[i] == 0) {
                    pixels[i] = 0;
                }
            }
            imageCompass->DrawRotatedMasked(0, 0, 33, 33, 25, 25, 256,
                orbitCameraYaw, compassMaskLineLengths, compassMaskLineOffsets);
            areaViewport->Bind();
            return;
        }

        int32_t angle = (orbitCameraYaw + minimapAnticheatAngle) & 0x7ff;
        int32_t anchorX = 48 + (localPlayer->x / 32);
        int32_t anchorY = 464 - (localPlayer->z / 32);

        imageMinimap->DrawRotatedMasked(25, 5, 146, 151, anchorX, anchorY, 256 + minimapZoom,
            angle, minimapMaskLineLengths, minimapMaskLineOffsets);
        imageCompass->DrawRotatedMasked(0, 0, 33, 33, 25, 25, 256,
            orbitCameraYaw, compassMaskLineLengths, compassMaskLineOffsets);

        DrawMinimapFunctions();
        DrawMinimapObjs();
        DrawMinimapNPCs();
        DrawMinimapPlayers();
        DrawMinimapHint();
        DrawMinimapFlag();

        // center dot
        Draw2D::FillRect(97, 78, 3, 3, 0xffffff);

        areaViewport->Bind();
    }

    void Game::DrawMinimapPlayers()
    {
        for (int32_t i = 0; i < playerCount; i++) {
            const auto& player = players[playerIDs[i]];

            if ((player == nullptr) || !player->IsVisible()) {
                continue;
            }

            int32_t x = (player->x / 32) - (localPlayer->x / 32);
            int32_t y = (player->z / 32) - (localPlayer->z / 32);
            bool isFriend = false;
            auto name37 = StringUtil::ToBase37(player->name);

            for (int32_t j = 0; j < friendCount; j++) {
                if ((name37 != friendName37[j]) || (friendWorld[j] == 0)) {
                    continue;
                }
                isFriend = true;
                break;
            }

            bool team = (player->team != 0) && (localPlayer->team == player->team);

            if (isFriend) {
                DrawOnMinimap(*imageMapdot3, x, y);
            } else if (team) {
                DrawOnMinimap(*imageMapdot4, x, y);
            } else {
                DrawOnMinimap(*imageMapdot2, x, y);
            }
        }
    }

    void Game::DrawMinimapFlag() const
    {
        if (flagSceneTileX != 0) {
            int32_t flagX = ((flagSceneTileX * 4) + 2) - (localPlayer->x / 32);
            int32_t flagY = ((flagSceneTileZ * 4) + 2) - (localPlayer->z / 32);
            DrawOnMinimap(*imageMapmarker0, flagX, flagY);
        }
    }

    void Game::DrawOnMinimap(Image24& image, int32_t dx, int32_t dy) const
    {
        int32_t angle = (orbitCameraYaw + minimapAnticheatAngle) & 0x7ff;
        int32_t distance = (dx * dx) + (dy * dy);

        if (distance > 6400) {
            return;
        }

        int32_t sinAngle = Draw3D::sin[angle];
        int32_t cosAngle = Draw3D::cos[angle];

        sinAngle = (sinAngle * 256) / (minimapZoom + 256);
        cosAngle = (cosAngle * 256) / (minimapZoom + 256);

        int32_t x = ((dy * sinAngle) + (dx * cosAngle)) >> 16;
        int32_t y = ((dy * cosAngle) - (dx * sinAngle)) >> 16;

        if (distance > 2500) {
            image.DrawMasked(*imageMapback, 83 - y - (image.cropH / 2) - 4, ((94 + x) - (image.cropW / 2)) + 4);
        } else {
            image.Draw(((94 + x) - (image.cropW / 2)) + 4, 83 - y - (image.cropH / 2) - 4);
        }
    }

    void Game::CreateMinimap(int32_t level)
    {
        auto& pixels = imageMinimap->pixels;

        std::ranges::fill(pixels, 0);

        for (int32_t z = 1; z < 103; z++) {
            int32_t offset = 52 + (48 * 512) + ((103 - z) * 512 * 4);

            for (int32_t x = 1; x < 103; x++) {
                if ((levelTileFlags[level][x][z] & 0x18) == 0) {
                    scene->DrawMinimapTile(pixels, offset, 512, level, x, z);
                }

                if ((level < 3) && ((levelTileFlags[level + 1][x][z] & 8) != 0)) {
                    scene->DrawMinimapTile(pixels, offset, 512, level + 1, x, z);
                }

                offset += 4;
            }
        }

        int32_t wallRGB = 0xFFEEEEEE;
        int32_t doorRGB = 0xFFEE0000;

        imageMinimap->Bind();

        for (int32_t z = 1; z < 103; z++) {
            for (int32_t x = 1; x < 103; x++) {
                if ((levelTileFlags[level][x][z] & 0x18) == 0) {
                    DrawMinimapLoc(z, wallRGB, x, doorRGB, level);
                }

                if ((level < 3) && ((levelTileFlags[level + 1][x][z] & 8) != 0)) {
                    DrawMinimapLoc(z, wallRGB, x, doorRGB, level + 1);
                }
            }
        }

        areaViewport->Bind();
        activeMapFunctionCount = 0;

        for (int32_t tileX = 0; tileX < 104; tileX++) {
            for (int32_t tileZ = 0; tileZ < 104; tileZ++) {
                int32_t bitset = scene->GetGroundDecorationBitset(currentLevel, tileX, tileZ);

                if (bitset == 0) {
                    continue;
                }

                bitset = (bitset >> 14) & 0x7fff;

                int32_t func = LocType::Get(bitset)->mapfunctionIcon;

                if (func < 0) {
                    continue;
                }

                int32_t stx = tileX;
                int32_t stz = tileZ;

                if ((func != 22) && (func != 29) && (func != 34) && (func != 36) && (func != 46) && (func != 47) && (func != 48)) {
                    int8_t byte0 = 104;
                    int8_t byte1 = 104;
                    auto& flags = levelCollisionMap[currentLevel]->flags;
                    for (int32_t i4 = 0; i4 < 10; i4++) {
                        int32_t j4 = SDL_rand(4);
                        if ((j4 == 0) && (stx > 0) && (stx > (tileX - 3)) && ((flags[stx - 1][stz] & 0x1280108) == 0)) {
                            stx--;
                        }
                        if ((j4 == 1) && (stx < (byte0 - 1)) && (stx < (tileX + 3)) && ((flags[stx + 1][stz] & 0x1280180) == 0)) {
                            stx++;
                        }
                        if ((j4 == 2) && (stz > 0) && (stz > (tileZ - 3)) && ((flags[stx][stz - 1] & 0x1280102) == 0)) {
                            stz--;
                        }
                        if ((j4 == 3) && (stz < (byte1 - 1)) && (stz < (tileZ + 3)) && ((flags[stx][stz + 1] & 0x1280120) == 0)) {
                            stz++;
                        }
                    }
                }
                activeMapFunctions[activeMapFunctionCount] = std::make_unique<Image24>(*imageMapfunction[func]);
                activeMapFunctionX[activeMapFunctionCount] = stx;
                activeMapFunctionZ[activeMapFunctionCount] = stz;
                activeMapFunctionCount++;
            }
        }
    }

    void Game::DrawMinimapLoc(int32_t tileZ, int32_t wallRGB, int32_t tileX, int32_t doorRGB, int32_t level) const
    {
        int32_t bitset = scene->GetWallBitset(level, tileX, tileZ);

        if (bitset != 0) {
            int32_t info = scene->GetInfo(level, tileX, tileZ, bitset);
            int32_t rotation = (info >> 6) & 3;
            int32_t kind = info & 0x1f;
            int32_t rgb = wallRGB;

            if (bitset > 0) {
                rgb = doorRGB;
            }

            auto& dst = imageMinimap->pixels;
            int32_t offset = 24624 + (tileX * 4) + ((103 - tileZ) * 512 * 4);

            int32_t locID = (bitset >> 14) & 0x7fff;
            const auto& type = LocType::Get(locID);

            if (type->mapsceneIcon != -1) {
                const auto& icon = imageMapscene[type->mapsceneIcon];
                if (icon != nullptr) {
                    int32_t offsetX = ((type->sizeX * 4) - icon->width) / 2;
                    int32_t offsetY = ((type->sizeZ * 4) - icon->height) / 2;
                    icon->Blit(48 + (tileX * 4) + offsetX, 48 + ((104 - tileZ - type->sizeZ) * 4) + offsetY);
                }
            } else {
                if ((kind == 0) || (kind == 2)) {
                    if (rotation == 0) {
                        dst[offset] = rgb;
                        dst[offset + 512] = rgb;
                        dst[offset + 1024] = rgb;
                        dst[offset + 1536] = rgb;
                    } else if (rotation == 1) {
                        dst[offset] = rgb;
                        dst[offset + 1] = rgb;
                        dst[offset + 2] = rgb;
                        dst[offset + 3] = rgb;
                    } else if (rotation == 2) {
                        dst[offset + 3] = rgb;
                        dst[offset + 3 + 512] = rgb;
                        dst[offset + 3 + 1024] = rgb;
                        dst[offset + 3 + 1536] = rgb;
                    } else if (rotation == 3) {
                        dst[offset + 1536] = rgb;
                        dst[offset + 1536 + 1] = rgb;
                        dst[offset + 1536 + 2] = rgb;
                        dst[offset + 1536 + 3] = rgb;
                    }
                }
                if (kind == 3) {
                    if (rotation == 0) {
                        dst[offset] = rgb;
                    } else if (rotation == 1) {
                        dst[offset + 3] = rgb;
                    } else if (rotation == 2) {
                        dst[offset + 3 + 1536] = rgb;
                    } else if (rotation == 3) {
                        dst[offset + 1536] = rgb;
                    }
                }
                if (kind == 2) {
                    if (rotation == 3) {
                        dst[offset] = rgb;
                        dst[offset + 512] = rgb;
                        dst[offset + 1024] = rgb;
                        dst[offset + 1536] = rgb;
                    } else if (rotation == 0) {
                        dst[offset] = rgb;
                        dst[offset + 1] = rgb;
                        dst[offset + 2] = rgb;
                        dst[offset + 3] = rgb;
                    } else if (rotation == 1) {
                        dst[offset + 3] = rgb;
                        dst[offset + 3 + 512] = rgb;
                        dst[offset + 3 + 1024] = rgb;
                        dst[offset + 3 + 1536] = rgb;
                    } else if (rotation == 2) {
                        dst[offset + 1536] = rgb;
                        dst[offset + 1536 + 1] = rgb;
                        dst[offset + 1536 + 2] = rgb;
                        dst[offset + 1536 + 3] = rgb;
                    }
                }
            }
        }

        bitset = scene->GetLocBitset(level, tileX, tileZ);

        if (bitset != 0) {
            int32_t info = scene->GetInfo(level, tileX, tileZ, bitset);
            int32_t rotation = (info >> 6) & 3;
            int32_t kind = info & 0x1f;
            int32_t locID = (bitset >> 14) & 0x7fff;
            const auto& type = LocType::Get(locID);

            if (type->mapsceneIcon != -1) {
                const auto& icon = imageMapscene[type->mapsceneIcon];

                if (icon != nullptr) {
                    int32_t offsetX = ((type->sizeX * 4) - icon->width) / 2;
                    int32_t offsetY = ((type->sizeZ * 4) - icon->height) / 2;
                    icon->Blit(48 + (tileX * 4) + offsetX, 48 + ((104 - tileZ - type->sizeZ) * 4) + offsetY);
                }
            } else if (kind == 9) {
                int32_t rgb = 0xFFEEEEEE;

                if (bitset > 0) {
                    rgb = 0xFFEE0000;
                }

                auto& dst = imageMinimap->pixels;
                int32_t offset = 24624 + (tileX * 4) + ((103 - tileZ) * 512 * 4);

                if (rotation == 0 || (rotation == 2)) {
                    dst[offset + 1536] = rgb;
                    dst[offset + 1024 + 1] = rgb;
                    dst[offset + 512 + 2] = rgb;
                    dst[offset + 3] = rgb;
                } else {
                    dst[offset] = rgb;
                    dst[offset + 512 + 1] = rgb;
                    dst[offset + 1024 + 2] = rgb;
                    dst[offset + 1536 + 3] = rgb;
                }
            }
        }

        bitset = scene->GetGroundDecorationBitset(level, tileX, tileZ);

        if (bitset != 0) {
            int32_t locID = (bitset >> 14) & 0x7fff;
            const auto& type = LocType::Get(locID);

            if (type->mapsceneIcon != -1) {
                const auto& icon = imageMapscene[type->mapsceneIcon];

                if (icon != nullptr) {
                    int32_t offsetX = ((type->sizeX * 4) - icon->width) / 2;
                    int32_t offsetY = ((type->sizeZ * 4) - icon->height) / 2;
                    icon->Blit(48 + (tileX * 4) + offsetX, 48 + ((104 - tileZ - type->sizeZ) * 4) + offsetY);
                }
            }
        }
    }

    void Game::DrawMinimapFunctions()
    {
        for (int32_t i = 0; i < activeMapFunctionCount; i++) {
            int32_t x = ((activeMapFunctionX[i] * 4) + 2) - (localPlayer->x / 32);
            int32_t y = ((activeMapFunctionZ[i] * 4) + 2) - (localPlayer->z / 32);
            DrawOnMinimap(*activeMapFunctions[i], x, y);
        }
    }

    void Game::DrawMinimapObjs()
    {
        for (int32_t ltx = 0; ltx < 104; ltx++) {
            for (int32_t ltz = 0; ltz < 104; ltz++) {
                auto& stack = levelObjStacks[currentLevel][ltx][ltz];

                if (!stack.isEmpty()) {
                    int32_t x = ((ltx * 4) + 2) - (localPlayer->x / 32);
                    int32_t y = ((ltz * 4) + 2) - (localPlayer->z / 32);
                    DrawOnMinimap(*imageMapdot0, x, y);
                }
            }
        }
    }

    void Game::DrawMinimapNPCs()
    {
        for (int32_t i = 0; i < npcCount; i++) {
            const auto& npc = npcs[npcIDs[i]];
            if ((npc == nullptr) || !npc->IsVisible()) {
                continue;
            }
            auto type = npc->type;
            if (!type->overrides.empty()) {
                type = type->GetOverrideType();
            }
            if ((type != nullptr) && type->showOnMinimap && type->interactable) {
                int32_t x = (npc->x / 32) - (localPlayer->x / 32);
                int32_t y = (npc->z / 32) - (localPlayer->z / 32);
                DrawOnMinimap(*imageMapdot1, x, y);
            }
        }
    }

    void Game::DrawChat()
    {
        const auto& font = fontPlain12;
        int32_t line = 0;
        Draw2D::SetBounds(0, 0, 463, 77);

        for (int32_t i = 0; i < 100; i++) {
            if (messageText[i].empty()) {
                continue;
            }

            int32_t type = messageType[i];
            int32_t y = (70 - (line * 14)) + chatScrollOffset;
            auto sender = messageSender[i];
            int8_t icon = 0;

            if (!sender.empty() && sender.starts_with("@cr1@")) {
                sender = sender.substr(5);
                icon = 1;
            }

            if (!sender.empty() && sender.starts_with("@cr2@")) {
                sender = sender.substr(5);
                icon = 2;
            }

            if (type == 0) {
                if ((y > 0) && (y < 110)) {
                    font->DrawString(messageText[i], 4, y, 0);
                }
                line++;
            }

            if (((type == 1) || (type == 2)) && ((type == 1) || (publicChatSetting == 0) || ((publicChatSetting == 1) && IsFriend(sender)))) {
                if ((y > 0) && (y < 110)) {
                    int32_t x = 4;

                    if (icon == 1) {
                        imageModIcons[0]->Blit(x, y - 12);
                        x += 14;
                    }

                    if (icon == 2) {
                        imageModIcons[1]->Blit(x, y - 12);
                        x += 14;
                    }

                    font->DrawString(sender + ":", x, y, 0);
                    x += font->StringWidthTaggable(sender) + 8;

                    font->DrawString(messageText[i], x, y, 255);
                }
                line++;
            }

            if (((type == 3) || (type == 7)) && (splitPrivateChat == 0) && ((type == 7) || (privateChatSetting == 0) || ((privateChatSetting == 1) && IsFriend(sender)))) {
                if ((y > 0) && (y < 110)) {
                    int32_t x = 4;

                    font->DrawString("From", x, y, 0);
                    x += font->StringWidthTaggable("From ");

                    if (icon == 1) {
                        imageModIcons[0]->Blit(x, y - 12);
                        x += 14;
                    }

                    if (icon == 2) {
                        imageModIcons[1]->Blit(x, y - 12);
                        x += 14;
                    }

                    font->DrawString(sender + ":", x, y, 0);
                    x += font->StringWidthTaggable(sender) + 8;

                    font->DrawString(messageText[i], x, y, 0x800000);
                }
                line++;
            }

            if ((type == 4) && ((tradeChatSetting == 0) || ((tradeChatSetting == 1) && IsFriend(sender)))) {
                if ((y > 0) && (y < 110)) {
                    font->DrawString(sender + " " + messageText[i], 4, y, 0x800080);
                }
                line++;
            }

            if ((type == 5) && (splitPrivateChat == 0) && (privateChatSetting < 2)) {
                if ((y > 0) && (y < 110)) {
                    font->DrawString(messageText[i], 4, y, 0x800000);
                }
                line++;
            }

            if ((type == 6) && (splitPrivateChat == 0) && (privateChatSetting < 2)) {
                if ((y > 0) && (y < 110)) {
                    font->DrawString("To " + sender + ":", 4, y, 0);
                    font->DrawString(messageText[i], 12 + font->StringWidthTaggable("To " + sender), y, 0x800000);
                }
                line++;
            }

            if ((type == 8) && ((tradeChatSetting == 0) || ((tradeChatSetting == 1) && IsFriend(sender)))) {
                if ((y > 0) && (y < 110)) {
                    font->DrawString(sender + " " + messageText[i], 4, y, 0x7e3200);
                }
                line++;
            }
        }

        Draw2D::ResetBounds();

        chatScrollHeight = (line * 14) + 7;

        if (chatScrollHeight < 78) {
            chatScrollHeight = 78;
        }

        DrawScrollbar(463, 0, 77, chatScrollHeight, chatScrollHeight - chatScrollOffset - 77);

        std::string name;

        if ((localPlayer != nullptr) && (!localPlayer->name.empty())) {
            name = localPlayer->name;
        } else {
            name = StringUtil::FormatName(username);
        }

        font->DrawString(name + ":", 4, 90, 0);
        font->DrawString(chatTyped + "*", 6 + font->StringWidthTaggable(name + ": "), 90, 255);
        Draw2D::DrawLineX(0, 77, 479, 0);
    }

    void Game::DrawChats()
    {
        for (int32_t i = 0; i < chatCount; i++) {
            int32_t x = chatX[i];
            int32_t y = chatY[i];
            int32_t padding = chatWidth[i];
            int32_t height = chatHeight[i];

            bool sorting = true;

            while (sorting) {
                sorting = false;
                for (int32_t j = 0; j < i; j++) {
                    if ((y + 2) > (chatY[j] - chatHeight[j]) && (y - height) < (chatY[j] + 2) &&
                        (x - padding < chatX[j] + chatWidth[j]) &&
                        (x + padding) > (chatX[j] - chatWidth[j]) && (chatY[j] - chatHeight[j]) < y) {
                        y = chatY[j] - chatHeight[j];
                        sorting = true;
                        }
                }
            }

            projectX = chatX[i];
            projectY = chatY[i] = y;

            const auto& message = chats[i];

            if (chatEffects == 0) {
                int32_t color = 0xffff00;

                if (chatColors[i] < 6) {
                    color = CHAT_COLORS[chatColors[i]];
                }

                if (chatColors[i] == 6) {
                    color = ((sceneCycle % 20) >= 10) ? 0xffff00 : 0xff0000;
                }

                if (chatColors[i] == 7) {
                    color = ((sceneCycle % 20) >= 10) ? 65535 : 255;
                }

                if (chatColors[i] == 8) {
                    color = ((sceneCycle % 20) >= 10) ? 0x80ff80 : 45056;
                }

                if (chatColors[i] == 9) {
                    int32_t delta = 150 - chatTimers[i];

                    if (delta < 50) {
                        color = 0xff0000 + (1280 * delta);
                    } else if (delta < 100) {
                        color = 0xffff00 - (0x50000 * (delta - 50));
                    } else if (delta < 150) {
                        color = 65280 + (5 * (delta - 100));
                    }
                }

                if (chatColors[i] == 10) {
                    int32_t delta = 150 - chatTimers[i];

                    if (delta < 50) {
                        color = 0xff0000 + (5 * delta);
                    } else if (delta < 100) {
                        color = 0xff00ff - (0x50000 * (delta - 50));
                    } else if (delta < 150) {
                        color = (255 + (0x50000 * (delta - 100))) - (5 * (delta - 100));
                    }
                }
                if (chatColors[i] == 11) {
                    int32_t delta = 150 - chatTimers[i];

                    if (delta < 50) {
                        color = 0xffffff - (0x50005 * delta);
                    } else if (delta < 100) {
                        color = 65280 + (0x50005 * (delta - 50));
                    } else if (delta < 150) {
                        color = 0xffffff - (0x50000 * (delta - 100));
                    }
                }


                if (chatStyles[i] == 0) {
                    fontBold12->DrawStringCenter(message, projectX, projectY + 1, 0);
                    fontBold12->DrawStringCenter(message, projectX, projectY, color);
                }

                if (chatStyles[i] == 1) {
                    fontBold12->DrawStringWave(message, projectX, projectY + 1, 0, sceneCycle);
                    fontBold12->DrawStringWave(message, projectX, projectY, color, sceneCycle);
                }

                if (chatStyles[i] == 2) {
                    fontBold12->DrawStringWave2(message, projectX, projectY + 1, 0, sceneCycle);
                    fontBold12->DrawStringWave2(message, projectX, projectY, color, sceneCycle);
                }

                if (chatStyles[i] == 3) {
                    fontBold12->DrawStringShake(message, projectX, projectY + 1, 0, sceneCycle, 150 - chatTimers[i]);
                    fontBold12->DrawStringShake(message, projectX, projectY, color, sceneCycle, 150 - chatTimers[i]);
                }

                if (chatStyles[i] == 4) {
                    int32_t w = fontBold12->StringWidth(message);
                    int32_t offsetX = ((150 - chatTimers[i]) * (w + 100)) / 150;
                    Draw2D::SetBounds(projectX - 50, 0, projectX + 50, 334);
                    fontBold12->DrawString(message, (projectX + 50) - offsetX, projectY + 1, 0);
                    fontBold12->DrawString(message, (projectX + 50) - offsetX, projectY, color);
                    Draw2D::ResetBounds();
                }

                if (chatStyles[i] == 5) {
                    int32_t delta = 150 - chatTimers[i];
                    int32_t slide = 0;
                    if (delta < 25) {
                        slide = delta - 25;
                    } else if (delta > 125) {
                        slide = delta - 125;
                    }
                    Draw2D::SetBounds(0, projectY - fontBold12->height - 1, 512, projectY + 5);
                    fontBold12->DrawStringCenter(message, projectX, projectY + 1 + slide, 0);
                    fontBold12->DrawStringCenter(message, projectX, projectY + slide, color);
                    Draw2D::ResetBounds();
                }
            } else {
                fontBold12->DrawStringCenter(message, projectX, projectY + 1, 0);
                fontBold12->DrawStringCenter(message, projectX, projectY, 0xffff00);
            }
        }
    }

    void Game::DrawScrollbar(int32_t x, int32_t y, int32_t height, int32_t scrollHeight, int32_t scrollY) const
    {
        imageScrollbar0->Blit(x, y);
        imageScrollbar1->Blit(x, (y + height) - 16);
        Draw2D::FillRect(x, y + 16, 16, height - 32, 0x23201b);
        int32_t gripSize = ((height - 32) * height) / scrollHeight;
        if (gripSize < 8) {
            gripSize = 8;
        }
        int32_t gripY = ((height - 32 - gripSize) * scrollY) / (scrollHeight - height);
        Draw2D::FillRect(x, y + 16 + gripY, 16, gripSize, 0x4d4233);
        Draw2D::DrawLineY(x, y + 16 + gripY, gripSize, 0x766654);
        Draw2D::DrawLineY(x + 1, y + 16 + gripY, gripSize, 0x766654);
        Draw2D::DrawLineX(x, y + 16 + gripY, 16, 0x766654);
        Draw2D::DrawLineX(x, y + 17 + gripY, 16, 0x766654);
        Draw2D::DrawLineY(x + 15, y + 16 + gripY, gripSize, 0x332d25);
        Draw2D::DrawLineY(x + 14, y + 17 + gripY, gripSize - 1, 0x332d25);
        Draw2D::DrawLineX(x, y + 15 + gripY + gripSize, 16, 0x332d25);
        Draw2D::DrawLineX(x + 1, y + 14 + gripY + gripSize, 15, 0x332d25);
    }

    void Game::Draw2DEntityElements()
    {
        chatCount = 0;
        for (int32_t index = -1; index < (playerCount + npcCount); index++) {
            std::shared_ptr<PathingEntity> entity;

            if (index == -1) {
                entity = localPlayer;
            } else if (index < playerCount) {
                entity = players[playerIDs[index]];
            } else {
                entity = npcs[npcIDs[index - playerCount]];
            }

            if (entity == nullptr || !entity->IsVisible()) {
                continue;
            }

            if (auto npc = std::dynamic_pointer_cast<NPCEntity>(entity)) {
                auto type = npc->type;
                if (!type->overrides.empty()) {
                    type = type->GetOverrideType();
                }
                if (type == nullptr) {
                    continue;
                }
            }

            if (index < playerCount) {
                const auto& player = std::static_pointer_cast<PlayerEntity>(entity);

                int32_t y = 30;

                if (player->headicons != 0) {
                    ProjectFromGround(player, player->height + 15);

                    if (projectX > -1) {
                        for (int32_t icon = 0; icon < 8; icon++) {
                            if ((player->headicons & (1 << icon)) != 0) {
                                imageHeadicons[icon]->Draw(projectX - 12, projectY - y);
                                y -= 25;
                            }
                        }
                    }
                }

                if ((index >= 0) && (hintType == 10) && (hintPlayer == playerIDs[index])) {
                    ProjectFromGround(player, player->height + 15);

                    if (projectX > -1) {
                        imageHeadicons[7]->Draw(projectX - 12, projectY - y);
                    }
                }
            } else {
                auto npc = std::dynamic_pointer_cast<NPCEntity>(entity);
                auto type = npc->type;

                if ((type->headicon >= 0) && (type->headicon < static_cast<int32_t>(imageHeadicons.size()))) {
                    ProjectFromGround(entity, entity->height + 15);

                    if (projectX > -1) {
                        imageHeadicons[type->headicon]->Draw(projectX - 12, projectY - 30);
                    }
                }
                if ((hintType == 1) && (hintNPC == npcIDs[index - playerCount]) && ((loopCycle % 20) < 10)) {
                    ProjectFromGround(entity, entity->height + 15);
                    if (projectX > -1) {
                        imageHeadicons[2]->Draw(projectX - 12, projectY - 28);
                    }
                }
            }

            if (!entity->chat.empty() && (index >= playerCount || (publicChatSetting == 0) ||
                (publicChatSetting == 3) || publicChatSetting == 1 &&
                IsFriend(std::static_pointer_cast<PlayerEntity>(entity)->name))) {
                ProjectFromGround(entity, entity->height);

                if (projectX > -1 && (chatCount < MAX_CHATS)) {

                    chatWidth[chatCount] = fontBold12->StringWidth(entity->chat) / 2;
                    chatHeight[chatCount] = fontBold12->height;
                    chatX[chatCount] = projectX;
                    chatY[chatCount] = projectY;
                    chatColors[chatCount] = entity->chatColor;
                    chatStyles[chatCount] = entity->chatStyle;
                    chatTimers[chatCount] = entity->chatTimer;
                    chats[chatCount++] = entity->chat;

                    if ((chatEffects == 0) && (entity->chatStyle >= 1) && (entity->chatStyle <= 3)) {
                        chatHeight[chatCount] += 10;
                        chatY[chatCount] += 5;
                    }

                    if ((chatEffects == 0) && (entity->chatStyle == 4)) {
                        chatWidth[chatCount] = 60;
                    }

                    if ((chatEffects == 0) && (entity->chatStyle == 5)) {
                        chatHeight[chatCount] += 5;
                    }
                }
                }

            DrawHealth(entity);
            DrawHitmarks(entity);
        }
    }

    void Game::UpdateSceneState() {
        if (lowmem && (sceneState == 2) && (SceneBuilder::curLevel != currentLevel)) {
            areaViewport->Bind();
            fontPlain12->DrawStringCenter("Loading - please wait.", 257, 151, 0);
            fontPlain12->DrawStringCenter("Loading - please wait.", 256, 150, 0xffffff);
            areaViewport->Draw(drawSurface, 4, 4);
            sceneState = 1;
            sceneLoadStartTime = SDL_GetTicks();
        }

        if (sceneState == 1) {
            int32_t state = CheckScene();

            if ((state != 0) && ((SDL_GetTicks() - sceneLoadStartTime) > 360000L)) {
                LOG_ERROR("%s glcfb %lld,%d,%d,%p,%d,%d,%d", username.c_str(), serverSeed, state, lowmem, filestores[0].get(), currentLevel, sceneCenterZoneX, sceneCenterZoneZ);
                sceneLoadStartTime = SDL_GetTicks();
            }
        }

        if ((sceneState == 2) && (currentLevel != minimapLevel)) {
            minimapLevel = currentLevel;
            CreateMinimap(currentLevel);
        }
    }

    int32_t Game::CheckScene() {
        const size_t n = sceneMapLandData.size();

        for (size_t i = 0; i < n; ++i) {
            // if there is a file but the data vector is empty -> missing data
            if (sceneMapLandFile[i] != -1 && sceneMapLandData[i].empty()) {
                return -1;
            }
            if (sceneMapLocFile[i] != -1 && sceneMapLocData[i].empty()) {
                return -2;
            }
        }

        if (!IsSceneLocsLoaded()) {
            return -3;
        }

        if (awaitingSync) {
            return -4;
        }

        sceneState = 2;
        SceneBuilder::curLevel = currentLevel;
        BuildScene();
        out.WriteOp(121);
        return 0;
    }

    int32_t Game::GetHeightmapY(int32_t level, int32_t sceneX, int32_t sceneZ)
    {
        int32_t tileX = sceneX >> 7;
        int32_t tileZ = sceneZ >> 7;

        if ((tileX < 0) || (tileZ < 0) || (tileX > 103) || (tileZ > 103)) {
            return 0;
        }

        int32_t realLevel = level;

        if ((realLevel < 3) && ((levelTileFlags[1][tileX][tileZ] & 2) == 2)) {
            realLevel++;
        }

        int32_t tileLocalX = sceneX & 0x7f; // the coordinate within the tile 0..127
        int32_t tileLocalZ = sceneZ & 0x7f;
        int32_t y00 = ((levelHeightmap[realLevel][tileX][tileZ] * (128 - tileLocalX)) + (levelHeightmap[realLevel][tileX + 1][tileZ] * tileLocalX)) >> 7;
        int32_t y11 = ((levelHeightmap[realLevel][tileX][tileZ + 1] * (128 - tileLocalX)) + (levelHeightmap[realLevel][tileX + 1][tileZ + 1] * tileLocalX)) >> 7;
        return ((y00 * (128 - tileLocalZ)) + (y11 * tileLocalZ)) >> 7;
    }

    void Game::ReadRebuildRegion() {
        int32_t zoneX = sceneCenterZoneX;
        int32_t zoneZ = sceneCenterZoneZ;

        if (packetType == PacketIn::REBUILD_REGION)
        {
            zoneX = in.ReadU16A();
            zoneZ = in.ReadU16();
            sceneInstanced = false;
        }

        if (packetType == PacketIn::REBUILD_REGION_INSTANCE) {
            zoneZ = in.ReadU16A();
            in.AccessBits();

            for (int32_t level = 0; level < 4; level++) {
                for (int32_t cx = 0; cx < 13; cx++) {
                    for (int32_t cz = 0; cz < 13; cz++) {
                        if (in.ReadN(1) == 1) {
                            levelChunkBitset[level][cx][cz] = in.ReadN(26);
                        } else {
                            levelChunkBitset[level][cx][cz] = -1;
                        }
                    }
                }
            }

            in.AccessBytes();
            zoneX = in.ReadU16();
            sceneInstanced = true;
        }

        if ((sceneCenterZoneX == zoneX) && (sceneCenterZoneZ == zoneZ) && (sceneState == 2)) {
            packetType = -1;
            return;
        }

        sceneCenterZoneX = zoneX;
        sceneCenterZoneZ = zoneZ;
        sceneBaseTileX = (sceneCenterZoneX - 6) * 8;
        sceneBaseTileZ = (sceneCenterZoneZ - 6) * 8;
        withinTutorialIsland = (((sceneCenterZoneX / 8) == 48) || ((sceneCenterZoneX / 8) == 49)) && ((sceneCenterZoneZ / 8) == 48);

        if (((sceneCenterZoneX / 8) == 48) && ((sceneCenterZoneZ / 8) == 148)) {
            withinTutorialIsland = true;
        }

        sceneState = 1;
        sceneLoadStartTime = SDL_GetTicks();

        areaViewport->Bind();
        fontPlain12->DrawStringCenter("Loading - please wait.", 257, 151, 0);
        fontPlain12->DrawStringCenter("Loading - please wait.", 256, 150, 0xffffff);
        areaViewport->Draw(drawSurface, 4, 4);
        PresentFrame();

        if (packetType == PacketIn::REBUILD_REGION) {
            FetchMaps();
        }

        if (packetType == PacketIn::REBUILD_REGION_INSTANCE) {
            FetchMapsInstanced();
        }

        ShiftScene();

        cutscene = false;
    }

    void Game::UpdateGame() {

        if (systemUpdateTimer > 1) {
            systemUpdateTimer--;
        }

        if (idleTimeout > 0) {
            idleTimeout--;
        }

        for (int32_t j = 0; j < 5; j++) {
            if (!Read()) {
                break;
            }
        }

        if (!ingame) {
            return;
        }

        //UpdateAnticheats();
        UpdateSceneState();
        UpdateTemporaryLocs();
        UpdateAudio();

        idleNetCycles++;

        if (idleNetCycles > 750) {
            TryReconnect();
        }

        UpdatePlayers();
        UpdateNPCs();
        UpdateEntityChats();
        delta++;

        if (crossMode != 0) {
            crossCycle += 20;
            if (crossCycle >= 400) {
                crossMode = 0;
            }
        }

        if (actionArea != 0) {
            actionCycles++;
            if (actionCycles >= 15) {
                if (actionArea == 2) {
                    redrawSidebar = true;
                }
                if (actionArea == 3) {
                    redrawChatback = true;
                }
                actionArea = 0;
            }
        }

        HandleObjDragging();

        if (Scene::clickTileX != -1)
        {
            int32_t x = Scene::clickTileX;
            int32_t z = Scene::clickTileZ;

            bool success = TryMove(0, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], x, z, 0, 0, 0, 0, 0, true);
            Scene::clickTileX = -1;

            if (success) {
                crossX = static_cast<int32_t>(mouseClickX);
                crossY = static_cast<int32_t>(mouseClickY);
                crossMode = 1;
                crossCycle = 0;
            }
        }

        if ((mouseClickButton == 1) && (!modalMessage.empty())) {
            modalMessage.clear();
            redrawChatback = true;
            mouseClickButton = 0;
        }

        HandleMouseInput();
        HandleMinimapInput();
        HandleTabInput();
        HandleChatSettingsInput();

        if ((mouseButton == 1) || (mouseClickButton == 1)) {
            dragCycles++;
        }

        UpdateCamera();
        HandleInputKey();
        UpdateIdleCycles();
        UpdateNetwork();
    }

    void Game::UpdateCamera() {
        if (sceneState == 2) {
            UpdateOrbitCamera();
        }

        if ((sceneState == 2) && cutscene) {
            ApplyCutscene();
        }

        for (int32_t type = 0; type < 5; type++) {
            CameraModifierCycle[type]++;
        }
    }

    void Game::UpdateNetwork()
    {
        heartbeatTimer++;
        if (heartbeatTimer > 50) {
            out.WriteOp(0);
        }
        bytesOut += out.position;

        if (connection && !connection->Closed() && out.position > 0) {
            connection->Write(out.data, 0, out.position);
            out.position = 0;
            heartbeatTimer = 0;
        } else if (!connection || connection->Closed())
        {
            TryReconnect();
        }
    }

    void Game::UpdateOrbitCamera() {
        int32_t orbitX = localPlayer->x; //+ cameraAnticheatOffsetX;
        int32_t orbitZ = localPlayer->z; //+ cameraAnticheatOffsetZ;

        if (((orbitCameraX - orbitX) < -500) || ((orbitCameraX - orbitX) > 500) || ((orbitCameraZ - orbitZ) < -500) || ((orbitCameraZ - orbitZ) > 500)) {
            orbitCameraX = orbitX;
            orbitCameraZ = orbitZ;
        }

        if (orbitCameraX != orbitX) {
            orbitCameraX += (orbitX - orbitCameraX) / 16;
        }

        if (orbitCameraZ != orbitZ) {
            orbitCameraZ += (orbitZ - orbitCameraZ) / 16;
        }

        if (actionKey[1] == 1) {
            orbitCameraYawVelocity += (-24 - orbitCameraYawVelocity) / 2;
        } else if (actionKey[2] == 1) {
            orbitCameraYawVelocity += (24 - orbitCameraYawVelocity) / 2;
        } else {
            orbitCameraYawVelocity /= 2;
        }

        if (actionKey[3] == 1) {
            orbitCameraPitchVelocity += (12 - orbitCameraPitchVelocity) / 2;
        } else if (actionKey[4] == 1) {
            orbitCameraPitchVelocity += (-12 - orbitCameraPitchVelocity) / 2;
        } else {
            orbitCameraPitchVelocity /= 2;
        }

        orbitCameraYaw = (orbitCameraYaw + (orbitCameraYawVelocity / 2)) & 0x7ff;
        orbitCameraPitch += orbitCameraPitchVelocity / 2;

        if (orbitCameraPitch < 128) {
            orbitCameraPitch = 128;
        }
        if (orbitCameraPitch > 383) {
            orbitCameraPitch = 383;
        }

        int32_t orbitTileX = orbitCameraX >> 7;
        int32_t orbitTileZ = orbitCameraZ >> 7;
        int32_t orbitY = GetHeightmapY(currentLevel, orbitCameraX, orbitCameraZ);
        int32_t maxY = 0;

        if ((orbitTileX > 3) && (orbitTileZ > 3) && (orbitTileX < 100) && (orbitTileZ < 100)) {
            for (int32_t x = orbitTileX - 4; x <= (orbitTileX + 4); x++) {
                for (int32_t z = orbitTileZ - 4; z <= (orbitTileZ + 4); z++) {
                    int32_t level = currentLevel;
                    if ((level < 3) && ((levelTileFlags[1][x][z] & 2) == 2)) {
                        level++;
                    }
                    int32_t y = orbitY - levelHeightmap[level][x][z];

                    if (y > maxY) {
                        maxY = y;
                    }
                }
            }
        }

        int32_t clamp = maxY * 192;

        if (clamp > 98048) {
            clamp = 98048;
        }

        if (clamp < 32768) {
            clamp = 32768;
        }

        if (clamp > cameraPitchClamp) {
            cameraPitchClamp += (clamp - cameraPitchClamp) / 24;
            return;
        }
        if (clamp < cameraPitchClamp) {
            cameraPitchClamp += (clamp - cameraPitchClamp) / 80;
        }
    }

    void Game::OrbitCamera(int32_t distance, int32_t pitch, int32_t targetX, int32_t targetY, int32_t yaw,
        int32_t targetZ) {
        int32_t invPitch = (2048 - pitch) & 0x7ff;
        int32_t invYaw = (2048 - yaw) & 0x7ff;
        int32_t x = 0;
        int32_t z = 0;
        int32_t y = distance;
        if (invPitch != 0) {
            int32_t sin = Draw3D::sin[invPitch];
            int32_t cos = Draw3D::cos[invPitch];
            int32_t tmp = ((z * cos) - (y * sin)) >> 16;
            y = ((z * sin) + (y * cos)) >> 16;
            z = tmp;
        }
        if (invYaw != 0) {
            int32_t sin = Draw3D::sin[invYaw];
            int32_t cos = Draw3D::cos[invYaw];
            int32_t tmp = ((y * sin) + (x * cos)) >> 16;
            y = ((y * cos) - (x * sin)) >> 16;
            x = tmp;
        }
        cameraX = targetX - x;
        cameraY = targetY - z;
        cameraZ = targetZ - y;
        cameraPitch = pitch;
        cameraYaw = yaw;
    }

    void Game::UpdateTitle() {
        if (CheckLoginStatus())
        {
            return;
        }
        if (titleScreenState == 0) {
            uint32_t x = (screenWidth / 2) - 80;
            uint32_t y = (screenHeight / 2) + 20;
            y += 20;

            if ((mouseClickButton == 1) && (mouseClickX >= (x - 75)) && (mouseClickX <= (x + 75)) && (mouseClickY >= (y - 20)) && (mouseClickY <= (y + 20))) {
                titleScreenState = 3;
                titleLoginField = 0;
            }

            x = (screenWidth / 2) + 80;

            if ((mouseClickButton == 1) && (mouseClickX >= (x - 75)) && (mouseClickX <= (x + 75)) && (mouseClickY >= (y - 20)) && (mouseClickY <= (y + 20))) {
                loginMessage0 = "";
                loginMessage1 = "Enter your username & password.";
                titleScreenState = 2;
                titleLoginField = 0;
            }
        } else {
            if (titleScreenState == 2) {
                int32_t fieldY = (screenHeight / 2) - 40;
                fieldY += 30;
                fieldY += 25;
                if ((mouseClickButton == 1) && (mouseClickY >= (fieldY - 15)) && (mouseClickY < fieldY)) {
                    titleLoginField = 0;
                }
                fieldY += 15;
                if ((mouseClickButton == 1) && (mouseClickY >= (fieldY - 15)) && (mouseClickY < fieldY)) {
                    titleLoginField = 1;
                }

                if (!hideLoginButtons) {
                    int32_t buttonX = (screenWidth / 2) - 80;
                    int32_t buttonY = (screenHeight / 2) + 50;
                    buttonY += 20;

                    if ((mouseClickButton == 1) && (mouseClickX >= (buttonX - 75)) && (mouseClickX <= (buttonX + 75)) && (mouseClickY >= (buttonY - 20)) && (mouseClickY <= (buttonY + 20))) {
                        loginAttempts = 0;
                        Login(username, password, false);
                        if (ingame) {
                            return;
                        }
                    }

                    buttonX = (screenWidth / 2) + 80;

                    if ((mouseClickButton == 1) && (mouseClickX >= (buttonX - 75)) && (mouseClickX <= (buttonX + 75)) && (mouseClickY >= (buttonY - 20)) && (mouseClickY <= (buttonY + 20))) {
                        titleScreenState = 0;
                        username = "";
                        password = "";
                    }
                }

                do {
                    int32_t key = PollKey();

                    if (key == -1) {
                        break;
                    }

                    bool valid = false;
                    for (int32_t i = 0; i < VALID_CHAT_CHARACTERS.length(); i++) {
                        if (key != VALID_CHAT_CHARACTERS[i]) {
                            continue;
                        }
                        valid = true;
                        break;
                    }

                    if (titleLoginField == 0) {
                        if ((key == 8) && (!username.empty())) {
                            username = username.substr(0, username.length() - 1);
                        }
                        if ((key == 9) || (key == 10) || (key == 13)) {
                            titleLoginField = 1;
                        }
                        if (valid) {
                            username += static_cast<char>(key);
                        }
                        if (username.length() > 12) {
                            username = username.substr(0, 12);
                        }
                    } else if (titleLoginField == 1) {
                        if ((key == 8) && (!password.empty())) {
                            password = password.substr(0, password.length() - 1);
                        }
                        if ((key == 9) || (key == 10) || (key == 13)) {
                            titleLoginField = 0;
                        }
                        if (valid) {
                            password += static_cast<char>(key);
                        }
                        if (password.length() > 20) {
                            password = password.substr(0, 20);
                        }
                    }
                } while (true);
                return;
            }
            if (titleScreenState == 3) {
                int32_t x = screenWidth / 2;
                int32_t y = (screenHeight / 2) + 50;
                y += 20;
                if ((mouseClickButton == 1) && (mouseClickX >= (x - 75)) && (mouseClickX <= (x + 75)) && (mouseClickY >= (y - 20)) && (mouseClickY <= (y + 20))) {
                    titleScreenState = 0;
                }
            }
        }
    }

    void Game::PrepareGameScreen() {
        if (areaChatback != nullptr) {
            return;
        }
        UnloadTitle();
        imageTitle2.reset();
        imageTitle3.reset();
        imageTitle4.reset();
        imageTitle0.reset();
        imageTitle1.reset();
        imageTitle5.reset();
        imageTitle6.reset();
        imageTitle7.reset();
        imageTitle8.reset();
        areaChatback = std::make_unique<DrawArea>(479, 96);
        areaMapback = std::make_unique<DrawArea>(172, 156);
        Draw2D::Clear();
        imageMapback->Blit(0, 0);
        areaSidebar = std::make_unique<DrawArea>(190, 261);
        areaViewport = std::make_unique<DrawArea>(viewportWidth, viewportHeight);
        Draw2D::Clear();
        areaBackbase1 = std::make_unique<DrawArea>(496, 50);
        areaBackbase2 = std::make_unique<DrawArea>(269, 37);
        areaBackhmid1 = std::make_unique<DrawArea>(249, 45);
        redrawTitleBackground = true;
    }

    void Game::Login(const std::string &usernameInput, const std::string &passwordInput, bool reconnect) {
        // Clean up any existing connection before starting a new one
        if (connection) {
            connection->Close();
            connection.reset();
            socket = nullptr;  // Connection destroyed the socket
        } else if (socket) {
            // Socket exists but no connection was created with it
            NET_DestroyStreamSocket(socket);
            socket = nullptr;
        }
        if (addr) {
            NET_UnrefAddress(addr);
            addr = nullptr;
        }

        if (!reconnect)
        {
            loginMessage0 = "";
            loginMessage1 = "Connecting to server...";
            hideLoginButtons = true;
        }
        // This is called later when socket connects
        pendingLoginTask = [=, this]
        {
            connection = std::make_unique<Connection>(socket);

            auto name37 = StringUtil::ToBase37(usernameInput);
            auto namePart = static_cast<int32_t>((name37 >> 16) & 31L);

            out.position = 0;
            out.Write8(14);
            out.Write8(namePart);

            connection->Write(out.data, 0, 2);

            for (int32_t j = 0; j < 8; j++) {
                connection->Read();
            }

            int32_t response = connection->Read();
            int32_t lastResponse = response;

            if (response == 0)
            {
                connection->Read(in.data, 0, 8);
                in.position = 0;
                serverSeed = in.Read64();
                std::vector<int32_t> seed(1 << 8);

                seed[0] = SDL_rand(100000000);
                seed[1] = SDL_rand(100000000);
                seed[2] = static_cast<int32_t>(serverSeed >> 32);
                seed[3] = static_cast<int32_t>(serverSeed);

                out.position = 0;
                out.Write8(10);
                out.Write32(seed[0]);
                out.Write32(seed[1]);
                out.Write32(seed[2]);
                out.Write32(seed[3]);
                out.Write32(Signlink::uid);
                out.WriteString(usernameInput);
                out.WriteString(passwordInput);
                out.Encrypt(RSA_EXPONENT, RSA_MODULUS);

                login.position = 0;

                login.Write8(reconnect ? 18 : 16);
                login.Write8(out.position + 36 + 1 + 1 + 2);
                login.Write8(255);
                login.Write16(317);
                login.Write8(lowmem ? 1 : 0);

                for (int32_t archive = 0; archive < 9; archive++) {
                    login.Write32(archiveChecksum[archive]);
                }

                login.Write(out.data, 0, out.position);

                out.random = ISAACRandom(seed);
                for (int32_t i = 0; i < 4; i++) {
                    seed[i] += 50;
                }
                randomIn = ISAACRandom(seed);

                connection->Write(login.data, 0, login.position);
                response = connection->Read();
                //connection->SetLagSimulation(10);
            }

            switch (response)
            {
            case 1:
                SDL_Delay(2000);
                Login(usernameInput, passwordInput, reconnect);
                return;
            case 2:
                rights = connection->Read();
                flagged = connection->Read() == 1;
                /*prevMousePressTime = 0L;
                lastWriteDuplicates = 0;
                mouseRecorder.length = 0;
                super.focused = true;
                _focused = true;*/
                ingame = true;
                out.position = 0;
                in.position = 0;
                packetType = -1;
                lastPacketType0 = -1;
                lastPacketType1 = -1;
                lastPacketType2 = -1;
                packetSize = 0;
                idleNetCycles = 0;
                systemUpdateTimer = 0;
                idleTimeout = 0;
                hintType = 0;
                menuSize = 0;
                menuVisible = false;
                idleCycles = 0;
                std::ranges::fill(messageText, std::string());
                objSelected = 0;
                spellSelected = 0;
                sceneState = 0;
                waveCount = 0;
                minimapAnticheatAngle = static_cast<int32_t>(SDL_randf() * 120.0f) - 60;
                minimapZoom = static_cast<int32_t>(SDL_randf() * 30.0f) - 20;
                orbitCameraYaw = static_cast<int>(SDL_randf() * 20.0f) - 10 & 0x7ff;
                minimapState = 0;
                minimapLevel = -1;
                flagSceneTileX = 0;
                flagSceneTileZ = 0;
                playerCount = 0;
                npcCount = 0;
                for (int32_t i = 0; i < MAX_PLAYER_COUNT; i++) {
                    players[i] = nullptr;
                    playerAppearanceBuffer[i].Clear();
                }
                for (int32_t i = 0; i < 16384; i++) {
                    npcs[i] = nullptr;
                }
                localPlayer = players[LOCAL_PLAYER_INDEX] = std::make_shared<PlayerEntity>();
                projectiles.clear();
                while (!spotanims.isEmpty()) {
                    delete spotanims.pollFront();
                }
                for (int32_t level = 0; level < 4; level++) {
                    for (int32_t x = 0; x < 104; x++) {
                        for (int32_t z = 0; z < 104; z++) {
                            levelObjStacks[level][x][z].clear();
                        }
                    }
                }
                while (!temporaryLocs.isEmpty()) {
                    delete temporaryLocs.pollFront();
                }
                friendlistStatus = 0;
                friendCount = 0;
                stickyChatInterfaceID = -1;
                chatInterfaceID = -1;
                viewportInterfaceID = -1;
                sidebarInterfaceID = -1;
                viewportOverlayInterfaceID = -1;
                pressedContinueOption = false;
                selectedTab = 3;
                chatbackInputType = 0;
                menuVisible = false;
                showSocialInput = false;
                modalMessage.clear();
                multizone = 0;
                flashingTab = -1;
                designGenderMale = true;
                ValidateCharacterDesign();
                for (int32_t i = 0; i < 5; i++) {
                    designColors[i] = 0;
                }
                for (int32_t i = 0; i < 5; i++) {
                    playerOptions[i] = "";
                    playerOptionPushDown[i] = false;
                }
                StopMidi();
                PrepareGameScreen();
                break;
            case 3:
                loginMessage0 = "";
                loginMessage1 = "Invalid username or password.";
                break;
            case 4:
                loginMessage0 = "Your account has been disabled.";
                loginMessage1 = "Please check your message-centre for details.";
                break;
            case 5:
                loginMessage0 = "Your account is already logged in.";
                loginMessage1 = "Try again in 60 secs...";
                break;
            case 6:
                loginMessage0 = "RuneScape has been updated!";
                loginMessage1 = "Please reload this page.";
                break;
            case 7:
                loginMessage0 = "This world is full.";
                loginMessage1 = "Please use a different world.";
                break;
            case 8:
                loginMessage0 = "Unable to connect.";
                loginMessage1 = "Login server offline.";
                break;
            case 9:
                loginMessage0 = "Login limit exceeded.";
                loginMessage1 = "Too many connections from your address.";
                break;
            case 10:
                loginMessage0 = "Unable to connect.";
                loginMessage1 = "Bad session id.";
                break;
            case 11:
                loginMessage0 = "Login server rejected session.";
                loginMessage1 = "Please try again.";
                break;
            case 12:
                loginMessage0 = "You need a members account to login to this world.";
                loginMessage1 = "Please subscribe, or use a different world.";
                break;
            case 13:
                loginMessage0 = "Could not complete login.";
                loginMessage1 = "Please try using a different world.";
                break;
            case 14:
                loginMessage0 = "The server is being updated.";
                loginMessage1 = "Please wait 1 minute and try again.";
                break;
            case 15:
                ingame = true;
                out.position = 0;
                in.position = 0;
                packetType = -1;
                lastPacketType0 = -1;
                lastPacketType1 = -1;
                lastPacketType2 = -1;
                packetSize = 0;
                idleNetCycles = 0;
                systemUpdateTimer = 0;
                menuSize = 0;
                menuVisible = false;
                sceneLoadStartTime = SDL_GetTicks();
                break;
            case 16:
                loginMessage0 = "Login attempts exceeded.";
                loginMessage1 = "Please wait 1 minute and try again.";
                break;
            case 17:
                loginMessage0 = "You are standing in a members-only area.";
                loginMessage1 = "To play on this world move to a free area first";
                break;
            case 20:
                loginMessage0 = "Invalid loginserver requested";
                loginMessage1 = "Please try using a different world.";
                break;
            case 21:
                for (int32_t remaining = connection->Read(); remaining >= 0; remaining--) {
                    loginMessage0 = "You have only just left another world";
                    loginMessage1 = "Your profile will be transferred in: " + std::to_string(remaining) + " seconds";
                    DrawTitleScreen(true);
                    SDL_Delay(1000);
                }
                Login(usernameInput, passwordInput, reconnect);
                break;
            case -1:
                if (lastResponse == 0) {
                    if (loginAttempts < 2) {
                        SDL_Delay(2000);
                        loginAttempts++;
                        Login(usernameInput, passwordInput, reconnect);
                    } else {
                        loginMessage0 = "No response from loginserver";
                        loginMessage1 = "Please wait 1 minute and try again.";
                    }
                } else {
                    loginMessage0 = "No response from server";
                    loginMessage1 = "Please try using a different world.";
                }
                break;
            default:
                loginMessage0 = "Unexpected server response";
                loginMessage1 = "Please try using a different world.";
                break;
            }

            // Mark disconnected for failed logins
            if (!ingame) {
                connectionState = ConnectionState::DISCONNECTED;
                hideLoginButtons = false;
            }
        };

        addr = NET_ResolveHostname(server.c_str());
        connectionState = ConnectionState::RESOLVING_DNS;
    }

    bool Game::CheckLoginStatus()
    {
        // DNS resolution
        if (connectionState == ConnectionState::RESOLVING_DNS) {
            auto status = NET_GetAddressStatus(addr);
            if (status == NET_FAILURE) {
                loginMessage0.clear();
                loginMessage1 = "Error connecting to server.";
                connectionState = ConnectionState::DISCONNECTED;
                hideLoginButtons = false;
                if (addr) {
                    NET_UnrefAddress(addr);
                    addr = nullptr;
                }
                return false;
            }
            if (status == NET_SUCCESS) {
                socket = NET_CreateClient(addr, port + portOffset);
                connectionState = ConnectionState::RESOLVING_SOCKET;
            }
            return false;
        }

        // Socket connection
        if (connectionState == ConnectionState::RESOLVING_SOCKET) {
            auto status = NET_GetConnectionStatus(socket);
            if (status == NET_FAILURE) {
                loginMessage0.clear();
                loginMessage1 = "Error connecting to server.";
                connectionState = ConnectionState::DISCONNECTED;
                hideLoginButtons = false;
                if (socket) {
                    NET_DestroyStreamSocket(socket);
                    socket = nullptr;
                }
                if (addr) {
                    NET_UnrefAddress(addr);
                    addr = nullptr;
                }
                return false;
            }
            if (status == NET_SUCCESS) {
                connectionState = ConnectionState::CONNECTED;
                if (pendingLoginTask) {
                    pendingLoginTask();
                    pendingLoginTask = nullptr;
                }
                return true;
            }
        }
        return false;
    }

    void Game::UnloadTitle() {
        StopFlames();
        imageTitlebox.reset();
        imageTitlebutton.reset();
        imageRunes->reset();
        flameGradient.fill(0);
        flameGradient0.fill(0);
        flameGradient1.fill(0);
        flameGradient2.fill(0);
        flameBuffer0.fill(0);
        flameBuffer1.fill(0);
        flameBuffer3.fill(0);
        flameBuffer2.fill(0);
        imageFlamesLeft.reset();
        imageFlamesRight.reset();
    }

    void Game::ClearCaches()
    {
        LocType::modelCacheStatic.clear();
        LocType::modelCacheDynamic.clear();
        NPCType::modelCache.clear();
        ObjType::modelCache.clear();
        ObjType::iconCache.clear();
        PlayerEntity::modelCache.clear();
        SpotAnimType::modelCache.clear();
    }

    void Game::BuildScene() {
        minimapLevel = -1;

        while (!spotanims.isEmpty()) {
            delete spotanims.pollFront();
        }
        projectiles.clear();

        Draw3D::ClearTexels();

        ClearCaches();
        scene->Reset();

        for (int32_t i = 0; i < 4; i++) {
            levelCollisionMap[i]->Reset();
        }

        ClearTileFlags();

        SceneBuilder builder(levelTileFlags, 104, 104, levelHeightmap);

        out.WriteOp(0);

        if (sceneInstanced) {
            BuildSceneInstanced(builder);
        } else {
            BuildSceneStandard(builder);
        }

        out.WriteOp(0);

        builder.Build(levelCollisionMap, *scene);
        areaViewport->Bind();

        out.WriteOp(0);

        if (lowmem) {
            scene->SetMinLevel(SceneBuilder::minLevel);
        } else {
            scene->SetMinLevel(0);
        }
        for (int32_t x = 0; x < 104; x++) {
            for (int32_t z = 0; z < 104; z++) {
                SortObjStacks(x, z);
            }
        }

        ClearTemporaryLocs();

        if (window != nullptr) {
            out.WriteOp(210);
            out.Write32(0x3F008EDD);
        }

        if (lowmem && !Signlink::cacheDatPath.empty()) {
            int32_t count = ondemand->GetFileCount(0);
            for (int32_t i = 0; i < count; i++) {
                if ((ondemand->GetModelFlags(i) & 0x79) == 0) {
                    Model::Unload(i);
                }
            }
        }

        Draw3D::InitPool(20);
        ondemand->ClearPrefetches();

        int32_t minMapX = ((sceneCenterZoneX - 6) / 8) - 1;
        int32_t maxMapX = ((sceneCenterZoneX + 6) / 8) + 1;
        int32_t minMapZ = ((sceneCenterZoneZ - 6) / 8) - 1;
        int32_t maxMapZ = ((sceneCenterZoneZ + 6) / 8) + 1;

        if (withinTutorialIsland) {
            minMapX = 49;
            maxMapX = 50;
            minMapZ = 49;
            maxMapZ = 50;
        }

        for (int32_t mapX = minMapX; mapX <= maxMapX; mapX++) {
            for (int32_t mapZ = minMapZ; mapZ <= maxMapZ; mapZ++) {
                if ((mapX == minMapX) || (mapX == maxMapX) || (mapZ == minMapZ) || (mapZ == maxMapZ)) {
                    int32_t mapFile = ondemand->GetMapFile(0, mapX, mapZ);
                    if (mapFile != -1) {
                        ondemand->Prefetch(mapFile, 3);
                    }
                    int32_t locFile = ondemand->GetMapFile(1, mapX, mapZ);
                    if (locFile != -1) {
                        ondemand->Prefetch(locFile, 3);
                    }
                }
            }
        }
    }

    void Game::FetchMaps() {
        int32_t mapCount = 0;

        for (int32_t x = (sceneCenterZoneX - 6) / 8; x <= ((sceneCenterZoneX + 6) / 8); x++) {
            for (int32_t z = (sceneCenterZoneZ - 6) / 8; z <= ((sceneCenterZoneZ + 6) / 8); z++) {
                mapCount++;
            }
        }

        sceneMapLandData.resize(mapCount);
        sceneMapLocData.resize(mapCount);
        sceneMapIndex.resize(mapCount);
        sceneMapLandFile.resize(mapCount);
        sceneMapLocFile.resize(mapCount);

        mapCount = 0;
        for (int32_t mx = (sceneCenterZoneX - 6) / 8; mx <= ((sceneCenterZoneX + 6) / 8); mx++) {
            for (int32_t mz = (sceneCenterZoneZ - 6) / 8; mz <= ((sceneCenterZoneZ + 6) / 8); mz++) {
                sceneMapIndex[mapCount] = (mx << 8) + mz;
                if (withinTutorialIsland && ((mz == 49) || (mz == 149) || (mz == 147) || (mx == 50) || ((mx == 49) && (mz == 47)))) {
                    sceneMapLandFile[mapCount] = -1;
                    sceneMapLocFile[mapCount] = -1;
                } else {
                    int32_t landFile = sceneMapLandFile[mapCount] = ondemand->GetMapFile(0, mx, mz);

                    if (landFile != -1) {
                        ondemand->Request(3, landFile);
                    }

                    int32_t locFile = sceneMapLocFile[mapCount] = ondemand->GetMapFile(1, mx, mz);

                    if (locFile != -1) {
                        ondemand->Request(3, locFile);
                    }
                }
                mapCount++;
            }
        }
    }

    void Game::FetchMapsInstanced()
    {
        int32_t mapCount = 0;
        std::array<int32_t, 676> mapIDs{};

        for (int32_t level = 0; level < 4; level++) {
            for (int32_t x = 0; x < 13; x++) {
                for (int32_t z = 0; z < 13; z++) {
                    int32_t bitset = levelChunkBitset[level][x][z];

                    if (bitset != -1) {
                        int32_t mapX = (bitset >> 14) & 0x3ff;
                        int32_t mapZ = (bitset >> 3) & 0x7ff;
                        int32_t mapID = ((mapX / 8) << 8) + (mapZ / 8);

                        for (int32_t i = 0; i < mapCount; i++) {
                            if (mapIDs[i] != mapID) {
                                continue;
                            }
                            mapID = -1;
                            break;
                        }

                        if (mapID != -1) {
                            mapIDs[mapCount++] = mapID;
                        }
                    }
                }
            }
        }

        sceneMapLandData.resize(mapCount);
        sceneMapLocData.resize(mapCount);
        sceneMapIndex.resize(mapCount);
        sceneMapLandFile.resize(mapCount);
        sceneMapLocFile.resize(mapCount);

        for (int32_t i = 0; i < mapCount; i++) {
            int32_t mapIndex = sceneMapIndex[i] = mapIDs[i];
            int32_t mapX = (mapIndex >> 8) & 0xff;
            int32_t mapZ = mapIndex & 0xff;

            int32_t mapLandFile = sceneMapLandFile[i] = ondemand->GetMapFile(0, mapX, mapZ);

            if (mapLandFile != -1) {
                ondemand->Request(3, mapLandFile);
            }

            int32_t mapLocFile = sceneMapLocFile[i] = ondemand->GetMapFile(1, mapX, mapZ);

            if (mapLocFile != -1) {
                ondemand->Request(3, mapLocFile);
            }
        }
    }

    void Game::UpdateFlameBuffer(const std::shared_ptr<Image8>& image) {
        std::ranges::fill(flameBuffer0, 0);

        for (int32_t l = 0; l < 5000; l++) {
            flameBuffer0[SDL_rand(128 * 256)] = SDL_rand(256);
        }

        for (int32_t i = 0; i < 20; i++) {
            for (int32_t y = 1; y < 255; y++) {
                for (int32_t x = 1; x < 127; x++) {
                    const int32_t pos = x + (y << 7);
                    flameBuffer1[pos] = (flameBuffer0[pos - 1] + flameBuffer0[pos + 1] + flameBuffer0[pos - 128] + flameBuffer0[pos + 128]) / 4;
                }
            }

            std::swap(flameBuffer0, flameBuffer1);
        }

        // erase masked pixels from image
        if (image != nullptr && image->surface != nullptr) {
            const auto* pixels = static_cast<int32_t*>(image->surface->pixels);
            int32_t pixelsPerRow = image->surface->pitch / sizeof(int32_t);
            for (int32_t y = 0; y < image->height; y++) {
                for (int32_t x = 0; x < image->width; x++) {
                    int32_t pixel = pixels[y * pixelsPerRow + x];
                    if (pixel != 0) {
                        const int32_t dstX = x + 16 + image->cropX;
                        const int32_t dstY = y + 16 + image->cropY;
                        flameBuffer0[dstX + (dstY << 7)] = 0;
                    }
                }
            }
        }
    }

    void Game::DrawTitleScreen(bool hideButtons) {
        LoadTitle();

        if (SDL_GetAtomicInt(&flameActive)) {
            uint64_t now = SDL_GetTicks();
            if (now - lastFlameUpdate >= 20) {
                UpdateFlames();
                lastFlameUpdate = now;
            }
            DrawFlames();
        }

        imageTitle4->Bind();
        imageTitlebox->Blit(0, 0);

        int32_t w = 360;
        int32_t h = 200;

        if (titleScreenState == 0) {
            int32_t y = (h / 2) + 80;
            fontPlain11->DrawStringTaggableCenter(ondemand->GetMessage(), w / 2, y, 0x75a9a9, true);

            y = (h / 2) - 20;
            fontBold12->DrawStringTaggableCenter("Welcome to RuneScape", w / 2, y, 0xffff00, true);

            int32_t x = (w / 2) - 80;
            y = (h / 2) + 20;
            imageTitlebutton->Blit(x - 73, y - 20);
            fontBold12->DrawStringTaggableCenter("New User", x, y + 5, 0xffffff, true);

            x = (w / 2) + 80;
            imageTitlebutton->Blit(x - 73, y - 20);
            fontBold12->DrawStringTaggableCenter("Existing User", x, y + 5, 0xffffff, true);

        }

        if (titleScreenState == 2) {
            int32_t y = (h / 2) - 40;

            if (!loginMessage0.empty()) {
                fontBold12->DrawStringTaggableCenter(loginMessage0, w / 2, y - 15, 0xffff00, true);
                fontBold12->DrawStringTaggableCenter(loginMessage1, w / 2, y, 0xffff00, true);
            } else {
                fontBold12->DrawStringTaggableCenter(loginMessage1, w / 2, y - 7, 0xffff00, true);
            }
            y += 30;

            fontBold12->DrawStringTaggable("Username: " + username + ((titleLoginField == 0 & loopCycle % 40 < 20) ? "@yel@|" : ""), (w / 2) - 90, y, 0xffffff, true);
            y += 15;

            fontBold12->DrawStringTaggable("Password: " + StringUtil::ToAsterisks(password) + ((titleLoginField == 1 & loopCycle % 40 < 20) ? "@yel@|" : ""), (w / 2) - 88, y, 0xffffff, true);

            if (!hideButtons) {
                int32_t x = (w / 2) - 80;
                y = (h / 2) + 50;
                imageTitlebutton->Blit(x - 73, y - 20);
                fontBold12->DrawStringTaggableCenter("Login", x, y + 5, 0xffffff, true);

                x = (w / 2) + 80;
                imageTitlebutton->Blit(x - 73, y - 20);
                fontBold12->DrawStringTaggableCenter("Cancel", x, y + 5, 0xffffff, true);
            }
        }

        if (titleScreenState == 3) {
            fontBold12->DrawStringTaggableCenter("Create a free account", w / 2, (h / 2) - 60, 0xffff00, true);

            int32_t y = (h / 2) - 35;
            fontBold12->DrawStringTaggableCenter("To create a new account you need to", w / 2, y, 0xffffff, true);
            y += 15;

            fontBold12->DrawStringTaggableCenter("go back to the main RuneScape webpage", w / 2, y, 0xffffff, true);
            y += 15;

            fontBold12->DrawStringTaggableCenter("and choose the red 'create account'", w / 2, y, 0xffffff, true);
            y += 15;

            fontBold12->DrawStringTaggableCenter("button at the top right of that page.", w / 2, y, 0xffffff, true);

            int32_t x = w / 2;
            y = (h / 2) + 50;
            imageTitlebutton->Blit(x - 73, y - 20);
            fontBold12->DrawStringTaggableCenter("Cancel", x, y + 5, 0xffffff, true);
        }

        imageTitle4->Draw(drawSurface, 202, 171);

        if (redrawTitleBackground) {
            redrawTitleBackground = false;
            imageTitle2->Draw(drawSurface, 128, 0);
            imageTitle3->Draw(drawSurface, 202, 371);
            imageTitle5->Draw(drawSurface, 0, 265);
            imageTitle6->Draw(drawSurface, 562, 265);
            imageTitle7->Draw(drawSurface, 128, 171);
            imageTitle8->Draw(drawSurface, 562, 171);
        }

        PresentFrame();
    }

    int32_t Game::Mix(const int32_t src, const int32_t dst, const int32_t alpha) {
        const int32_t invAlpha = 256 - alpha;

        //                 0xAARRGGBB
        const int32_t srcRB = (src & 0x00FF00FF) * invAlpha; // mul R and B (invAlpha of 256 is the same as << 8)
        //                 0xRRR0BBB0
        // This multiplication causes the channels to shift. Since it most likely shifts on the magnitude of bits and
        // not bytes, this will cause the channels to be contained within 2 bytes. You can imagine the first byte being
        // its whole value, and the second byte being its 'fractional' value. Like a fixed-integer.

        //                 0xAARRGGBB
        const int32_t srcG = (src & 0x0000FF00) * invAlpha; // mul G
        //                 0x00GGG000
        // same as above but with only a single channel

        // same as above, just with alpha instead of invAlpha
        const int32_t dstRB = (dst & 0x00FF00FF) * alpha;
        const int32_t dstG = (dst & 0x0000FF00) * alpha;

        // Then we add the products together and trim off the fractional bits. Now it's just the Red and Blue channels
        // left shifted by 8.           0xRRR0BBB0
        //                      becomes 0xRR00BB00
        const int32_t finalRB = (srcRB + dstRB) & 0xFF00FF00;

        //                           0x00GGG000
        //                   becomes 0x00GG0000
        const int32_t finalG = (srcG + dstG) & 0x00FF0000;

        // Now we just add the channels back to each other
        //                      0xRR00BB00
        //                    + 0x00GG0000
        //                    = 0xRRGGBB00
        // And the result is our RGB channels left shifted by 8.
        return (finalRB + finalG) >> 8;
    }

    void Game::HandleOnDemandRequests() {
        do {
            OnDemandRequest* request = ondemand->Poll();
            if (request == nullptr || request->data.empty()) {
                return;
            }
            int32_t store = request->store;
            int32_t file = request->file;
            std::vector<int8_t>& data = request->data;

            if (store == 0) {
                Model::Unpack(data, file);

                if ((ondemand->GetModelFlags(file) & 0x62) != 0) {
                    redrawSidebar = true;
                    if (chatInterfaceID != -1) {
                        redrawChatback = true;
                    }
                }
            } else if (store == 1) {
                SeqTransform::Unpack(data);
            } else if (store == 2) {
                if ((file == song) && (!data.empty())) {
                    PlayMidi(songFading, data);
                }
            } else if (store == 3) {
                if (sceneState == 1) {
                    for (size_t i = 0; i < sceneMapLandData.size(); i++) {
                        if (sceneMapLandFile[i] == file) {
                            sceneMapLandData[i] = data;

                            if (data.empty()) {
                                sceneMapLandFile[i] = -1;
                            }
                            break;
                        }

                        if (sceneMapLocFile[i] == file) {
                            sceneMapLocData[i] = data;

                            if (data.empty()) {
                                sceneMapLocFile[i] = -1;
                            }
                            break;
                        }
                    }
                }
            } else if (store == 93) {
                if (ondemand->HasMapLocFile(file)) {
                    SceneBuilder::PrefetchLocs(std::make_shared<Buffer>(data), *ondemand);
                }
            }
        } while (true);
    }

    void Game::UseWalkHereOption(int32_t mouseX, int32_t mouseY)
    {
        if (!menuVisible) {
            scene->Click(static_cast<int32_t>(mouseClickY) - 4, static_cast<int32_t>(mouseClickX) - 4);
        } else {
            scene->Click(mouseY - 4, mouseX - 4);
        }
    }

    void Game::HandleMouseInput()
    {
        if (objDragArea != 0) {
            return;
        }

        int32_t button = mouseClickButton;

        if ((spellSelected == 1) && (mouseClickX >= 516) && (mouseClickY >= 160) && (mouseClickX <= 765) && (mouseClickY <= 205)) {
            button = 0;
        }

        if (menuVisible) {
            HandleMenuInput(button);
        } else
        {
            if ((button == 1) && (menuSize > 0)) {
                int32_t action = menuAction[menuSize - 1];
                switch (action) {
                    case 632:
                    case 78:
                    case 867:
                    case 431:
                    case 53:
                    case 74:
                    case 454:
                    case 539:
                    case 493:
                    case 847:
                    case 447:
                    case 1125: {
                        int32_t objSlot = menuParamA[menuSize - 1];
                        int32_t interfaceID = menuParamB[menuSize - 1];
                        const auto& iface = IfType::instances[interfaceID];

                        if (iface->inventoryDraggable || iface->inventoryMoveReplaces) {
                            objGrabThreshold = false;
                            objDragCycles = 0;
                            objDragInterfaceID = interfaceID;
                            objDragSlot = objSlot;
                            objDragArea = 2;
                            objGrabX = static_cast<int32_t>(mouseClickX);
                            objGrabY = static_cast<int32_t>(mouseClickY);
                            if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
                                objDragArea = 1;
                            }
                            if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
                                objDragArea = 3;
                            }
                            return;
                        }
                        break;
                }
                }
            }

            if ((button == 1) && ((mouseButtonsOption == 1) || IsAddFriendOption(menuSize - 1)) && (menuSize > 2)) {
                button = 2;
            }
            if (button == 1 && (menuSize > 0)) {
                UseMenuOption(menuSize - 1);
            }

            if ((button == 2) && (menuSize > 0)) {
                ShowContextMenu();
            }
        }
    }

    void Game::HandleMinimapInput()
    {
        if (minimapState != 0) {
            return;
        }

        if (mouseClickButton != 1) {
            return;
        }

        int32_t x = static_cast<int32_t>(mouseClickX) - 25 - 550;
        int32_t y = static_cast<int32_t>(mouseClickY) - 5 - 4;

        if ((x < 0) || (y < 0) || (x >= 146) || (y >= 151)) {
            return;
        }

        x -= 73;
        y -= 75;

        int32_t yaw = (orbitCameraYaw + minimapAnticheatAngle) & 0x7ff;
        int32_t sinYaw = Draw3D::sin[yaw];
        int32_t cosYaw = Draw3D::cos[yaw];

        sinYaw = (sinYaw * (minimapZoom + 256)) >> 8;
        cosYaw = (cosYaw * (minimapZoom + 256)) >> 8;

        int32_t relativeX = ((y * sinYaw) + (x * cosYaw)) >> 11;
        int32_t relativeZ = ((y * cosYaw) - (x * sinYaw)) >> 11;

        int32_t tileX = (localPlayer->x + relativeX) >> 7;
        int32_t tileZ = (localPlayer->z - relativeZ) >> 7;

        bool ok = TryMove(1, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], tileX, tileZ,
            0, 0, 0, 0, 0, true);

        if (ok) {
            out.Write8(x);
            out.Write8(y);
            out.Write16(orbitCameraYaw);
            out.Write8(57);
            out.Write8(minimapAnticheatAngle);
            out.Write8(minimapZoom);
            out.Write8(89);
            out.Write16(localPlayer->x);
            out.Write16(localPlayer->z);
            out.Write8(tryMoveNearest);
            out.Write8(63);
        }
    }

    void Game::HandleTabInput()
    {
        if (mouseClickButton == 1) {
            if ((mouseClickX >= 539) && (mouseClickX <= 573) && (mouseClickY >= 169) && (mouseClickY < 205) && (tabInterfaceID[0] != -1)) {
                redrawSidebar = true;
                selectedTab = 0;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 569) && (mouseClickX <= 599) && (mouseClickY >= 168) && (mouseClickY < 205) && (tabInterfaceID[1] != -1)) {
                redrawSidebar = true;
                selectedTab = 1;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 597) && (mouseClickX <= 627) && (mouseClickY >= 168) && (mouseClickY < 205) && (tabInterfaceID[2] != -1)) {
                redrawSidebar = true;
                selectedTab = 2;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 625) && (mouseClickX <= 669) && (mouseClickY >= 168) && (mouseClickY < 203) && (tabInterfaceID[3] != -1)) {
                redrawSidebar = true;
                selectedTab = 3;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 666) && (mouseClickX <= 696) && (mouseClickY >= 168) && (mouseClickY < 205) && (tabInterfaceID[4] != -1)) {
                redrawSidebar = true;
                selectedTab = 4;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 694) && (mouseClickX <= 724) && (mouseClickY >= 168) && (mouseClickY < 205) && (tabInterfaceID[5] != -1)) {
                redrawSidebar = true;
                selectedTab = 5;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 722) && (mouseClickX <= 756) && (mouseClickY >= 169) && (mouseClickY < 205) && (tabInterfaceID[6] != -1)) {
                redrawSidebar = true;
                selectedTab = 6;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 540) && (mouseClickX <= 574) && (mouseClickY >= 466) && (mouseClickY < 502) && (tabInterfaceID[7] != -1)) {
                redrawSidebar = true;
                selectedTab = 7;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 572) && (mouseClickX <= 602) && (mouseClickY >= 466) && (mouseClickY < 503) && (tabInterfaceID[8] != -1)) {
                redrawSidebar = true;
                selectedTab = 8;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 599) && (mouseClickX <= 629) && (mouseClickY >= 466) && (mouseClickY < 503) && (tabInterfaceID[9] != -1)) {
                redrawSidebar = true;
                selectedTab = 9;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 627) && (mouseClickX <= 671) && (mouseClickY >= 467) && (mouseClickY < 502) && (tabInterfaceID[10] != -1)) {
                redrawSidebar = true;
                selectedTab = 10;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 669) && (mouseClickX <= 699) && (mouseClickY >= 466) && (mouseClickY < 503) && (tabInterfaceID[11] != -1)) {
                redrawSidebar = true;
                selectedTab = 11;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 696) && (mouseClickX <= 726) && (mouseClickY >= 466) && (mouseClickY < 503) && (tabInterfaceID[12] != -1)) {
                redrawSidebar = true;
                selectedTab = 12;
                redrawSideicons = true;
            }
            if ((mouseClickX >= 724) && (mouseClickX <= 758) && (mouseClickY >= 466) && (mouseClickY < 502) && (tabInterfaceID[13] != -1)) {
                redrawSidebar = true;
                selectedTab = 13;
                redrawSideicons = true;
            }
        }
    }

    void Game::HandleChatMouseInput(int32_t mouseY)
    {
        int32_t line = 0;
        for (int32_t i = 0; i < 100; i++) {
            if (messageText[i].empty()) {
                continue;
            }

            int32_t type = messageType[i];
            int32_t y = (70 - (line * 14)) + chatScrollOffset + 4;

            if (y < -20) {
                break;
            }

            auto s = messageSender[i];

            if ((!s.empty()) && s.starts_with("@cr1@")) {
                s = s.substr(5);
            }

            if ((!s.empty()) && s.starts_with("@cr2@")) {
                s = s.substr(5);
            }

            if (type == 0) {
                line++;
            }

            if (((type == 1) || (type == 2)) && ((type == 1) || (publicChatSetting == 0) || ((publicChatSetting == 1) && IsFriend(s)))) {
                if ((mouseY > (y - 14)) && (mouseY <= y) && s != localPlayer->name) {
                    if (rights >= 1) {
                        AddMenuOption("Report abuse @whi@" + s, 606);
                    }
                    AddMenuOption("Add ignore @whi@" + s, 42);
                    AddMenuOption("Add friend @whi@" + s, 337);
                }
                line++;
            }

            if (((type == 3) || (type == 7)) && (splitPrivateChat == 0) && ((type == 7) || (privateChatSetting == 0) || ((privateChatSetting == 1) && IsFriend(s)))) {
                if ((mouseY > (y - 14)) && (mouseY <= y)) {
                    if (rights >= 1) {
                        AddMenuOption("Report abuse @whi@" + s, 606);
                    }
                    AddMenuOption("Add ignore @whi@" + s, 42);
                    AddMenuOption("Add friend @whi@" + s, 337);
                }
                line++;
            }

            if ((type == 4) && ((tradeChatSetting == 0) || ((tradeChatSetting == 1) && IsFriend(s)))) {
                if ((mouseY > (y - 14)) && (mouseY <= y)) {
                    AddMenuOption("Accept trade @whi@" + s, 484);
                }
                line++;
            }

            if (((type == 5) || (type == 6)) && (splitPrivateChat == 0) && (privateChatSetting < 2)) {
                line++;
            }

            if ((type == 8) && ((tradeChatSetting == 0) || ((tradeChatSetting == 1) && IsFriend(s)))) {
                if ((mouseY > (y - 14)) && (mouseY <= y)) {
                    AddMenuOption("Accept challenge @whi@" + s, 6);
                }
                line++;
            }
        }
    }

    void Game::UseMenuOption(int32_t optionID)
    {
        if (optionID < 0) {
            return;
        }

        if (chatbackInputType != 0) {
            chatbackInputType = 0;
            redrawChatback = true;
        }

        int32_t action = menuAction[optionID];
        int32_t a = menuParamA[optionID];
        int32_t b = menuParamB[optionID];
        int32_t c = menuParamC[optionID];

        if (action >= 2000) {
            action -= 2000;
        }

        switch (action) {
        case 582:
            SendUseObjOnNPC(c);
            break;
        case 234:
            UseGroundObjOption2(a, b, c);
            break;
        case 62:
            if (InteractWithLoc(c, a, b)) {
                UseObjOnLoc(a, b, c);
            }
            break;
        case 511:
            UseObjOnGroundObj(a, b, c);
            break;
        case 74:
            UseObjOption0(a, b, c);
            break;
        case 315:
            UseButton(b);
            break;
        case 561:
            UsePlayerOption0(c);
            break;
        case 20:
            UseNPCOption0(c);
            break;
        case 779:
            UsePlayerOption1(c);
            break;
        case 516:
            UseWalkHereOption(a, b);
            break;
        case 1062:
            UseLocOption4(a, b, c);
            break;
        case 679:
            if (!pressedContinueOption) {
                out.WriteOp(40);
                out.Write16(b);
                pressedContinueOption = true;
            }
            break;
        case 431:
            UseInventoryOption3(a, b, c);
            break;
        case 337:
        case 42:
        case 792:
        case 322: {
            auto ption = menuOption[optionID];
            auto tag = ption.find("@whi@");
            if (tag != std::string::npos) {
                int64_t name = StringUtil::ToBase37(StringUtil::Trim(ption.substr(0, tag + 5)));

                if (action == 337) {
                    AddFriend(name);
                }
                if (action == 42) {
                    AddIgnore(name);
                }
                if (action == 792) {
                    RemoveFriend(name);
                }
                if (action == 322) {
                    RemoveIgnore(name);
                }
            }
            break;
        }
        case 53:
            UseInventoryOption4(a, b, c);
            break;
        case 539:
            UseObjOption2(a, b, c);
            break;
        case 484:
        case 6: {
            auto option = menuOption[optionID];
            int32_t tag = option.find("@whi@");

            if (tag != -1) {
                option = StringUtil::Trim(option.substr(0, tag + 5));
                AcceptPlayerRequest(action, StringUtil::FormatName(StringUtil::FromBase37(StringUtil::ToBase37(option))));
            }
            break;
        }
        case 870:
            UseObjOnObj(a, b, c);
            break;
        case 847:
            UseObjOption4(a, b, c);
            break;
        case 626:
            SelectSpell(b);
            return;
        case 78:
            UseInventoryOption1(a, b, c);
            break;
        case 27:
            UsePlayerOption2(c);
            break;
        case 213:
            UseGroundObjOption4(a, b, c);
            break;
        case 632:
            UseInventoryOption0(a, b, c);
            break;
        case 493:
            UseObjOption3(a, b, c);
            break;
        case 652:
            UseGroundObjOption0(a, b, c);
            break;
        case 94:
            CastSpellOnGroundObj(a, b, c);
            break;
        case 646:
            UseSelectOption(b);
            break;
        case 225:
            UseNPCOption2(c);
            break;
        case 965:
            UseNPCOption3(c);
            break;
        case 413:
            CastSpellOnNPC(c);
            break;
        case 200:
            CloseInterfaces();
            break;
        case 1025:
            ExamineNPC(c);
            break;
        case 900:
            UseLocOption1(a, b, c);
            break;
        case 412:
            UseNPCOption1(c);
            break;
        case 365:
            CastSpellOnPlayer(c);
            break;
        case 729:
            UsePlayerOption4(c);
            break;
        case 577:
            UsePlayerOption3(c);
            break;
        case 956:
            if (InteractWithLoc(c, a, b)) {
                CastSpellOnLoc(a, b, c);
            }
            break;
        case 567:
            UseGroundObjOption1(a, b, c);
            break;
        case 867:
            UseInventoryOption2(a, b, c);
            break;
        case 543:
            CastSpellOnObj(a, b, c);
            break;
        case 606: {
            const auto& option = menuOption[optionID];
            int32_t tag = option.find("@whi@");
            if (tag != -1) {
                UseReportAbuseOption(StringUtil::Trim(option.substr(0, tag + 5)));
            }
            break;
        }
        case 491:
            UseObjOnPlayer(c);
            break;
        case 639: {
            auto option = menuOption[optionID];
            int32_t tag = option.find("@whi@");
            if (tag != -1) {
                PromptMessageFriend(StringUtil::ToBase37(StringUtil::Trim(option.substr(0, tag + 5))));
            }
            break;
        }
        case 454:
            UseObjOption1(a, b, c);
            break;
        case 478:
            UseNPCOption4(c);
            break;
        case 113:
            UseLocOption2(a, b, c);
            break;
        case 872:
            UseLocOption3(a, b, c);
            break;
        case 502:
            UseLocOption0(a, b, c);
            break;
        case 1125:
            ExamineInventoryObj(a, b, c);
            break;
        case 169:
            UseToggleOption(b);
            break;
        case 447:
            SelectObj(a, b, c);
            return;
        case 1226:
            ExamineLoc(c);
            break;
        case 244:
            UseGroundObjOption3(a, b, c);
            break;
        case 1448:
            ExamineObj(c);
            break;
        }

        objSelected = 0;
        spellSelected = 0;
        redrawSidebar = true;

    }

    void Game::SendUseObjOnNPC(int32_t npcID)
    {
        const auto& npc = npcs[npcID];
        if (npc == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], npc->pathTileX[0],
            npc->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(57);
        out.Write16A(selectedObjID);
        out.Write16A(npcID);
        out.Write16LE(selectedObjSlot);
        out.Write16A(selectedObjInterfaceID);
    }

    void Game::UseGroundObjOption0(int32_t tileX, int32_t tileZ, int32_t objID)
    {
        bool ok = TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0],
            tileX, tileZ, 0, 0, 0, 0, 0, false);
        if (!ok) {
            TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0],
                tileX, tileZ, 0, 1, 1, 0, 0, false);
        }
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(156);
        out.Write16A(tileX + sceneBaseTileX);
        out.Write16LE(tileZ + sceneBaseTileZ);
        out.Write16LEA(objID);
    }

    void Game::UseGroundObjOption1(int32_t tileX, int32_t tileZ, int32_t objID)
    {
        bool ok = TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0],
            tileX, tileZ, 0, 0, 0, 0, 0, false);
        if (!ok) {
            TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0],
                tileX, tileZ, 0, 1, 1, 0, 0, false);
        }
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(23);
        out.Write16LE(tileZ + sceneBaseTileZ);
        out.Write16LE(objID);
        out.Write16LE(tileX + sceneBaseTileX);
    }

    void Game::UseGroundObjOption2(int32_t tileX, int32_t tileZ, int32_t objID)
    {
        bool ok = TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0],
            tileX, tileZ, 0, 0, 0, 0, 0, false);
        if (!ok) {
            TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], tileX, tileZ, 0, 1, 1, 0, 0, false);
        }
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(236);
        out.Write16LE(tileZ + sceneBaseTileZ);
        out.Write16(objID);
        out.Write16LE(tileX + sceneBaseTileX);
    }

    void Game::UseGroundObjOption3(int32_t tileX, int32_t tileZ, int32_t objID)
    {
        bool ok = TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], tileX, tileZ,
            0, 0, 0, 0, 0, false);
        if (!ok) {
            TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], tileX, tileZ,
                0, 1, 1, 0, 0, false);
        }
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(253);
        out.Write16LE(tileX + sceneBaseTileX);
        out.Write16LEA(tileZ + sceneBaseTileZ);
        out.Write16A(objID);
    }

    void Game::UseGroundObjOption4(int32_t tileX, int32_t tileZ, int32_t objID)
    {
        bool ok = TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], tileX, tileZ,
            0, 0, 0, 0, 0, false);
        if (!ok) {
            TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], tileX, tileZ,
                0, 1, 1, 0, 0, false);
        }
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(79);
        out.Write16LE(tileZ + sceneBaseTileZ);
        out.Write16(objID);
        out.Write16A(tileX + sceneBaseTileX);
    }

    void Game::UseObjOnPlayer(int32_t playerID)
    {
        const auto& player = players[playerID];
        if (player == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], player->pathTileX[0],
            player->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(14);
        out.Write16A(selectedObjInterfaceID);
        out.Write16(playerID);
        out.Write16(selectedObjID);
        out.Write16LE(selectedObjSlot);
    }

    void Game::CastSpellOnObj(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        out.WriteOp(237);
        out.Write16(slot);
        out.Write16A(objID);
        out.Write16(interfaceID);
        out.Write16A(activeSpellID);
        actionCycles = 0;
        actionInterfaceID = interfaceID;
        actionSlot = slot;
        actionArea = 2;
        if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
            actionArea = 1;
        }
        if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
            actionArea = 3;
        }
    }

    void Game::CastSpellOnGroundObj(int32_t tileX, int32_t tileZ, int32_t objID)
    {
        bool ok = TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], tileX, tileZ,
            0, 0, 0, 0, 0, false);
        if (!ok) {
            TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], tileX, tileZ,
                0, 1, 1, 0, 0, false);
        }
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(181);
        out.Write16LE(tileZ + sceneBaseTileZ);
        out.Write16(objID);
        out.Write16LE(tileX + sceneBaseTileX);
        out.Write16A(activeSpellID);
    }

    void Game::CastSpellOnLoc(int32_t tileX, int32_t tileZ, int32_t locBitset)
    {
        out.WriteOp(35);
        out.Write16LE(tileX + sceneBaseTileX);
        out.Write16A(activeSpellID);
        out.Write16A(tileZ + sceneBaseTileZ);
        out.Write16LE((locBitset >> 14) & 0x7fff);
    }

    void Game::UseObjOnGroundObj(int32_t tileX, int32_t tileZ, int32_t groundObjID)
    {
        bool k = TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0],
            tileX, tileZ, 0, 0, 0, 0, 0, false);
        if (!k) {
            TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0],
                tileX, tileZ, 0, 1, 1, 0, 0, false);
        }
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(25);
        out.Write16LE(selectedObjInterfaceID);
        out.Write16A(selectedObjID);
        out.Write16(groundObjID);
        out.Write16A(tileZ + sceneBaseTileZ);
        out.Write16LEA(selectedObjSlot);
        out.Write16(tileX + sceneBaseTileX);
    }

    void Game::UseObjOnLoc(int32_t tileX, int32_t tileZ, int32_t locBitset)
    {
        out.WriteOp(192);
        out.Write16(selectedObjInterfaceID);
        out.Write16LE((locBitset >> 14) & 0x7fff);
        out.Write16LEA(tileZ + sceneBaseTileZ);
        out.Write16LE(selectedObjSlot);
        out.Write16LEA(tileX + sceneBaseTileX);
        out.Write16(selectedObjID);
    }

    void Game::UseButton(int32_t b)
    {
        const auto& iface = IfType::instances[b];
        bool notify = true;
        if (iface->contentType > 0) {
            notify = HandleInterfaceAction(*iface);
        }
        if (notify) {
            out.WriteOp(185);
            out.Write16(b);
        }
    }

    void Game::SelectSpell(int32_t interfaceID)
    {
        const auto& iface = IfType::instances[interfaceID];
        spellSelected = 1;
        activeSpellID = interfaceID;
        activeSpellFlags = iface->spellFlags;
        objSelected = 0;
        redrawSidebar = true;

        std::string prefix = iface->spellAction;

        if (prefix.contains(" ")) {
            prefix = prefix.substr(0, prefix.find(" "));
        }

        std::string suffix = iface->spellAction;

        if (suffix.contains(" ")) {
            suffix = suffix.substr(suffix.find(" ") + 1);
        }

        spellCaption = prefix + " " + iface->spellName + " " + suffix;

        if (activeSpellFlags == 0x10) {
            redrawSidebar = true;
            selectedTab = 3;
            redrawSideicons = true;
        }
    }

    void Game::SelectObj(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        objSelected = 1;
        selectedObjSlot = slot;
        selectedObjInterfaceID = interfaceID;
        selectedObjID = objID;
        selectedObjName = ObjType::Get(objID)->name;
        spellSelected = 0;
        redrawSidebar = true;
    }

    void Game::UsePlayerOption3(int32_t playerID)
    {
        const auto& player = players[playerID];
        if (player == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], player->pathTileX[0],
            player->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(139);
        out.Write16LE(playerID);
    }

    void Game::UsePlayerOption4(int32_t playerID)
    {
        const auto& player = players[playerID];
        if (player == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0],
            player->pathTileX[0], player->pathTileZ[0], 0, 1, 1,
            0, 0, false);
        crossX = static_cast<int32_t>(mouseClickX);
        crossY = static_cast<int32_t>(mouseClickY);
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(39);
        out.Write16LE(playerID);
    }

    void Game::UseSelectOption(int32_t interfaceID)
    {
        out.WriteOp(185);
        out.Write16(interfaceID);
        const auto& iface = IfType::instances[interfaceID];
        if ((!iface->scripts.empty()) && (iface->scripts[0][0] == 5)) {
            int32_t varpID = iface->scripts[0][1];
            if (varps[varpID] != iface->scriptOperand[0]) {
                varps[varpID] = iface->scriptOperand[0];
                UpdateVarp(varpID);
                redrawSidebar = true;
            }
        }
    }

    void Game::UseNPCOption0(int32_t npcID)
    {
        const auto& npc = npcs[npcID];
        if (npc == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], npc->pathTileX[0],
            npc->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(155);
        out.Write16LE(npcID);
    }

    void Game::UseNPCOption1(int32_t c)
    {
        const auto& npc = npcs[c];
        if (npc == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], npc->pathTileX[0],
            npc->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(72);
        out.Write16A(c);
    }

    void Game::UseNPCOption2(int32_t npcID)
    {
        const auto& npc = npcs[npcID];
        if (npc == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], npc->pathTileX[0],
            npc->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(17);
        out.Write16LEA(npcID);
    }

    void Game::UseNPCOption3(int32_t npcID)
    {
        const auto& npc = npcs[npcID];
        if (npc == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], npc->pathTileX[0],
            npc->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(21);
        out.Write16(npcID);
    }

    void Game::UseNPCOption4(int32_t npcID)
    {
        const auto& npc = npcs[npcID];
        if (npc == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], npc->pathTileX[0],
            npc->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(18);
        out.Write16LE(npcID);
    }

    void Game::UseObjOption0(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        out.WriteOp(122);
        out.Write16LEA(interfaceID);
        out.Write16A(slot);
        out.Write16LE(objID);
        actionCycles = 0;
        actionInterfaceID = interfaceID;
        actionSlot = slot;
        actionArea = 2;
        if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
            actionArea = 1;
        }
        if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
            actionArea = 3;
        }
    }

    void Game::CastSpellOnNPC(int32_t npcID)
    {
        const auto& npc = npcs[npcID];
        if (npc == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], npc->pathTileX[0],
         npc->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(131);
        out.Write16LEA(npcID);
        out.Write16A(activeSpellID);
    }


    void Game::CastSpellOnPlayer(int32_t playerID)
    {
        const auto& player = players[playerID];
        if (player == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], player->pathTileX[0],
            player->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = mouseClickX;
        crossY = mouseClickY;
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(249);
        out.Write16A(playerID);
        out.Write16LE(activeSpellID);
    }

    void Game::ExamineNPC(int32_t npcID)
    {
        const auto& npc = npcs[npcID];

        if (npc == nullptr) {
            return;
        }

        auto type = npc->type;

        if (!type->overrides.empty()) {
            type = type->GetOverrideType();
        }

        if (type == nullptr) {
            return;
        }

        std::string message;
        if (!type->examine.empty()) {
            message = type->examine;
        } else {
            message = "It's a " + type->name + ".";
        }
        AddMessage(0, "", message);
    }

    void Game::ExamineObj(int32_t objID)
    {
        const auto& obj = ObjType::Get(objID);
        std::string message;
        if (!obj->examine.empty()) {
            message = obj->examine;
        } else {
            message = "It's a " + obj->name + ".";
        }
        AddMessage(0, "", message);
    }

    void Game::ExamineInventoryObj(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        const auto& type = ObjType::Get(objID);
        const auto& iface = IfType::instances[interfaceID];
        std::string message;
        if ((iface != nullptr) && (iface->inventorySlotObjCount[slot] >= 0x186a0)) {
            message = std::to_string(iface->inventorySlotObjCount[slot]) + " x " + type->name;
        } else if (!type->examine.empty()) {
            message = type->examine;
        } else {
            message = "It's a " + type->name + ".";
        }
        AddMessage(0, "", message);
    }

    void Game::UseLocOption0(int32_t a, int32_t b, int32_t c)
    {
        InteractWithLoc(c, a, b);
        out.WriteOp(132);
        out.Write16LEA(a + sceneBaseTileX);
        out.Write16((c >> 14) & 0x7fff);
        out.Write16A(b + sceneBaseTileZ);
    }

    void Game::UseLocOption2(int32_t a, int32_t b, int32_t c)
    {
        InteractWithLoc(c, a, b);
        out.WriteOp(70);
        out.Write16LE(a + sceneBaseTileX);
        out.Write16(b + sceneBaseTileZ);
        out.Write16LEA((c >> 14) & 0x7fff);
    }

    void Game::UseLocOption3(int32_t a, int32_t b, int32_t c)
    {
        InteractWithLoc(c, a, b);
        out.WriteOp(234);
        out.Write16LEA(a + sceneBaseTileX);
        out.Write16A((c >> 14) & 0x7fff);
        out.Write16LEA(b + sceneBaseTileZ);
    }

    void Game::UsePlayerOption0(int32_t playerID)
    {
        const auto& player = players[playerID];
        if (player == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], player->pathTileX[0],
            player->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = static_cast<int32_t>(mouseClickX);
        crossY = static_cast<int32_t>(mouseClickY);
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(128);
        out.Write16(playerID);
    }

    void Game::UsePlayerOption1(int32_t playerID)
    {
        const auto& player = players[playerID];
        if (player == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], player->pathTileX[0],
            player->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = static_cast<int32_t>(mouseClickX);
        crossY = static_cast<int32_t>(mouseClickY);
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(153);
        out.Write16LE(playerID);
    }

    void Game::UsePlayerOption2(int32_t c)
    {
        const auto& player = players[c];
        if (player == nullptr) {
            return;
        }
        TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], player->pathTileX[0],
            player->pathTileZ[0], 0, 1, 1, 0, 0, false);
        crossX = static_cast<int32_t>(mouseClickX);
        crossY = static_cast<int32_t>(mouseClickY);
        crossMode = 2;
        crossCycle = 0;
        out.WriteOp(73);
        out.Write16LE(c);
    }

    void Game::UseLocOption1(int32_t a, int32_t b, int32_t c)
    {
        InteractWithLoc(c, a, b);
        out.WriteOp(252);
        out.Write16LEA((c >> 14) & 0x7fff);
        out.Write16LE(b + sceneBaseTileZ);
        out.Write16A(a + sceneBaseTileX);
    }

    void Game::UseLocOption4(int32_t tileX, int32_t tileZ, int32_t locBitset)
    {
        InteractWithLoc(locBitset, tileX, tileZ);
        out.WriteOp(228);
        out.Write16A((locBitset >> 14) & 0x7fff);
        out.Write16A(tileZ + sceneBaseTileZ);
        out.Write16(tileX + sceneBaseTileX);
    }

    void Game::UseInventoryOption0(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        out.WriteOp(145);
        out.Write16A(interfaceID);
        out.Write16A(slot);
        out.Write16A(objID);
        actionCycles = 0;
        actionInterfaceID = interfaceID;
        actionSlot = slot;
        actionArea = 2;
        if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
            actionArea = 1;
        }
        if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
            actionArea = 3;
        }
    }

    void Game::UseInventoryOption1(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        out.WriteOp(117);
        out.Write16LEA(interfaceID);
        out.Write16LEA(objID);
        out.Write16LE(slot);
        actionCycles = 0;
        actionInterfaceID = interfaceID;
        actionSlot = slot;
        actionArea = 2;
        if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
            actionArea = 1;
        }
        if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
            actionArea = 3;
        }
    }

    void Game::UseInventoryOption2(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        out.WriteOp(43);
        out.Write16LE(interfaceID);
        out.Write16A(objID);
        out.Write16A(slot);
        actionCycles = 0;
        actionInterfaceID = interfaceID;
        actionSlot = slot;
        actionArea = 2;
        if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
            actionArea = 1;
        }
        if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
            actionArea = 3;
        }
    }

    void Game::UseInventoryOption3(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        out.WriteOp(129);
        out.Write16A(slot);
        out.Write16(interfaceID);
        out.Write16A(objID);
        actionCycles = 0;
        actionInterfaceID = interfaceID;
        actionSlot = slot;
        actionArea = 2;
        if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
            actionArea = 1;
        }
        if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
            actionArea = 3;
        }
    }

    void Game::UseInventoryOption4(int32_t a, int32_t interfaceID, int32_t c)
    {
        out.WriteOp(135);
        out.Write16LE(a);
        out.Write16A(interfaceID);
        out.Write16LE(c);
        actionCycles = 0;
        actionInterfaceID = interfaceID;
        actionSlot = a;
        actionArea = 2;
        if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
            actionArea = 1;
        }
        if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
            actionArea = 3;
        }
    }

    void Game::UseObjOption1(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        out.WriteOp(41);
        out.Write16(objID);
        out.Write16A(slot);
        out.Write16A(interfaceID);
        actionCycles = 0;
        actionInterfaceID = interfaceID;
        actionSlot = slot;
        actionArea = 2;
        if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
            actionArea = 1;
        }
        if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
            actionArea = 3;
        }
    }

    void Game::UseObjOption2(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        out.WriteOp(16);
        out.Write16A(objID);
        out.Write16LEA(slot);
        out.Write16LEA(interfaceID);
        actionCycles = 0;
        actionInterfaceID = interfaceID;
        actionSlot = slot;
        actionArea = 2;
        if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
            actionArea = 1;
        }
        if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
            actionArea = 3;
        }
    }

    void Game::UseObjOption3(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        out.WriteOp(75);
        out.Write16LEA(interfaceID);
        out.Write16LE(slot);
        out.Write16A(objID);
        actionCycles = 0;
        actionInterfaceID = interfaceID;
        actionSlot = slot;
        actionArea = 2;
        if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
            actionArea = 1;
        }
        if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
            actionArea = 3;
        }
    }

    void Game::UseObjOption4(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        out.WriteOp(87);
        out.Write16A(objID);
        out.Write16(interfaceID);
        out.Write16A(slot);
        actionCycles = 0;
        actionInterfaceID = interfaceID;
        actionSlot = slot;
        actionArea = 2;
        if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
            actionArea = 1;
        }
        if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
            actionArea = 3;
        }
    }

    void Game::UseObjOnObj(int32_t slot, int32_t interfaceID, int32_t objID)
    {
        out.WriteOp(53);
        out.Write16(slot);
        out.Write16A(selectedObjSlot);
        out.Write16LEA(objID);
        out.Write16(selectedObjInterfaceID);
        out.Write16LE(selectedObjID);
        out.Write16(interfaceID);
        actionCycles = 0;
        actionInterfaceID = interfaceID;
        actionSlot = slot;
        actionArea = 2;
        if (IfType::instances[interfaceID]->parentID == viewportInterfaceID) {
            actionArea = 1;
        }
        if (IfType::instances[interfaceID]->parentID == chatInterfaceID) {
            actionArea = 3;
        }
    }

    void Game::UseToggleOption(int32_t interfaceID)
    {
        out.WriteOp(185);
        out.Write16(interfaceID);
        const auto& iface = IfType::instances[interfaceID];

        if ((!iface->scripts.empty()) && (iface->scripts[0][0] == 5)) {
            int32_t varpID = iface->scripts[0][1];
            varps[varpID] = 1 - varps[varpID];
            UpdateVarp(varpID);
            redrawSidebar = true;
        }
    }

    bool Game::InteractWithLoc(int32_t bitset, int32_t x, int32_t z)
    {
        int32_t locID = (bitset >> 14) & 0x7fff;
        int32_t info = scene->GetInfo(currentLevel, x, z, bitset);
        if (info == -1) {
            return false;
        }
        int32_t type = info & 0x1f;
        int32_t angle = (info >> 6) & 3;
        if ((type == 10) || (type == 11) || (type == 22)) {
            const auto& loc = LocType::Get(locID);
            int32_t width;
            int32_t length;
            if ((angle == 0) || (angle == 2)) {
                width = loc->sizeX;
                length = loc->sizeZ;
            } else {
                width = loc->sizeZ;
                length = loc->sizeX;
            }
            int32_t interactionFlags = loc->interactionSideFlags;
            if (angle != 0) {
                interactionFlags = ((interactionFlags << angle) & 0xf) + (interactionFlags >> (4 - angle));
            }
            TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], x, z,
                0, width, length, 0, interactionFlags, false);
        } else {
            TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], x, z,
                type + 1, 0, 0, angle, 0, false);
        }
        crossX = static_cast<int32_t>(mouseClickX);
        crossY = static_cast<int32_t>(mouseClickY);
        crossMode = 2;
        crossCycle = 0;
        return true;
    }

    void Game::BuildSceneInstanced(SceneBuilder& builder)
    {
        for (int32_t level = 0; level < 4; level++) {
            for (int32_t x = 0; x < 13; x++) {
                for (int32_t z = 0; z < 13; z++) {
                    int32_t chunk = levelChunkBitset[level][x][z];
                    if (chunk == -1) {
                        continue;
                    }
                    int32_t mapLevel = (chunk >> 24) & 3;
                    int32_t chunkRotation = (chunk >> 1) & 3;
                    int32_t mapX = (chunk >> 14) & 0x3ff;
                    int32_t mapZ = (chunk >> 3) & 0x7ff;
                    int32_t mapID = ((mapX / 8) << 8) + (mapZ / 8);
                    for (int32_t i = 0; i < sceneMapIndex.size(); i++) {
                        if ((sceneMapIndex[i] != mapID) || (sceneMapLandData[i].empty())) {
                            continue;
                        }
                        builder.ReadChunkTiles(levelCollisionMap, sceneMapLandData[i], (mapX & 7) * 8, (mapZ & 7) * 8, mapLevel, chunkRotation, x * 8, z * 8, level);
                        break;
                    }
                }
            }
        }

        for (int32_t chunkX = 0; chunkX < 13; chunkX++) {
            for (int32_t chunkZ = 0; chunkZ < 13; chunkZ++) {
                int32_t bitset = levelChunkBitset[0][chunkX][chunkZ];
                if (bitset == -1) {
                    builder.StitchHeightmap(chunkX * 8, chunkZ * 8, 8, 8);
                }
            }
        }

        out.WriteOp(0);

        for (int32_t level = 0; level < 4; level++) {
            for (int32_t chunkX = 0; chunkX < 13; chunkX++) {
                for (int32_t chunkZ = 0; chunkZ < 13; chunkZ++) {
                    int32_t chunkBitset = levelChunkBitset[level][chunkX][chunkZ];
                    if (chunkBitset == -1) {
                        continue;
                    }
                    int32_t mapLevel = (chunkBitset >> 24) & 3;
                    int32_t mapRotation = (chunkBitset >> 1) & 3;
                    int32_t mapX = (chunkBitset >> 14) & 0x3ff;
                    int32_t mapZ = (chunkBitset >> 3) & 0x7ff;
                    int32_t mapID = ((mapX / 8) << 8) + (mapZ / 8);
                    for (int32_t i = 0; i < sceneMapIndex.size(); i++) {
                        if ((sceneMapIndex[i] != mapID) || (sceneMapLocData[i].empty())) {
                            continue;
                        }
                        builder.ReadChunkLocs(levelCollisionMap, *scene, mapLevel, mapRotation, (mapX & 7) * 8, (mapZ & 7) * 8, chunkX * 8, chunkZ * 8, sceneMapLocData[i], level);
                        break;
                    }
                }
            }
        }
    }

    void Game::BuildSceneStandard(SceneBuilder &builder)
    {
        auto mapCount = static_cast<int32_t>(sceneMapLandData.size());

        for (int32_t i = 0; i < mapCount; i++) {
            const std::vector<int8_t>& data = sceneMapLandData[i];
            if (!data.empty()) {
                int32_t originX = ((sceneMapIndex[i] >> 8) * 64) - sceneBaseTileX;
                int32_t originZ = ((sceneMapIndex[i] & 0xff) * 64) - sceneBaseTileZ;
                builder.ReadTiles(
                    data,
                    originZ,
                    originX,
                    (sceneCenterZoneX - 6) * 8,
                    (sceneCenterZoneZ - 6) * 8,
                    levelCollisionMap
                );
            }
        }

        for (int32_t i = 0; i < mapCount; i++) {
            const std::vector<int8_t>& data = sceneMapLandData[i];
            if ((data.empty()) && (sceneCenterZoneZ < 800)) {
                int32_t originX = ((sceneMapIndex[i] >> 8) * 64) - sceneBaseTileX;
                int32_t originZ = ((sceneMapIndex[i] & 0xff) * 64) - sceneBaseTileZ;
                builder.StitchHeightmap(originX, originZ, 64, 64);
            }
        }

        out.WriteOp(0);

        for (int32_t i = 0; i < mapCount; i++) {
            const std::vector<int8_t>& data = sceneMapLocData[i];
            if (!data.empty()) {
                int32_t originX = ((sceneMapIndex[i] >> 8) * 64) - sceneBaseTileX;
                int32_t originZ = ((sceneMapIndex[i] & 0xff) * 64) - sceneBaseTileZ;
                builder.ReadLocs(levelCollisionMap, *scene, originX, originZ, data);
            }
        }
    }

    void Game::SortObjStacks(int32_t x, int32_t z)
    {
        auto& list = levelObjStacks[currentLevel][x][z];

        if (list.isEmpty()) {
            scene->RemoveObjStack(currentLevel, x, z);
            return;
        }

        int32_t topCost = -99999999;

        ObjEntity* topObj = nullptr;
        ObjEntity* bottomObj = nullptr;
        ObjEntity* middleObj = nullptr;

        for (auto* node = list. peekFront(); node != nullptr; node = list.prev()) {
            auto* obj = static_cast<ObjEntity*>(node);
            const auto& type = ObjType:: Get(obj->id);

            int32_t cost = type->cost;
            if (type->stackable) {
                cost *= (obj->count + 1);
            }

            if (cost > topCost) {
                topCost = cost;
                topObj = obj;
            }
        }

        list.pushFront(topObj);

        for (auto* node = list.peekFront(); node != nullptr; node = list.prev()) {
            auto* obj = static_cast<ObjEntity*>(node);

            if (obj->id != topObj->id && bottomObj == nullptr) {
                bottomObj = obj;
            }
            if (obj->id != topObj->id && bottomObj != nullptr && obj->id != bottomObj->id && middleObj == nullptr) {
                middleObj = obj;
            }
        }

        int32_t bitset = x + (z << 7) + 0x60000000;

        // Create shared_ptr copies while preserving ObjEntity type
        std::shared_ptr<Entity> topObjPtr = topObj ? std::make_shared<ObjEntity>(*topObj) : nullptr;
        std::shared_ptr<Entity> bottomObjPtr = bottomObj ? std::make_shared<ObjEntity>(*bottomObj) : nullptr;
        std::shared_ptr<Entity> middleObjPtr = middleObj ? std::make_shared<ObjEntity>(*middleObj) : nullptr;

        scene->AddObjStack(topObjPtr, bottomObjPtr, middleObjPtr, currentLevel, x, z,
                           GetHeightmapY(currentLevel, (x * 128) + 64, (z * 128) + 64), bitset);
    }

    void Game::ClearTemporaryLocs()
    {
        for (auto loc = static_cast<SceneLocTemporary*>(temporaryLocs.peekFront()); loc != nullptr; loc = static_cast<SceneLocTemporary*>(temporaryLocs.prev())) {
            if (loc->duration == -1) {
                loc->delay = 0;
                StoreLoc(*loc);
            } else {
                loc->unlink();
                delete loc;
            }
        }
    }

    void Game::StoreLoc(SceneLocTemporary& loc) const
    {
        int32_t bitset = 0;
        int32_t locID = -1;
        int32_t kind = 0;
        int32_t rotation = 0;
        if (loc.classID == 0) {
            bitset = scene->GetWallBitset(loc.level, loc.localX, loc.localZ);
        }
        if (loc.classID == 1) {
            bitset = scene->GetWallDecorationBitset(loc.level, loc.localX, loc.localZ);
        }
        if (loc.classID == 2) {
            bitset = scene->GetLocBitset(loc.level, loc.localX, loc.localZ);
        }
        if (loc.classID == 3) {
            bitset = scene->GetGroundDecorationBitset(loc.level, loc.localX, loc.localZ);
        }
        if (bitset != 0) {
            int32_t info = scene->GetInfo(loc.level, loc.localX, loc.localZ, bitset);
            locID = (bitset >> 14) & 0x7fff;
            kind = info & 0x1f;
            rotation = info >> 6;
        }
        loc.previousLocID = locID;
        loc.previousKind = kind;
        loc.previousRotation = rotation;
    }

    void Game::UpdateTemporaryLocs()
    {
        if (sceneState == 2) {
            for (auto loc = static_cast<SceneLocTemporary*>(temporaryLocs.peekFront()); loc != nullptr; loc = static_cast<SceneLocTemporary*>(temporaryLocs.prev())) {
                if (loc->duration > 0) {
                    loc->duration--;
                }

                if (loc->duration == 0) {
                    if ((loc->previousLocID < 0) || SceneBuilder::IsLocReady(loc->previousLocID, loc->previousKind)) {
                        AddLoc(
                            loc->localZ,
                            loc->level,
                            loc->previousRotation,
                            loc->previousKind,
                            loc->localX,
                            loc->classID,
                            loc->previousLocID
                        );
                        loc->unlink();
                        delete loc;
                    }
                } else {
                    if (loc->delay > 0) {
                        loc->delay--;
                    }

                    if (loc->delay == 0
                        && loc->localX >= 1 && loc->localZ >= 1
                        && loc->localX <= 102 && loc->localZ <= 102
                        && ((loc->id < 0) || SceneBuilder::IsLocReady(loc->id, loc->kind)))
                    {
                        AddLoc(
                            loc->localZ,
                            loc->level,
                            loc->rotation,
                            loc->kind,
                            loc->localX,
                            loc->classID,
                            loc->id
                        );
                        loc->delay = -1;

                        if ((loc->id == loc->previousLocID) && (loc->previousLocID == -1)) {
                            loc->unlink();
                            delete loc;
                        } else if ((loc->id == loc->previousLocID)
                                && (loc->rotation == loc->previousRotation)
                                && (loc->kind == loc->previousKind)) {
                            loc->unlink();
                            delete loc;
                        }
                    }
                }
            }
        }
    }

    void Game::UpdateVarp(int32_t varpID)
    {
        int32_t type = VarpType::instances[varpID]->type;

        if (type == 0) {
            return;
        }

        int32_t varp = varps[varpID];

        if (type == 1) {
            if (varp == 1) {
                Draw3D::SetBrightness(0.9);
            }
            if (varp == 2) {
                Draw3D::SetBrightness(0.8);
            }
            if (varp == 3) {
                Draw3D::SetBrightness(0.7);
            }
            if (varp == 4) {
                Draw3D::SetBrightness(0.6);
            }
            ObjType::iconCache.clear();
            redrawTitleBackground = true;
        }

        if (type == 3) {
            bool active = midiEnabled;

            if (varp == 0) {
                MidiVol(midiEnabled, 0);
                midiEnabled = true;
            }

            if (varp == 1) {
                MidiVol(midiEnabled, -400);
                midiEnabled = true;
            }

            if (varp == 2) {
                MidiVol(midiEnabled, -800);
                midiEnabled = true;
            }

            if (varp == 3) {
                MidiVol(midiEnabled, -1200);
                midiEnabled = true;
            }

            if (varp == 4) {
                midiEnabled = false;
            }

            if ((midiEnabled != active) && !lowmem) {
                if (midiEnabled) {
                    song = nextSong;
                    songFading = true;
                    ondemand->Request(2, song);
                } else {
                    StopMidi();
                }
                nextSongDelay = 0;
            }
        }

        if (type == 4) {
            if (varp == 0) {
                waveEnabled = true;
                SetWaveVolume(0);
            }
            if (varp == 1) {
                waveEnabled = true;
                SetWaveVolume(-400);
            }
            if (varp == 2) {
                waveEnabled = true;
                SetWaveVolume(-800);
            }
            if (varp == 3) {
                waveEnabled = true;
                SetWaveVolume(-1200);
            }
            if (varp == 4) {
                waveEnabled = false;
            }
        }

        if (type == 5) {
            mouseButtonsOption = varp;
        }

        if (type == 6) {
            chatEffects = varp;
        }

        if (type == 8) {
            splitPrivateChat = varp;
            redrawChatback = true;
        }

        if (type == 9) {
            bankArrangeMode = varp;
        }
    }

    void Game::AddLoc(int32_t z, int32_t level, int32_t angle, int32_t kind, int32_t x, int32_t classID, int32_t id)
    {
        if ((x < 1) || (z < 1) || (x > 102) || (z > 102)) {
            return;
        }

        if (lowmem && (level != currentLevel)) {
            return;
        }

        int32_t bitset = 0;

        if (classID == 0) {
            bitset = scene->GetWallBitset(level, x, z);
        }

        if (classID == 1) {
            bitset = scene->GetWallDecorationBitset(level, x, z);
        }

        if (classID == 2) {
            bitset = scene->GetLocBitset(level, x, z);
        }

        if (classID == 3) {
            bitset = scene->GetGroundDecorationBitset(level, x, z);
        }

        if (bitset != 0) {
            int32_t otherInfo = scene->GetInfo(level, x, z, bitset);
            int32_t otherID = (bitset >> 14) & 0x7fff;
            int32_t otherKind = otherInfo & 0x1f;
            int32_t otherRotation = otherInfo >> 6;

            if (classID == 0) {
                scene->RemoveWall(x, level, z);
                const auto& type = LocType::Get(otherID);
                if (type->solid) {
                    levelCollisionMap[level]->Remove(x, z, otherRotation, otherKind, type->blocksProjectiles);
                }
            }

            if (classID == 1) {
                scene->RemoveWallDecoration(level, x, z);
            }

            if (classID == 2) {
                scene->RemoveLoc(level, x, z);
                const auto& type = LocType::Get(otherID);

                if ((x + type->sizeX) > 103 || ((z + type->sizeX) > 103) || ((x + type->sizeZ) > 103) || (z + type->sizeZ) > 103) {
                    return;
                }

                if (type->solid) {
                    levelCollisionMap[level]->Remove(otherRotation, type->sizeX, x, z, type->sizeZ, type->blocksProjectiles);
                }
            }

            if (classID == 3) {
                scene->RemoveGroundDecoration(level, x, z);
                const auto& type = LocType::Get(otherID);

                if (type->solid && type->interactable) {
                    levelCollisionMap[level]->RemoveSolid(x, z);
                }
            }
        }

        if (id >= 0) {
            int32_t tileLevel = level;

            // check for bridged tile
            if ((tileLevel < 3) && ((levelTileFlags[1][x][z] & 2) == 2)) {
                tileLevel++;
            }

            SceneBuilder::AddLoc(*scene, angle, z, kind, tileLevel, levelCollisionMap[level], levelHeightmap, x, id, level);
        }
    }

    void Game::UpdateTextures(int32_t cycle)
    {
        if (lowmem) {
            return;
        }

        UpdateTexture(17, cycle);
        UpdateTexture(24, cycle);
        UpdateTexture(34, cycle);
    }

    void Game::UpdateTexture(int32_t textureID, int32_t cycle)
    {
        if (Draw3D::textureCycle[textureID] < cycle) {
            return;
        }

        auto& texture = Draw3D::textures[textureID];

        int32_t bottom = (texture.width * texture.height) - 1;
        int32_t adjustment = texture.width * delta * 2; // moves texels down by 2 pixels

        auto& buffer0 = texture.pixels;
        auto& buffer1 = textureBuffer;

        for (int32_t i = 0; i <= bottom; i++) {
            buffer1[i] = buffer0[(i - adjustment) & bottom];
        }

        texture.pixels = buffer1;
        textureBuffer = buffer0;

        // causes the texture to rebuild
        Draw3D::PushTexture(textureID);
    }

    bool Game::IsSceneLocsLoaded()
    {
        bool ok = true;
        for (int32_t i = 0; i < sceneMapLocData.size(); i++) {
            std::vector<int8_t>& data = sceneMapLocData[i];

            if (data.empty()) {
                continue;
            }

            int32_t originX = ((sceneMapIndex[i] >> 8) * 64) - sceneBaseTileX;
            int32_t originZ = ((sceneMapIndex[i] & 0xff) * 64) - sceneBaseTileZ;

            if (sceneInstanced) {
                originX = 10;
                originZ = 10;
            }

            ok &= SceneBuilder::ValidateLocs(data, originX, originZ);
        }
        return ok;
    }

    void Game::AcceptPlayerRequest(int32_t action, const std::string& playerName)
    {
        bool found = false;
        for (int32_t i = 0; i < playerCount; i++) {
            const auto& player = players[playerIDs[i]];

            if ((player == nullptr) || (player->name.empty()) || !StringUtil::EqualsIgnoreCase(player->name, playerName)) {
                continue;
            }

            TryMove(2, localPlayer->pathTileX[0], localPlayer->pathTileZ[0], player->pathTileX[0],
                player->pathTileZ[0], 0, 1, 1, 0, 0, false);

            // trade
            if (action == 484) {
                out.WriteOp(139);
                out.Write16LE(playerIDs[i]);
            }

            // challenge
            if (action == 6) {
                out.WriteOp(128);
                out.Write16(playerIDs[i]);
            }

            found = true;
            break;
        }

        if (!found) {
            AddMessage(0, "", "Unable to find " + playerName);
        }
    }

    void Game::DrawMinimapHint()
    {
        if ((hintType != 0) && ((loopCycle % 20) < 10)) {
            if ((hintType == 1) && (hintNPC >= 0) && (hintNPC < npcs.size())) {
                const auto& npc = npcs[hintNPC];

                if (npc != nullptr) {
                    int32_t x = (npc->x / 32) - (localPlayer->x / 32);
                    int32_t y = (npc->z / 32) - (localPlayer->z / 32);
                    DrawMinimapHint(*imageMapmarker1, x, y);
                }
            }

            if (hintType == 2) {
                int32_t x = (((hintTileX - sceneBaseTileX) * 4) + 2) - (localPlayer->x / 32);
                int32_t y = (((hintTileZ - sceneBaseTileZ) * 4) + 2) - (localPlayer->z / 32);
                DrawMinimapHint(*imageMapmarker1, x, y);
            }

            if ((hintType == 10) && (hintPlayer >= 0) && (hintPlayer < players.size())) {
                const auto& player = players[hintPlayer];
                if (player != nullptr) {
                    int32_t x = (player->x / 32) - (localPlayer->x / 32);
                    int32_t z = (player->z / 32) - (localPlayer->z / 32);
                    DrawMinimapHint(*imageMapmarker1, x, z);
                }
            }
        }
    }

    void Game::DrawMinimapHint(Image24& image, int32_t x, int32_t y) const
    {
        int32_t distance2 = (x * x) + (y * y);

        if ((distance2 > 4225) && (distance2 < 90000)) {
            int32_t angle = (orbitCameraYaw + minimapAnticheatAngle) & 0x7ff;
            int32_t sinAngle = Draw3D::sin[angle];
            int32_t cosAngle = Draw3D::cos[angle];
            sinAngle = (sinAngle * 256) / (minimapZoom + 256);
            cosAngle = (cosAngle * 256) / (minimapZoom + 256);
            int32_t directionX = ((y * sinAngle) + (x * cosAngle)) >> 16;
            int32_t directionY = ((y * cosAngle) - (x * sinAngle)) >> 16;
            float directionAngle = SDL_atan2f(static_cast<float>(directionX), static_cast<float>(directionY));
            auto hintX = static_cast<int32_t>(SDL_sinf(directionAngle) * 63.0f);
            auto hintY = static_cast<int32_t>(SDL_cosf(directionAngle) * 57.0f);
            imageMapedge->DrawRotated((94 + hintX + 4) - 10, 83 - hintY - 20, 20, 20, 15, 15, directionAngle, 256);
        } else {
            DrawOnMinimap(image, x, y);
        }
    }

    int32_t Game::GetTopLevel()
    {
        int32_t top = 3;

        if (cameraPitch < 310) {
            int32_t cameraLocalTileX = cameraX >> 7;
            int32_t cameraLocalTileZ = cameraZ >> 7;
            int32_t playerLocalTileX = localPlayer->x >> 7;
            int32_t playerLocalTileZ = localPlayer->z >> 7;

            if ((levelTileFlags[currentLevel][cameraLocalTileX][cameraLocalTileZ] & 4) != 0) {
                top = currentLevel;
            }

            int32_t tileDeltaX;

            if (playerLocalTileX > cameraLocalTileX) {
                tileDeltaX = playerLocalTileX - cameraLocalTileX;
            } else {
                tileDeltaX = cameraLocalTileX - playerLocalTileX;
            }

            int32_t tileDeltaZ;

            if (playerLocalTileZ > cameraLocalTileZ) {
                tileDeltaZ = playerLocalTileZ - cameraLocalTileZ;
            } else {
                tileDeltaZ = cameraLocalTileZ - playerLocalTileZ;
            }

            if (tileDeltaX > tileDeltaZ) {
                int32_t delta = (tileDeltaZ * 0x10000) / tileDeltaX;
                int32_t accumulator = 32768;

                while (cameraLocalTileX != playerLocalTileX) {
                    if (cameraLocalTileX < playerLocalTileX) {
                        cameraLocalTileX++;
                    } else if (cameraLocalTileX > playerLocalTileX) {
                        cameraLocalTileX--;
                    }

                    if ((levelTileFlags[currentLevel][cameraLocalTileX][cameraLocalTileZ] & 4) != 0) {
                        top = currentLevel;
                    }

                    accumulator += delta;

                    if (accumulator >= 0x10000) {
                        accumulator -= 0x10000;

                        if (cameraLocalTileZ < playerLocalTileZ) {
                            cameraLocalTileZ++;
                        } else if (cameraLocalTileZ > playerLocalTileZ) {
                            cameraLocalTileZ--;
                        }

                        if ((levelTileFlags[currentLevel][cameraLocalTileX][cameraLocalTileZ] & 4) != 0) {
                            top = currentLevel;
                        }
                    }
                }
            } else {
                int32_t delta = (tileDeltaX * 0x10000) / tileDeltaZ;
                int32_t accumulator = 32768;

                while (cameraLocalTileZ != playerLocalTileZ) {
                    if (cameraLocalTileZ < playerLocalTileZ) {
                        cameraLocalTileZ++;
                    } else if (cameraLocalTileZ > playerLocalTileZ) {
                        cameraLocalTileZ--;
                    }

                    if ((levelTileFlags[currentLevel][cameraLocalTileX][cameraLocalTileZ] & 4) != 0) {
                        top = currentLevel;
                    }

                    accumulator += delta;

                    if (accumulator >= 0x10000) {
                        accumulator -= 0x10000;

                        if (cameraLocalTileX < playerLocalTileX) {
                            cameraLocalTileX++;
                        } else if (cameraLocalTileX > playerLocalTileX) {
                            cameraLocalTileX--;
                        }

                        if ((levelTileFlags[currentLevel][cameraLocalTileX][cameraLocalTileZ] & 4) != 0) {
                            top = currentLevel;
                        }
                    }
                }
            }
        }

        if ((levelTileFlags[currentLevel][localPlayer->x >> 7][localPlayer->z >> 7] & 4) != 0) {
            top = currentLevel;
        }

        return top;
    }

    void Game::DrawPrivateMessages()
    {
        if (splitPrivateChat == 0) {
            return;
        }

        const auto& font = fontPlain12;
        int32_t i = 0;
        if (systemUpdateTimer != 0) {
            i = 1;
        }
        for (int32_t j = 0; j < 100; j++) {
            if (!messageText[j].empty()) {
                int32_t k = messageType[j];
                std::string s = messageSender[j];
                int8_t byte1 = 0;
                if (!s.empty() && s.starts_with("@cr1@")) {
                    s = s.substr(5);
                    byte1 = 1;
                }
                if (!s.empty() && s.starts_with("@cr2@")) {
                    s = s.substr(5);
                    byte1 = 2;
                }
                if (((k == 3) || (k == 7)) && ((k == 7) || (privateChatSetting == 0) || ((privateChatSetting == 1) && IsFriend(s)))) {
                    int32_t l = 329 - (i * 13);
                    int32_t k1 = 4;
                    font->DrawString("From", k1, l, 0);
                    font->DrawString("From", k1, l - 1, 65535);
                    k1 += font->StringWidthTaggable("From ");
                    if (byte1 == 1) {
                        imageModIcons[0]->Blit(k1, l - 12);
                        k1 += 14;
                    }
                    if (byte1 == 2) {
                        imageModIcons[1]->Blit(k1, l - 12);
                        k1 += 14;
                    }
                    font->DrawString(s + ": " + messageText[j], k1, l, 0);
                    font->DrawString(s + ": " + messageText[j], k1, l - 1, 65535);
                    if (++i >= 5) {
                        return;
                    }
                }
                if ((k == 5) && (privateChatSetting < 2)) {
                    int32_t i1 = 329 - (i * 13);
                    font->DrawString(messageText[j], 4, i1, 0);
                    font->DrawString(messageText[j], 4, i1 - 1, 65535);
                    if (++i >= 5) {
                        return;
                    }
                }
                if ((k == 6) && (privateChatSetting < 2)) {
                    int32_t y = 329 - (i * 13);
                    font->DrawString("To " + s + ": " + messageText[j], 4, y, 0);
                    font->DrawString("To " + s + ": " + messageText[j], 4, y - 1, 65535);
                    if (++i >= 5) {
                        return;
                    }
                }
            }
        }
    }

    void Game::DrawMouseCrosses() const
    {
        if (crossMode == 1) {
            imageCrosses[crossCycle / 100]->Draw(crossX - 8 - 4, crossY - 8 - 4);
        }

        if (crossMode == 2) {
            imageCrosses[4 + (crossCycle / 100)]->Draw(crossX - 8 - 4, crossY - 8 - 4);
        }
    }

    void Game::DrawViewportInterfaces()
    {
        if (viewportOverlayInterfaceID != -1) {
            UpdateInterfaceAnimation(delta, viewportOverlayInterfaceID);
            DrawParentInterface(*IfType::instances[viewportOverlayInterfaceID], 0, 0, 0);
        }

        if (viewportInterfaceID != -1) {
            UpdateInterfaceAnimation(delta, viewportInterfaceID);
            DrawParentInterface(*IfType::instances[viewportInterfaceID], 0, 0, 0);
        }
    }

    void Game::DrawMultizone() const
    {
        if (multizone == 1) {
            imageHeadicons[1]->Draw(472, 296);
        }
    }

    void Game::DrawSystemUpdateTimer() const
    {
        if (systemUpdateTimer != 0) {
            int32_t seconds = systemUpdateTimer / 50;
            int32_t minutes = seconds / 60;
            seconds %= 60;
            if (seconds < 10) {
                fontPlain12->DrawString("System update in: " + std::to_string(minutes) + ":0" + std::to_string(seconds), 4, 329, 0xffff00);
            } else {
                fontPlain12->DrawString("System update in: " + std::to_string(minutes) + ":" + std::to_string(seconds), 4, 329, 0xffff00);
            }
        }
    }

    void Game::AddMenuOption(const std::string& option, int32_t action)
    {
        AddMenuOption(option, action, 0, 0, 0);
    }

    void Game::AddMenuOption(const std::string& option, int32_t action, int32_t a, int32_t b, int32_t c)
    {
        menuOption[menuSize] = option;
        menuAction[menuSize] = action;
        menuParamA[menuSize] = a;
        menuParamB[menuSize] = b;
        menuParamC[menuSize] = c;
        menuSize++;
    }

    void Game::AddMessage(int32_t type, const std::string& prefix, const std::string& message)
    {
        if ((type == 0) && (stickyChatInterfaceID != -1)) {
            modalMessage = message;
            mouseClickButton = 0;
        }

        if (chatInterfaceID == -1) {
            redrawChatback = true;
        }

        for (int32_t j = 99; j > 0; j--) {
            messageType[j] = messageType[j - 1];
            messageSender[j] = messageSender[j - 1];
            messageText[j] = messageText[j - 1];
        }

        messageType[0] = type;
        messageSender[0] = prefix;
        messageText[0] = message;
    }

    void Game::UpdateChatOverride()
    {
        overrideChat = 0;
        int32_t worldTileX = (localPlayer->x >> 7) + sceneBaseTileX;
        int32_t worldTileZ = (localPlayer->z >> 7) + sceneBaseTileZ;

        if ((worldTileX >= 3053) && (worldTileX <= 3156) && (worldTileZ >= 3056) && (worldTileZ <= 3136)) {
            overrideChat = 1;
        }

        if ((worldTileX >= 3072) && (worldTileX <= 3118) && (worldTileZ >= 9492) && (worldTileZ <= 9535)) {
            overrideChat = 1;
        }

        if ((overrideChat == 1) && (worldTileX >= 3139) && (worldTileX <= 3199) && (worldTileZ >= 3008) && (worldTileZ <= 3062)) {
            overrideChat = 0;
        }
    }

    void Game::DrawMenu() const
    {
        int32_t x = menuX;
        int32_t y = menuY;
        int32_t w = menuWidth;
        int32_t h = menuHeight;
        int32_t background = 0x5d5447;

        Draw2D::FillRect(x, y, w, h, background);
        Draw2D::FillRect(x + 1, y + 1, w - 2, 16, 0);
        Draw2D::DrawRect(x + 1, y + 18, w - 2, h - 19, 0);
        fontBold12->DrawString("Choose Option", x + 3, y + 14, background);

        int32_t mX = static_cast<int32_t>(mouseX);
        int32_t mY = static_cast<int32_t>(mouseY);

        if (menuArea == 0) {
            mX -= 4;
            mY -= 4;
        }

        if (menuArea == 1) {
            mX -= 553;
            mY -= 205;
        }

        if (menuArea == 2) {
            mX -= 17;
            mY -= 357;
        }

        for (int32_t i = 0; i < menuSize; i++) {
            int32_t optionY = y + 31 + ((menuSize - 1 - i) * 15);
            int32_t rgb = 0xffffff;

            if ((mX > x) && (mX < (x + w)) && (mY > (optionY - 13)) && (mY < (optionY + 3))) {
                rgb = 0xffff00;
            }

            fontBold12->DrawStringTaggable(menuOption[i], x + 3, optionY, rgb, true);
        }
    }

    void Game::ShowContextMenu()
    {
        int32_t i = fontBold12->StringWidthTaggable("Choose Option");
        for (int32_t j = 0; j < menuSize; j++) {
            int32_t k = fontBold12->StringWidthTaggable(menuOption[j]);
            if (k > i) {
                i = k;
            }
        }
        i += 8;
        int32_t l = (15 * menuSize) + 21;
        if (mouseClickX > 4 && (mouseClickY > 4) && (mouseClickX < 516) && (mouseClickY < 338)) {
            int32_t i1 = static_cast<int32_t>(mouseClickX) - 4 - (i / 2);
            if ((i1 + i) > 512) {
                i1 = 512 - i;
            }
            if (i1 < 0) {
                i1 = 0;
            }
            int32_t l1 = static_cast<int32_t>(mouseClickY) - 4;
            if ((l1 + l) > 334) {
                l1 = 334 - l;
            }
            if (l1 < 0) {
                l1 = 0;
            }
            menuVisible = true;
            menuArea = 0;
            menuX = i1;
            menuY = l1;
            menuWidth = i;
            menuHeight = (15 * menuSize) + 22;
        }
        if (mouseClickX > 553 && (mouseClickY > 205) && (mouseClickX < 743) && (mouseClickY < 466)) {
            int32_t j1 = static_cast<int32_t>(mouseClickX) - 553 - (i / 2);
            if (j1 < 0) {
                j1 = 0;
            } else if ((j1 + i) > 190) {
                j1 = 190 - i;
            }
            int32_t i2 = static_cast<int32_t>(mouseClickY) - 205;
            if (i2 < 0) {
                i2 = 0;
            } else if ((i2 + l) > 261) {
                i2 = 261 - l;
            }
            menuVisible = true;
            menuArea = 1;
            menuX = j1;
            menuY = i2;
            menuWidth = i;
            menuHeight = (15 * menuSize) + 22;
        }
        if (mouseClickX > 17 && (mouseClickY > 357) && (mouseClickX < 496) && (mouseClickY < 453)) {
            int32_t k1 = static_cast<int32_t>(mouseClickX) - 17 - (i / 2);
            if (k1 < 0) {
                k1 = 0;
            } else if ((k1 + i) > 479) {
                k1 = 479 - i;
            }
            int32_t j2 = static_cast<int32_t>(mouseClickY) - 357;
            if (j2 < 0) {
                j2 = 0;
            } else if ((j2 + l) > 96) {
                j2 = 96 - l;
            }
            menuVisible = true;
            menuArea = 2;
            menuX = k1;
            menuY = j2;
            menuWidth = i;
            menuHeight = (15 * menuSize) + 22;
        }
    }

    void Game::HandleMenuInput(int32_t button)
    {
        if (button != 1) {
            int32_t mX = static_cast<int32_t>(mouseX);
            int32_t mY = static_cast<int32_t>(mouseY);

            if (menuArea == 0) {
                mX -= 4;
                mY -= 4;
            } else if (menuArea == 1) {
                mX -= 553;
                mY -= 205;
            } else if (menuArea == 2) {
                mX -= 17;
                mY -= 357;
            }

            if (mX < (menuX - 10) || (mX > (menuX + menuWidth + 10)) || (mY < (menuY - 10)) || (mY > (menuY + menuHeight + 10))) {
                menuVisible = false;
                if (menuArea == 1) {
                    redrawSidebar = true;
                }
                if (menuArea == 2) {
                    redrawChatback = true;
                }
            }
        }

        if (button == 1) {
            int32_t mX = this->menuX;
            int32_t mY = this->menuY;
            int32_t mWidth = this->menuWidth;
            int32_t mCX = static_cast<int32_t>(mouseClickX);
            int32_t mCY = static_cast<int32_t>(mouseClickY);

            if (menuArea == 0) {
                mCX -= 4;
                mCY -= 4;
            } else if (menuArea == 1) {
                mCX -= 553;
                mCY -= 205;
            } else if (menuArea == 2) {
                mCX -= 17;
                mCY -= 357;
            }

            int32_t option = -1;
            for (int32_t i = 0; i < menuSize; i++) {
                int32_t optionY = mY + 31 + ((menuSize - 1 - i) * 15);

                if ((mCX > mX) && (mCX < (mX + mWidth)) && (mCY > (optionY - 13)) && (mCY < (optionY + 3))) {
                    option = i;
                }
            }

            if (option != -1) {
                UseMenuOption(option);
            }

            menuVisible = false;

            if (menuArea == 1) {
                redrawSidebar = true;
            } else if (menuArea == 2) {
                redrawChatback = true;
            }
        }
    }

    void Game::HandleInput()
    {
        if (objDragArea != 0) {
            return;
        }

        menuOption[0] = "Cancel";
        menuAction[0] = 1107;
        menuSize = 1;

        HandlePrivateChatInput();
        HandleViewportInput();
        HandleSidebarInput();
        HandleChatInput();

        SortMenuOptions();
    }

    void Game::HandleSidebarInput()
    {
        lastHoveredInterfaceID = 0;

        if ((mouseX > 553) && (mouseY > 205) && (mouseX < 743) && (mouseY < 466)) {
            if (sidebarInterfaceID != -1) {
                HandleInterfaceInput(*IfType::instances[sidebarInterfaceID], 553, 205, 0);
            } else if (tabInterfaceID[selectedTab] != -1) {
                HandleInterfaceInput(*IfType::instances[tabInterfaceID[selectedTab]], 553, 205, 0);
            }
        }

        if (lastHoveredInterfaceID != sidebarHoveredInterfaceID) {
            redrawSidebar = true;
            sidebarHoveredInterfaceID = lastHoveredInterfaceID;
        }
    }

    void Game::HandleChatInput()
    {
        lastHoveredInterfaceID = 0;

        if ((mouseX > 17) && (mouseY > 357) && (mouseX < 496) && (mouseY < 453)) {
            if (chatInterfaceID != -1) {
                HandleInterfaceInput(*IfType::instances[chatInterfaceID], 17, 357, 0);
            } else if ((mouseY < 434) && (mouseX < 426)) {
                HandleChatMouseInput(mouseY - 357);
            }
        }

        if ((chatInterfaceID != -1) && (lastHoveredInterfaceID != chatHoveredInterfaceID)) {
            redrawChatback = true;
            chatHoveredInterfaceID = lastHoveredInterfaceID;
        }
    }

    void Game::HandleInputKey()
    {
        do {
            int32_t key = PollKey();

            if (key == -1) {
                break;
            }

            if ((viewportInterfaceID != -1) && (viewportInterfaceID == reportAbuseInterfaceID)) {
                HandleInputReportAbuseKey(key);
            } else if (showSocialInput) {
                HandleInputSocialKey(key);
            } else if (chatbackInputType == 1) {
                HandleInputAmountKey(key);
            } else if (chatbackInputType == 2) {
                HandleInputNameKey(key);
            } else if (chatInterfaceID == -1) {
                HandleInputChatKey(key);
            }
        } while (true);
    }

    void Game::HandleInputReportAbuseKey(int32_t key)
    {
        if ((key == SDLK_BACKSPACE) && (reportAbuseInput.length() > 0)) {
            reportAbuseInput = reportAbuseInput.substr(0, reportAbuseInput.length() - 1);
        }
        // https://www.asciitable.com/
        if ((((key >= 'a') && (key <= 'z')) || ((key >= 'A') && (key <= 'Z')) || ((key >= '0') && (key <= '9')) || (key == ' ')) && (reportAbuseInput.length() < 12)) {
            reportAbuseInput += (char) key;
        }
    }

    void Game::HandleInputSocialKey(int32_t key)
    {
        // https://www.asciitable.com/
        // 32 to 122 is all alpha, numbers and symbols excluding: {}|~
        if ((key >= 32) && (key <= 122) && (socialInput.length() < 80)) {
            socialInput += (char) key;
            redrawChatback = true;
        }
        if ((key == SDLK_BACKSPACE) && (socialInput.length() > 0)) {
            socialInput = socialInput.substr(0, socialInput.length() - 1);
            redrawChatback = true;
        }
        if ((key == 13) || (key == SDLK_RETURN)) {
            showSocialInput = false;
            redrawChatback = true;

            if (socialAction == 1) {
                AddFriend(StringUtil::ToBase37(socialInput));
            }

            if ((socialAction == 2) && (friendCount > 0)) {
                RemoveFriend(StringUtil::ToBase37(socialInput));
            }

            if ((socialAction == 3) && (socialInput.length() > 0)) {
                out.WriteOp(126);
                out.Write8(0);
                int32_t start = out.position;
                out.Write64(inputFriendName37);
                ChatCompression::Pack(socialInput, out);
                out.WriteSize(out.position - start);
                socialInput = ChatCompression::Format(socialInput);
                //socialInput = Censor.filter(socialInput);
                AddMessage(6, StringUtil::FormatName(StringUtil::FromBase37(inputFriendName37)), socialInput);
                if (privateChatSetting == 2) {
                    privateChatSetting = 1;
                    redrawPrivacySettings = true;
                    out.WriteOp(95);
                    out.Write8(publicChatSetting);
                    out.Write8(privateChatSetting);
                    out.Write8(tradeChatSetting);
                }
            }

            if ((socialAction == 4) && (ignoreCount < 100)) {
                AddIgnore(StringUtil::ToBase37(socialInput));
            }

            if ((socialAction == 5) && (ignoreCount > 0)) {
                RemoveIgnore(StringUtil::ToBase37(socialInput));
            }
        }
    }

    void Game::HandleInputAmountKey(int32_t key)
    {
        // https://www.asciitable.com/
        if ((key >= '0') && (key <= '9') && (chatbackInput.length() < 10)) {
            chatbackInput += (char) key;
            redrawChatback = true;
        }
        if ((key == SDLK_BACKSPACE) && (chatbackInput.length() > 0)) {
            chatbackInput = chatbackInput.substr(0, chatbackInput.length() - 1);
            redrawChatback = true;
        }
        if ((key == 13) || (key == SDLK_RETURN)) {
            if (chatbackInput.length() > 0) {
                int32_t i1 = std::stoi(chatbackInput);
                out.WriteOp(208);
                out.Write32(i1);
            }
            chatbackInputType = 0;
            redrawChatback = true;
        }
    }

    void Game::HandleInputNameKey(int32_t key)
    {
        // https://www.asciitable.com/
        // 32 to 122 is all alpha, numbers and symbols excluding: {}|~
        if ((key >= 32) && (key <= 122) && (chatbackInput.length() < 12)) {
            chatbackInput += (char) key;
            redrawChatback = true;
        }
        if ((key == SDLK_BACKSPACE) && (chatbackInput.length() > 0)) {
            chatbackInput = chatbackInput.substr(0, chatbackInput.length() - 1);
            redrawChatback = true;
        }
        if ((key == 13) || (key == SDLK_RETURN)) {
            if (chatbackInput.length() > 0) {
                out.WriteOp(60);
                out.Write64(StringUtil::ToBase37(chatbackInput));
            }
            chatbackInputType = 0;
            redrawChatback = true;
        }
    }

    void Game::HandleInputChatKey(int32_t key)
    {
        // https://www.asciitable.com/
        // 32 to 122 is all numbers and symbols excluding: {}|~
        if ((key >= 32) && (key <= 122) && (chatTyped.length() < 80)) {
            chatTyped += static_cast<char>(key);
            redrawChatback = true;
        }

        if (key == SDLK_BACKSPACE && !chatTyped.empty()) {
            chatTyped = chatTyped.substr(0, chatTyped.length() - 1);
            redrawChatback = true;
        }

        if ((key == 9 || key == 10 || key == 13) && !chatTyped.empty()) {
            //if (rights == 2) {
             if (chatTyped == "::clientdrop") {
                 TryReconnect();
             }
             if (chatTyped == "::lag") {
                 Debug();
             }
             if (chatTyped == "::prefetchmusic") {
                 for (int32_t i = 0; i < ondemand->GetFileCount(2); i++) {
                     ondemand->Prefetch(1, 2, i);
                 }
             }
             if (chatTyped == "::perf") {
                 showPerformance = !showPerformance;
             }
             if (chatTyped == "::occluders") {
                 showOccluders = !showOccluders;
             }
             if (chatTyped == "::traffic") {
                 showTraffic = !showTraffic;
             }
             if (chatTyped.starts_with("::sound"))
             {
                 if (waveEnabled && !lowmem && (waveCount < MAX_WAVES))
                 {
                     int32_t waveID = 70;
                     if (chatTyped.length() > 8)
                     {
                         try {
                             waveID = std::stoi(chatTyped.substr(8));
                         } catch (...) {
                             waveID = 70;
                         }
                     }
                     int32_t delay = 0;
                     int32_t loopCount = 1;
                     waveIDs[waveCount] = waveID;
                     waveLoops[waveCount] = loopCount;
                     waveDelay[waveCount] = delay + SoundTrack::delays[waveID];
                     waveCount++;
                 }
             }
            if (chatTyped.starts_with("::song"))
            {
                int32_t next = 35;
                if (chatTyped.length() > 7)
                {
                    try {
                        next = std::stoi(chatTyped.substr(7));
                    } catch (...) {
                        next = 35;
                    }
                }

                if (next == 65535)
                {
                    next = -1;
                }

                if (midiEnabled && !lowmem)
                {
                    song = next;
                    songFading = true;
                    ondemand->Request(2, song);
                }
                nextSong = next;
            }
             if (chatTyped == "::noclip") {
                 for (int32_t level = 0; level < 4; level++) {
                     for (int32_t x = 1; x < 103; x++) {
                         for (int32_t z = 1; z < 103; z++) {
                             levelCollisionMap[level]->flags[x][z] = 0;
                         }
                     }
                 }
             }
         //}
            if (chatTyped.starts_with("::")) {
                out.WriteOp(103);
                out.Write8(static_cast<int32_t>(chatTyped.length()) - 1);
                out.WriteString(chatTyped.substr(2));
            } else {
                std::string s = StringUtil::ToLower(chatTyped);

                int32_t color = 0;
                if (s.starts_with("yellow:")) {
                    color = 0;
                    chatTyped = chatTyped.substr(7);
                } else if (s.starts_with("red:")) {
                    color = 1;
                    chatTyped = chatTyped.substr(4);
                } else if (s.starts_with("green:")) {
                    color = 2;
                    chatTyped = chatTyped.substr(6);
                } else if (s.starts_with("cyan:")) {
                    color = 3;
                    chatTyped = chatTyped.substr(5);
                } else if (s.starts_with("purple:")) {
                    color = 4;
                    chatTyped = chatTyped.substr(7);
                } else if (s.starts_with("white:")) {
                    color = 5;
                    chatTyped = chatTyped.substr(6);
                } else if (s.starts_with("flash1:")) {
                    color = 6;
                    chatTyped = chatTyped.substr(7);
                } else if (s.starts_with("flash2:")) {
                    color = 7;
                    chatTyped = chatTyped.substr(7);
                } else if (s.starts_with("flash3:")) {
                    color = 8;
                    chatTyped = chatTyped.substr(7);
                } else if (s.starts_with("glow1:")) {
                    color = 9;
                    chatTyped = chatTyped.substr(6);
                } else if (s.starts_with("glow2:")) {
                    color = 10;
                    chatTyped = chatTyped.substr(6);
                } else if (s.starts_with("glow3:")) {
                    color = 11;
                    chatTyped = chatTyped.substr(6);
                }

                s = StringUtil::ToLower(chatTyped);
                int32_t style = 0;

                if (s.starts_with("wave:")) {
                    style = 1;
                    chatTyped = chatTyped.substr(5);
                } else if (s.starts_with("wave2:")) {
                    style = 2;
                    chatTyped = chatTyped.substr(6);
                } else if (s.starts_with("shake:")) {
                    style = 3;
                    chatTyped = chatTyped.substr(6);
                } else if (s.starts_with("scroll:")) {
                    style = 4;
                    chatTyped = chatTyped.substr(7);
                } else if (s.starts_with("slide:")) {
                    style = 5;
                    chatTyped = chatTyped.substr(6);
                }

                out.WriteOp(4);
                out.Write8(0);
                int32_t startPosition = out.position;
                out.Write8S(style);
                out.Write8S(color);
                chatBuffer.position = 0;
                ChatCompression::Pack(chatTyped, chatBuffer);
                out.WriteA(chatBuffer.data, 0, chatBuffer.position);
                out.WriteSize(out.position - startPosition);
                chatTyped = ChatCompression::Format(chatTyped);
                //chatTyped = Censor.filter(chatTyped);
                localPlayer->chat = chatTyped;
                localPlayer->chatColor = color;
                localPlayer->chatStyle = style;
                localPlayer->chatTimer = 150;
                if (rights == 2) {
                    AddMessage(2, "@cr2@" + localPlayer->name, localPlayer->chat);
                } else if (rights == 1) {
                    AddMessage(2, "@cr1@" + localPlayer->name, localPlayer->chat);
                } else {
                    AddMessage(2, localPlayer->name, localPlayer->chat);
                }
                if (publicChatSetting == 2) {
                    publicChatSetting = 3;
                    redrawPrivacySettings = true;
                    out.WriteOp(95);
                    out.Write8(publicChatSetting);
                    out.Write8(privateChatSetting);
                    out.Write8(tradeChatSetting);
                }
            }
            chatTyped = "";
            redrawChatback = true;
        }
    }

    void Game::HandleChatSettingsInput()
    {
        if (mouseClickButton == 1) {
            if (mouseClickX >= 6 && (mouseClickX <= 106) && (mouseClickY >= 467) && (mouseClickY <= 499)) {
                publicChatSetting = (publicChatSetting + 1) % 4;
                redrawPrivacySettings = true;
                redrawChatback = true;
                out.WriteOp(95);
                out.Write8(publicChatSetting);
                out.Write8(privateChatSetting);
                out.Write8(tradeChatSetting);
            }
            if (mouseClickX >= 135 && (mouseClickX <= 235) && (mouseClickY >= 467) && (mouseClickY <= 499)) {
                privateChatSetting = (privateChatSetting + 1) % 3;
                redrawPrivacySettings = true;
                redrawChatback = true;
                out.WriteOp(95);
                out.Write8(publicChatSetting);
                out.Write8(privateChatSetting);
                out.Write8(tradeChatSetting);
            }
            if (mouseClickX >= 273 && (mouseClickX <= 373) && (mouseClickY >= 467) && (mouseClickY <= 499)) {
                tradeChatSetting = (tradeChatSetting + 1) % 3;
                redrawPrivacySettings = true;
                redrawChatback = true;
                out.WriteOp(95);
                out.Write8(publicChatSetting);
                out.Write8(privateChatSetting);
                out.Write8(tradeChatSetting);
            }
            if (mouseClickX >= 412 && (mouseClickX <= 512) && (mouseClickY >= 467) && (mouseClickY <= 499)) {
                UseReportAbuseOption("");
            }
        }
    }

    void Game::SortMenuOptions()
    {
        // The code below pushes menu options with an action greater than 1000 to the bottom, reducing its priority.
        bool done = false;
        while (!done) {
            done = true;
            for (int32_t i = 0; i < (menuSize - 1); i++) {
                if ((menuAction[i] >= 1000) || (menuAction[i + 1] <= 1000)) {
                    continue;
                }

                std::string tmp0 = menuOption[i];
                menuOption[i] = menuOption[i + 1];
                menuOption[i + 1] = tmp0;

                int32_t tmp1 = menuAction[i];
                menuAction[i] = menuAction[i + 1];
                menuAction[i + 1] = tmp1;

                tmp1 = menuParamA[i];
                menuParamA[i] = menuParamA[i + 1];
                menuParamA[i + 1] = tmp1;

                tmp1 = menuParamB[i];
                menuParamB[i] = menuParamB[i + 1];
                menuParamB[i + 1] = tmp1;

                tmp1 = menuParamC[i];
                menuParamC[i] = menuParamC[i + 1];
                menuParamC[i + 1] = tmp1;

                done = false;
            }
        }
    }

    void Game::DrawTooltip()
    {
        if ((menuSize < 2) && (objSelected == 0) && (spellSelected == 0)) {
            return;
        }
        std::string tooltip;

        if ((objSelected == 1) && (menuSize < 2)) {
            tooltip = "Use " + selectedObjName + " with...";
        } else if ((spellSelected == 1) && (menuSize < 2)) {
            tooltip = spellCaption + "...";
        } else {
            tooltip = menuOption[menuSize - 1];
        }

        if (menuSize > 2) {
            tooltip += "@whi@ / " + std::to_string(menuSize - 2) + " more options";
        }
        fontBold12->DrawStringTooltip(tooltip, 4, 15, 0xffffff, true, loopCycle / 1000);
    }

    void Game::HandlePrivateChatInput()
    {
        if (splitPrivateChat == 0) {
            return;
        }

        int32_t count = 0;

        if (systemUpdateTimer != 0) {
            count = 1;
        }

        for (int32_t j = 0; j < 100; j++) {
            if (messageText[j].empty()) {
                continue;
            }

            int32_t type = messageType[j];
            auto& sender = messageSender[j];

            if (!sender.empty() && sender.starts_with("@cr1@")) {
                sender = sender.substr(5);
            }

            if (!sender.empty() && sender.starts_with("@cr2@")) {
                sender = sender.substr(5);
            }

            if ((type == 3 || (type == 7)) && ((type == 7) || (privateChatSetting == 0) || (privateChatSetting == 1 && IsFriend(sender)))) {
                int32_t y = 329 - (count * 13);

                if (mouseX > 4 && (mouseY - 4) > (y - 10) && (mouseY - 4) <= (y + 3)) {
                    int32_t w = fontPlain12->StringWidthTaggable("From:  " + sender + messageText[j]) + 25;

                    if (w > 450) {
                        w = 450;
                    }

                    if (mouseX < (4 + w)) {
                        if (rights >= 1) {
                            AddMenuOption("Report abuse @whi@" + sender, 2606);
                        }
                        AddMenuOption("Add ignore @whi@" + sender, 2042);
                        AddMenuOption("Add friend @whi@" + sender, 2337);
                    }
                }
                if (++count >= 5) {
                    return;
                }
            }

            if ((type == 5 || type == 6) && (privateChatSetting < 2) && (++count >= 5)) {
                return;
            }
        }
    }

    void Game::HandleViewportInput()
    {
        lastHoveredInterfaceID = 0;

        if (mouseX > 4 && mouseY > 4 && (mouseX < 516) && (mouseY < 338))
        {
            if (viewportInterfaceID != -1) {
                HandleInterfaceInput(*IfType::instances[viewportInterfaceID], 4, 4, 0);
            } else {
                HandleViewportOptions();
            }
        }

        if (lastHoveredInterfaceID != viewportHoveredInterfaceID) {
            viewportHoveredInterfaceID = lastHoveredInterfaceID;
        }
    }

    /**
     * Handles an interface action.
     *
     * @param iface the interface.
     * @return <code>false</code> to suppress packet 185.
     */
    bool Game::HandleInterfaceAction(IfType& iface)
    {
        int32_t type = iface.contentType;

        if (friendlistStatus == 2) {
            if (type == 201) {
                redrawChatback = true;
                chatbackInputType = 0;
                showSocialInput = true;
                socialInput = "";
                socialAction = 1;
                socialMessage = "Enter name of friend to add to list";
            }
            if (type == 202) {
                redrawChatback = true;
                chatbackInputType = 0;
                showSocialInput = true;
                socialInput = "";
                socialAction = 2;
                socialMessage = "Enter name of friend to delete from list";
            }
        }
        if (type == 205) {
            idleTimeout = 250;
            return true;
        }
        if (type == 501) {
            redrawChatback = true;
            chatbackInputType = 0;
            showSocialInput = true;
            socialInput = "";
            socialAction = 4;
            socialMessage = "Enter name of player to add to list";
        }
        if (type == 502) {
            redrawChatback = true;
            chatbackInputType = 0;
            showSocialInput = true;
            socialInput = "";
            socialAction = 5;
            socialMessage = "Enter name of player to delete from list";
        }
        if ((type >= 300) && (type <= 313)) {
            int32_t part = (type - 300) / 2;
            int32_t direction = type & 1;
            int32_t kit = designIdentikits[part];

            if (kit != -1) {
                do {
                    if ((direction == 0) && (--kit < 0)) {
                        kit = IdkType::count - 1;
                    }
                    if ((direction == 1) && (++kit >= IdkType::count)) {
                        kit = 0;
                    }
                } while (IdkType::instances[kit].selectable || (IdkType::instances[kit].type != (part + (designGenderMale ? 0 : 7))));

                designIdentikits[part] = kit;
                updateDesignModel = true;
            }
        }
        if ((type >= 314) && (type <= 323)) {
            int32_t part = (type - 314) / 2;
            int32_t direction = type & 1;
            int32_t color = designColors[part];

            if ((direction == 0) && (--color < 0)) {
                color = designPartColor[part].size() - 1;
            }

            if ((direction == 1) && (++color >= designPartColor[part].size())) {
                color = 0;
            }

            designColors[part] = color;
            updateDesignModel = true;
        }
        if ((type == 324) && !designGenderMale) {
            designGenderMale = true;
            ValidateCharacterDesign();
        }
        if ((type == 325) && designGenderMale) {
            designGenderMale = false;
            ValidateCharacterDesign();
        }
        if (type == 326) {
            out.WriteOp(101);
            out.Write8(designGenderMale ? 0 : 1);
            for (int32_t i = 0; i < 7; i++) {
                out.Write8(designIdentikits[i]);
            }
            for (int32_t i = 0; i < 5; i++) {
                out.Write8(designColors[i]);
            }
            return true;
        }
        if ((type >= 601) && (type <= 612)) {
            CloseInterfaces();
            if (!reportAbuseInput.empty()) {
                out.WriteOp(218);
                out.Write64(StringUtil::ToBase37(reportAbuseInput));
                out.Write8(type - 601);
                out.Write8(reportAbuseMuteOption ? 1 : 0);
            }
        }
        return false;
    }

    void Game::HandleViewportOptions()
    {
        if ((objSelected == 0) && (spellSelected == 0)) {
            AddMenuOption("Walk here", 516, static_cast<int32_t>(mouseX), static_cast<int32_t>(mouseY), 0);
        }

        int32_t lastBitset = -1;
        for (int32_t i = 0; i < Model::pickedCount; i++) {
            int32_t bitset = Model::pickedBitsets[i];
            int32_t x = bitset & 0x7f;
            int32_t z = (bitset >> 7) & 0x7f;
            int32_t type = (bitset >> 29) & 3;
            int32_t id = (bitset >> 14) & 0x7fff;

            if (bitset == lastBitset) {
                continue;
            }

            lastBitset = bitset;

            if (type == 2) {
                HandleLocOptions(bitset, x, z, id);
            } else if (type == 1) {
                HandleNPCOptions(x, z, id);
            } else if (type == 0) {
                HandlePlayerOptions(x, z, id);
            } else if (type == 3) {
                HandleObjStackOptions(x, z);
            }
        }
    }

    void Game::HandleLocOptions(int32_t bitset, int32_t x, int32_t z, int32_t id)
    {
        if (scene->GetInfo(currentLevel, x, z, bitset) < 0) {
            return;
        }

        std::shared_ptr<LocType> loc = LocType::Get(id);

        if (!loc->overrideTypeIDs.empty()) {
            loc = loc->GetOverrideType();
        }

        if (loc == nullptr) {
            return;
        }

        if (objSelected == 1) {
            AddMenuOption("Use " + selectedObjName + " with @cya@" + loc->name, 62, x, z, bitset);
        } else if (spellSelected == 1) {
            if ((activeSpellFlags & 4) == 4) {
                AddMenuOption(spellCaption + " @cya@" + loc->name, 956, x, z, bitset);
            }
        } else {
            if (!loc->options.empty()) {
                for (int32_t op = 4; op >= 0; op--) {
                    if (!loc->options[op].empty()) {
                        AddMenuOption(loc->options[op] + " @cya@" + loc->name, LOC_OP_ACTION[op], x, z, bitset);
                    }
                }
            }
            AddMenuOption(std::string("Examine @cya@") + loc->name + " (ID: " + std::to_string(id) + ", X: " + std::to_string(x) + " Y: " + std::to_string(z) + ")",
              1226, x, z, loc->index << 14);
        }
    }

    void Game::HandleNPCOptions(int32_t x, int32_t z, int32_t d)
    {
        const auto& npc = npcs[d];

        if ((npc->type->size == 1) && ((npc->x & 0x7f) == 64) && ((npc->z & 0x7f) == 64)) {
            for (int32_t i = 0; i < npcCount; i++) {
                const auto& other = npcs[npcIDs[i]];
                if ((other != nullptr) && (other != npc) && (other->type->size == 1) && (other->x == npc->x) && (other->z == npc->z)) {
                    AddNPCOptions(other->type, npcIDs[i], z, x);
                }
            }

            for (int32_t i = 0; i < playerCount; i++) {
                const auto& other = players[playerIDs[i]];
                if ((other != nullptr) && (other->x == npc->x) && (other->z == npc->z)) {
                    AddPlayerOptions(x, playerIDs[i], *other, z);
                }
            }
        }

        AddNPCOptions(npc->type, d, z, x);
    }

    void Game::AddNPCOptions(std::shared_ptr<NPCType> type, int32_t npcID, int32_t tileZ, int32_t tileX)
    {
        if (menuSize >= 400) {
            return;
        }

        if (!type->overrides.empty()) {
            type = type->GetOverrideType();
        }

        if (type == nullptr) {
            return;
        }

        if (!type->interactable) {
            return;
        }

        std::string text = type->name;

        if (type->level != 0) {
            text = text + GetCombatLevelColorTag(localPlayer->combatLevel, type->level) +
                " (level-" + std::to_string(type->level) + ")";
        }

        if (objSelected == 1) {
            AddMenuOption("Use " + selectedObjName + " with @yel@" + text, 582, tileX, tileZ, npcID);
            return;
        }

        if (spellSelected == 1) {
            if ((activeSpellFlags & 2) == 2) {
                AddMenuOption(spellCaption + " @yel@" + text, 413, tileX, tileZ, npcID);
            }
        } else {
        if (!type->options.empty()) {
            for (int32_t option = 4; option >= 0; option--) {
                if ((!type->options[option].empty()) && !StringUtil::EqualsIgnoreCase(type->options[option], "attack")) {
                    AddMenuOption(type->options[option] + " @yel@" + text, NPC_OP_ACTION[option], tileX, tileZ, npcID);
                }
            }
        }

        if (!type->options.empty()) {
            for (int32_t option = 4; option >= 0; option--) {
                if ((!type->options[option].empty()) && StringUtil::EqualsIgnoreCase(type->options[option], "attack")) {
                    int32_t offset = 0;
                    if (type->level > localPlayer->combatLevel) {
                        offset = 2000;
                    }
                    AddMenuOption(type->options[option] + " @yel@" + text, NPC_OP_ACTION[option] + offset, tileX, tileZ, npcID);
                }
            }
            }

            AddMenuOption("Examine @yel@" + text, 1025, tileX, tileZ, npcID);
        }
    }

    void Game::HandlePlayerOptions(int32_t x, int32_t z, int32_t id)
    {
        auto& player = players[id];

        if (((player->x & 0x7f) == 64) && ((player->z & 0x7f) == 64)) {
            for (int32_t i = 0; i < npcCount; i++) {
                auto& other = npcs[npcIDs[i]];
                if ((other != nullptr) && (other->type->size == 1) && (other->x == player->x) && (other->z == player->z)) {
                    AddNPCOptions(other->type, npcIDs[i], z, x);
                }
            }

            for (int32_t i = 0; i < playerCount; i++) {
                auto& other = players[playerIDs[i]];
                if ((other != nullptr) && (other != player) && (other->x == player->x) && (other->z == player->z)) {
                    AddPlayerOptions(x, playerIDs[i], *other, z);
                }
            }
        }

        AddPlayerOptions(x, id, *player, z);
    }

    void Game::HandleObjStackOptions(int32_t x, int32_t z)
    {
        auto& list = levelObjStacks[currentLevel][x][z];
        if (list.isEmpty()) {
            return;
        }

        for (auto* node = list.peekBack(); node != nullptr; node = list.next()) {
            auto* obj = static_cast<ObjEntity*>(node);
            const auto& type = ObjType::Get(obj->id);

            if (objSelected == 1) {
                AddMenuOption("Use " + selectedObjName + " with @lre@" + type->name, 511, x, z, obj->id);
            } else if (spellSelected == 1) {
                if ((activeSpellFlags & 1) == 1) {
                    AddMenuOption(spellCaption + " @lre@" + type->name, 94, x, z, obj->id);
                }
            } else {
                for (int32_t op = 4; op >= 0; op--) {
                    if ((!type->options.empty()) && (!type->options[op].empty())) {
                        AddMenuOption(type->options[op] + " @lre@" + type->name, OBJ_OP_ACTION[op], x, z, obj->id);
                    } else if (op == 2) {
                        AddMenuOption("Take @lre@" + type->name, 234, x, z, obj->id);
                    }
                }

                AddMenuOption("Examine @lre@" + type->name, 1448, x, z, obj->id);
            }
        }
    }

    void Game::AddPlayerOptions(int32_t tileX, int32_t playerID, PlayerEntity& player, int32_t tileZ)
    {
        if (&player == localPlayer.get()) {
            return;
        }

        if (menuSize >= 400) {
            return;
        }

        std::string caption;

        if (player.skillLevel == 0) {
            caption = player.name + GetCombatLevelColorTag(localPlayer->combatLevel, player.combatLevel) + " (level-" + std::to_string(player.combatLevel) + ")";
        } else {
            caption = player.name + " (skill-" + std::to_string(player.skillLevel) + ")";
        }

        if (objSelected == 1) {
            AddMenuOption("Use " + selectedObjName + " with @whi@" + caption, 491, tileX, tileZ, playerID);
        } else if (spellSelected == 1) {
            if ((activeSpellFlags & 8) == 8) {
                AddMenuOption(spellCaption + " @whi@" + caption, 365, tileX, tileZ, playerID);
            }
        } else {
            for (int32_t option = 4; option >= 0; option--) {
                if (playerOptions[option].empty()) {
                    continue;
                }
                int32_t offset = 0;

                if (StringUtil::EqualsIgnoreCase(playerOptions[option], "attack")) {
                    if (player.combatLevel > localPlayer->combatLevel) {
                        offset = 2000;
                    }

                    if ((localPlayer->team != 0) && (player.team != 0)) {
                        if (localPlayer->team == player.team) {
                            offset = 2000;
                        } else {
                            offset = 0;
                        }
                    }
                } else if (playerOptionPushDown[option]) {
                    offset = 2000;
                }

                int32_t action = 0;

                if (option == 0) {
                    action = 561;
                } else if (option == 1) {
                    action = 779;
                } else if (option == 2) {
                    action = 27;
                } else if (option == 3) {
                    action = 577;
                } else if (option == 4) {
                    action = 729;
                }

                action += offset;

                AddMenuOption(playerOptions[option] + " @whi@" + caption, action, tileX, tileZ, playerID);
            }
        }
        for (int32_t i = 0; i < menuSize; i++) {
            if (menuAction[i] == 516) {
                menuOption[i] = "Walk here @whi@" + caption;
                return;
            }
        }
    }

    void Game::ExamineLoc(int32_t bitset)
    {
        int32_t locID = (bitset >> 14) & 0x7fff;
        const auto& type = LocType::Get(locID);
        std::string message;
        if (!type->examine.empty()) {
            message = type->examine;
        } else {
            message = "It's a " + type->name + ".";
        }
        AddMessage(0, "", message);
    }

    bool Game::TryMove(int32_t type, int32_t srcX, int32_t srcZ, int32_t dx, int32_t dz, int32_t locType,
        int32_t locWidth, int32_t locLength, int32_t locAngle, int32_t locInteractionFlags, bool tryNearest)
    {
        int8_t sceneWidth = 104;
        int8_t sceneLength = 104;
        for (int32_t x = 0; x < sceneWidth; x++) {
            for (int32_t z = 0; z < sceneLength; z++) {
                bfsDirection[x][z] = 0;
                bfsCost[x][z] = 99999999;
            }
        }

        int32_t x = srcX;
        int32_t z = srcZ;

        bfsDirection[srcX][srcZ] = 99;
        bfsCost[srcX][srcZ] = 0;

        int32_t steps = 0;
        int32_t length = 0;

        bfsStepX[steps] = srcX;
        bfsStepZ[steps++] = srcZ;

        bool arrived = false;
        auto bufferSize = static_cast<int32_t>(bfsStepX.size());
        auto& flags = levelCollisionMap[currentLevel]->flags;

        while (length != steps)
        {
            x = bfsStepX[length];
            z = bfsStepZ[length];
            length = (length + 1) % bufferSize;

            if ((x == dx) && (z == dz)) {
                arrived = true;
                break;
            }

            if (locType != 0) {
                if (((locType < 5) || (locType == 10)) && levelCollisionMap[currentLevel]->ReachedDestination(x, z, dx, dz, locAngle, locType - 1)) {
                    arrived = true;
                    break;
                }
                if ((locType < 10) && levelCollisionMap[currentLevel]->ReachedWall(x, z, dx, dz, locType - 1, locAngle)) {
                    arrived = true;
                    break;
                }
            }

            if ((locWidth != 0) && (locLength != 0) && levelCollisionMap[currentLevel]->ReachedLoc(x, z, dx, dz, locWidth, locLength, locInteractionFlags)) {
                arrived = true;
                break;
            }

            int32_t nextCost = bfsCost[x][z] + 1;

            if ((x > 0) && (bfsDirection[x - 1][z] == 0) && ((flags[x - 1][z] & 0x1280108) == 0)) {
                bfsStepX[steps] = x - 1;
                bfsStepZ[steps] = z;
                steps = (steps + 1) % bufferSize;
                bfsDirection[x - 1][z] = 2;
                bfsCost[x - 1][z] = nextCost;
            }

            if ((x < (sceneWidth - 1)) && (bfsDirection[x + 1][z] == 0) && ((flags[x + 1][z] & 0x1280180) == 0)) {
                bfsStepX[steps] = x + 1;
                bfsStepZ[steps] = z;
                steps = (steps + 1) % bufferSize;
                bfsDirection[x + 1][z] = 8;
                bfsCost[x + 1][z] = nextCost;
            }

            if ((z > 0) && (bfsDirection[x][z - 1] == 0) && ((flags[x][z - 1] & 0x1280102) == 0)) {
                bfsStepX[steps] = x;
                bfsStepZ[steps] = z - 1;
                steps = (steps + 1) % bufferSize;
                bfsDirection[x][z - 1] = 1;
                bfsCost[x][z - 1] = nextCost;
            }

            if ((z < (sceneLength - 1)) && (bfsDirection[x][z + 1] == 0) && ((flags[x][z + 1] & 0x1280120) == 0)) {
                bfsStepX[steps] = x;
                bfsStepZ[steps] = z + 1;
                steps = (steps + 1) % bufferSize;
                bfsDirection[x][z + 1] = 4;
                bfsCost[x][z + 1] = nextCost;
            }

            if ((x > 0) && (z > 0) && (bfsDirection[x - 1][z - 1] == 0) && ((flags[x - 1][z - 1] & 0x128010e) == 0) && ((flags[x - 1][z] & 0x1280108) == 0) && ((flags[x][z - 1] & 0x1280102) == 0)) {
                bfsStepX[steps] = x - 1;
                bfsStepZ[steps] = z - 1;
                steps = (steps + 1) % bufferSize;
                bfsDirection[x - 1][z - 1] = 3;
                bfsCost[x - 1][z - 1] = nextCost;
            }

            if ((x < (sceneWidth - 1)) && (z > 0) && (bfsDirection[x + 1][z - 1] == 0) && ((flags[x + 1][z - 1] & 0x1280183) == 0) && ((flags[x + 1][z] & 0x1280180) == 0) && ((flags[x][z - 1] & 0x1280102) == 0)) {
                bfsStepX[steps] = x + 1;
                bfsStepZ[steps] = z - 1;
                steps = (steps + 1) % bufferSize;
                bfsDirection[x + 1][z - 1] = 9;
                bfsCost[x + 1][z - 1] = nextCost;
            }

            if ((x > 0) && (z < (sceneLength - 1)) && (bfsDirection[x - 1][z + 1] == 0) && ((flags[x - 1][z + 1] & 0x1280138) == 0) && ((flags[x - 1][z] & 0x1280108) == 0) && ((flags[x][z + 1] & 0x1280120) == 0)) {
                bfsStepX[steps] = x - 1;
                bfsStepZ[steps] = z + 1;
                steps = (steps + 1) % bufferSize;
                bfsDirection[x - 1][z + 1] = 6;
                bfsCost[x - 1][z + 1] = nextCost;
            }

            if ((x < (sceneWidth - 1)) && (z < (sceneLength - 1)) && (bfsDirection[x + 1][z + 1] == 0) && ((flags[x + 1][z + 1] & 0x12801e0) == 0) && ((flags[x + 1][z] & 0x1280180) == 0) && ((flags[x][z + 1] & 0x1280120) == 0)) {
                bfsStepX[steps] = x + 1;
                bfsStepZ[steps] = z + 1;
                steps = (steps + 1) % bufferSize;
                bfsDirection[x + 1][z + 1] = 12;
                bfsCost[x + 1][z + 1] = nextCost;
            }
        }

        tryMoveNearest = 0;

        if (!arrived) {
            if (tryNearest) {
                int32_t min = 100;
                for (int32_t padding = 1; padding < 2; padding++) {
                    for (int32_t px = dx - padding; px <= (dx + padding); px++) {
                        for (int32_t pz = dz - padding; pz <= (dz + padding); pz++) {
                            if ((px >= 0) && (pz >= 0) && (px < 104) && (pz < 104) && (bfsCost[px][pz] < min)) {
                                min = bfsCost[px][pz];
                                x = px;
                                z = pz;
                                tryMoveNearest = 1;
                                arrived = true;
                            }
                        }
                    }
                    if (arrived) {
                        break;
                    }
                }
            }
            if (!arrived) {
                return false;
            }
        }

        length = 0;
        bfsStepX[length] = x;
        bfsStepZ[length++] = z;

        int32_t dir = bfsDirection[x][z];
        int32_t next = dir;

        // build our path into bfsStepX/Z starting from our destination.
        // bfsStep[0->n] (dst->src)
        while ((x != srcX) || (z != srcZ)) {
            if (next != dir) {
                dir = next;
                bfsStepX[length] = x;
                bfsStepZ[length++] = z;
            }

            if ((next & 2) != 0) {
                x++;
            } else if ((next & 8) != 0) {
                x--;
            }

            if ((next & 1) != 0) {
                z++;
            } else if ((next & 4) != 0) {
                z--;
            }

            next = bfsDirection[x][z];
        }

        if (length > 0) {
            int32_t count = length;

            // a move packet is limited to 25 steps
            if (count > 25) {
                count = 25;
            }

            length--;

            int32_t startX = bfsStepX[length];
            int32_t startZ = bfsStepZ[length];

            if (type == 0) {
                out.WriteOp(164);
                out.Write8(count + count + 3);
            } else if (type == 1) {
                out.WriteOp(248);
                out.Write8(count + count + 3 + 14);
            } else if (type == 2) {
                out.WriteOp(98);
                out.Write8(count + count + 3);
            }

            out.Write16LEA(startX + sceneBaseTileX);

            flagSceneTileX = bfsStepX[0];
            flagSceneTileZ = bfsStepZ[0];

            for (int32_t i = 1; i < count; i++) {
                length--;
                out.Write8(bfsStepX[length] - startX);
                out.Write8(bfsStepZ[length] - startZ);
            }

            out.Write16LE(startZ + sceneBaseTileZ);
            out.Write8C((actionKey[5] != 1) ? 0 : 1);
            return true;
        }

        return type != 1;
    }

    std::unique_ptr<Connection> Game::OpenURL(const std::string& s)
    {
        if (!jaggrabEnabled) {
            return nullptr;
        }

        if (jaggrabSocket != nullptr) {
            NET_DestroyStreamSocket(jaggrabSocket);
            jaggrabSocket = nullptr;
        }

        NET_Address* jaggrabAddr = NET_ResolveHostname(server.c_str());
        if (jaggrabAddr == nullptr) {
            return nullptr;
        }

        if (NET_WaitUntilResolved(jaggrabAddr, 10000) != NET_SUCCESS) {
            NET_UnrefAddress(jaggrabAddr);
            return nullptr;
        }

        jaggrabSocket = NET_CreateClient(jaggrabAddr, 43595 + portOffset);
        NET_UnrefAddress(jaggrabAddr);

        if (jaggrabSocket == nullptr) {
            return nullptr;
        }

        if (NET_WaitUntilConnected(jaggrabSocket, 10000) != NET_SUCCESS) {
            NET_DestroyStreamSocket(jaggrabSocket);
            jaggrabSocket = nullptr;
            return nullptr;
        }

        auto jaggrabConnection = std::make_unique<Connection>(jaggrabSocket);
        jaggrabSocket = nullptr;

        std::string request = "JAGGRAB /" + s + "\n\n";
        std::vector<int8_t> requestData(request.begin(), request.end());
        jaggrabConnection->Write(requestData, 0, static_cast<int32_t>(requestData.size()));
        jaggrabConnection->Flush();

        return jaggrabConnection;
    }

    void Game::TryReconnect()
    {
        if (idleTimeout > 0) {
            Logout();
            return;
        }
        areaViewport->Bind();
        fontPlain12->DrawStringCenter("Connection lost", 257, 144, 0);
        fontPlain12->DrawStringCenter("Connection lost", 256, 143, 0xffffff);
        fontPlain12->DrawStringCenter("Please wait - attempting to reestablish", 257, 159, 0);
        fontPlain12->DrawStringCenter("Please wait - attempting to reestablish", 256, 158, 0xffffff);
        areaViewport->Draw(drawSurface, 4, 4);
        minimapState = 0;
        flagSceneTileX = 0;
        ingame = false;
        loginAttempts = 0;
        Login(username, password, true);

        if (!ingame) {
            Logout();
        }

        if (connection) {
            connection->Close();
        }
    }

    bool Game::Read()
    {
        if (!connection || connection->Closed()) {
            return false;
        }

        int32_t available = connection->Available();

        if (available == 0) {
            return false;
        }

        if (packetType == -1) {
            connection->Read(in.data, 0, 1);
            packetType = in.data[0] & 0xff;
            packetType = (packetType - randomIn.GetNextKey()) & 0xff;
            packetSize = PacketIn::SIZE[packetType];
            bytesIn++;
            available--;
        }

        if (packetSize == -1) {
            if (available > 0) {
                connection->Read(in.data, 0, 1);
                packetSize = in.data[0] & 0xff;
                available--;
            } else {
                return false;
            }
        }

        if (packetSize == -2) {
            if (available > 1) {
                connection->Read(in.data, 0, 2);
                in.position = 0;
                packetSize = in.ReadU16();
                bytesIn += 2;
                available -= 2;
            } else {
                return false;
            }
        }

        if (available < packetSize) {
            return false;
        }

        in.position = 0;
        connection->Read(in.data, 0, packetSize);

        idleNetCycles = 0;
        lastPacketType2 = lastPacketType1;
        lastPacketType1 = lastPacketType0;
        lastPacketType0 = packetType;
        bytesIn += packetSize;

        switch (packetType)
        {
        case PacketIn::SYNC_PLAYERS:
            ReadSyncPlayers();
            break;

        case PacketIn::LAST_LOGIN_INFO:
            ReadLastLoginInfo();
            break;

        case PacketIn::ZONE_CLEAR:
            ReadZoneClear();
            break;

        case PacketIn::IF_SETPLAYERHEAD:
            ReadIfSetPlayerHead();
            break;

        case PacketIn::CAM_RESET:
            ReadCameraReset();
            break;

        case PacketIn::INV_CLEAR:
            ReadInventoryClear();
            break;

        case PacketIn::IGNORE_LIST:
            ReadIgnoreList();
            break;

        case PacketIn::CAM_SETPOS:
            ReadCameraSetPos();
            break;

        case PacketIn::CAM_LOOKAT:
            ReadCameraLookAt();
            break;

        case PacketIn::UPDATE_STAT:
            ReadUpdateStat();
            break;

        case PacketIn::IF_TAB:
            ReadIfTab();
            break;
        case PacketIn::MIDI_SONG:
            ReadMidiSong();
            break;

        case PacketIn::MIDI_JINGLE:
            ReadMidiJingle();
            break;

        case PacketIn::LOGOUT:
            Logout();
            break;

        case PacketIn::IF_SETPOSITION:
            ReadIfSetPosition();
            break;
        case PacketIn::REBUILD_REGION:
        case PacketIn::REBUILD_REGION_INSTANCE:
            ReadRebuildRegion();
            break;
        case PacketIn::IF_VIEWPORT_OVERLAY:
            ReadIfViewportOverlay();
            break;
        case PacketIn::MINIMAP_TOGGLE:
            minimapState = in.ReadU8();
            break;
        case PacketIn::IF_SETNPCHEAD:
            ReadIfSetNPCHead();
            break;

        case PacketIn::UPDATE_REBOOT_TIMER:
            systemUpdateTimer = in.ReadU16LE() * 30;
            break;

        case PacketIn::ZONE_UPDATE:
            ReadZoneUpdate();
            break;

        case PacketIn::CAM_SHAKE:
            ReadCameraShake();
            break;

        case PacketIn::SYNTH_SOUND:
            ReadSynthSound();
            break;

        case PacketIn::SET_PLAYER_OP:
            ReadSetPlayerOp();
            break;

        case PacketIn::CLEAR_MAP_FLAG:
            flagSceneTileX = 0;
            break;

        case PacketIn::MESSAGE_GAME:
            ReadMessageGame();
            break;

        case PacketIn::RESET_ANIMS:
            ResetAnimations();
            break;

        case PacketIn::FRIEND_STATUS:
            ReadFriendStatus();
            break;

        case PacketIn::UPDATE_RUNENERGY:
            ReadUpdateRunEnergy();
            break;

        case PacketIn::HINT_ARROW:
            ReadHintArrow();
            break;

        case PacketIn::IF_VIEWPORT_AND_SIDEBAR:
            ReadIfViewportAndSidebar();
            break;

        case PacketIn::IF_SETSCROLLPOS:
            ReadIfSetScrollPos();
            break;

        case PacketIn::RESET_CLIENT_VARCACHE:
            RestoreVarCache();
            break;

        case PacketIn::MESSAGE_PUBLIC:
            ReadMessagePublic();
            break;

        case PacketIn::ZONE_BASE:
            baseZ = in.ReadU8C();
            baseX = in.ReadU8C();
            break;

        case PacketIn::TAB_HINT:
            ReadTabHint();
            break;

        case PacketIn::IF_SETOBJECT:
            ReadIfSetObject();
            break;

        case PacketIn::IF_SETHIDE:
            ReadIfSetHide();
            break;

        case PacketIn::IF_STOPANIM:
            ReadIfStopAnim();
            break;

        case PacketIn::IF_SETTEXT:
            ReadIfSetText();
            break;

        case PacketIn::CHAT_FILTER_SETTINGS:
            ReadChatFilterSettings();
            break;

        case PacketIn::UPDATE_RUNWEIGHT:
            ReadUpdateRunWeight();
            break;

        case PacketIn::IF_SETMODEL:
            ReadIfSetModel();
            break;

        case PacketIn::IF_SETCOLOR:
            ReadIfSetColor();
            break;

        case PacketIn::UPDATE_INV_FULL:
            ReadUpdateInvFull();
            break;

        case PacketIn::IF_SETANGLE:
            ReadIfSetAngle();
            break;

        case PacketIn::FRIENDLIST_LOADED:
            friendlistStatus = in.ReadU8();
            redrawSidebar = true;
            break;

        case PacketIn::LOCAL_PLAYER:
            isMember = in.ReadU8A();
            localPID = in.ReadU16LEA();
            break;

        case PacketIn::SYNC_NPCS:
            ReadSyncNPCs();
            break;

        case PacketIn::INPUT_AMOUNT:
            OpenChatInput(1);
            break;

        case PacketIn::INPUT_NAME:
            OpenChatInput(2);
            break;

        case PacketIn::IF_VIEWPORT:
            ReadViewportInterface();
            break;

        case PacketIn::IF_CHAT_STICKY:
            stickyChatInterfaceID = in.Read16LEA();
            redrawChatback = true;
            break;

        case PacketIn::VARP_LARGE:
            ReadVarpLarge();
            break;

        case PacketIn::VARP_SMALL:
            ReadVarpSmall();
            break;

        case PacketIn::MULTIZONE:
            multizone = in.ReadU8();
            break;

        case PacketIn::IF_SETANIM:
            ReadIfSetAnim();
            break;

        case PacketIn::IF_CLOSE:
            OpenViewportInterface(-1);
            break;

        case PacketIn::UPDATE_INV_PARTIAL:
            ReadUpdateInvPartial();
            break;

        case PacketIn::TAB_SELECTED:
            ReadTabSelected();
            break;

        case PacketIn::IF_CHAT:
            ReadIfChat();
            break;

        case PacketIn::OBJ_ADD:
        case PacketIn::OBJ_REVEAL:
        case PacketIn::OBJ_COUNT:
        case PacketIn::OBJ_DEL:
        case PacketIn::LOC_ADD:
        case PacketIn::LOC_CHANGE:
        case PacketIn::LOC_DEL:
        case PacketIn::MAP_SOUND:
        case PacketIn::LOC_PLAYER:
        case PacketIn::MAP_ANIM:
        case PacketIn::MAP_PROJECTILE:
            ReadZonePacket(packetType);
            break;
        default:
            LOG_ERROR("T1 (Unhandled Packet Type) - %i,%i - %i,%i", packetType, packetSize, lastPacketType1, lastPacketType2);
            Logout();
            break;
        }

        packetType = -1;
        return true;
    }

    void Game::ReadSyncPlayers()
    {
        entityRemovalCount = 0;
        entityUpdateCount = 0;

        ReadLocalPlayer();
        ReadPlayers();
        ReadNewPlayers();
        ReadPlayerUpdates();

        for (int32_t i = 0; i < entityRemovalCount; i++) {
            int32_t id = entityRemovalIDs[i];
            if (players[id]->cycle != loopCycle) {
                players[id] = nullptr;
            }
        }

        if (in.position != packetSize) {
            LOG_ERROR("Error packet size mismatch in getplayer pos: %i psize: %i", in.position, packetSize);
        }

        for (int32_t i = 0; i < playerCount; i++) {
            if (players[playerIDs[i]] == nullptr) {
                LOG_ERROR("%s null entry in pl list - pos: %i size: %i", username.c_str(), i, playerCount);
            }
        }

        awaitingSync = false;
    }

    void Game::ReadLastLoginInfo()
    {
        daysSinceRecoveriesChanged = in.ReadU8C();
        unreadMessages = in.ReadU16A();
        warnMembersInNonMembers = in.ReadU8();
        lastAddress = in.Read32ME();
        daysSinceLastLogin = in.ReadU16();

        if ((lastAddress != 0) && (viewportInterfaceID == -1)) {
            Signlink::DNSLookup(StringUtil::FormatIPv4(lastAddress));
            CloseInterfaces();

            int32_t reportAbuseContentType = 650;

            if ((daysSinceRecoveriesChanged != 201) || (warnMembersInNonMembers == 1)) {
                reportAbuseContentType = 655;
            }

            reportAbuseInput = "";
            reportAbuseMuteOption = false;

            for (int32_t i = 0; i < IfType::instances.size(); i++) {
                if ((IfType::instances[i] == nullptr) || (IfType::instances[i]->contentType != reportAbuseContentType)) {
                    continue;
                }
                viewportInterfaceID = IfType::instances[i]->parentID;
                break;
            }
        }
    }

    void Game::ReadLocalPlayer()
    {
        in.AccessBits();

        if (in.ReadN(1) == 0) {
            return;
        }

        int32_t type = in.ReadN(2);

        if (type == 0) {
            entityUpdateIDs[entityUpdateCount++] = LOCAL_PLAYER_INDEX;
        } else if (type == 1) {
            localPlayer->Step(false, in.ReadN(3));

            if (in.ReadN(1) == 1) {
                entityUpdateIDs[entityUpdateCount++] = LOCAL_PLAYER_INDEX;
            }
        } else if (type == 2) {
            localPlayer->Step(true, in.ReadN(3));
            localPlayer->Step(true, in.ReadN(3));

            if (in.ReadN(1) == 1) {
                entityUpdateIDs[entityUpdateCount++] = LOCAL_PLAYER_INDEX;
            }
        } else if (type == 3) {
            currentLevel = in.ReadN(2);
            const int32_t teleport = in.ReadN(1);

            if (in.ReadN(1) == 1) {
                entityUpdateIDs[entityUpdateCount++] = LOCAL_PLAYER_INDEX;
            }

            int32_t z = in.ReadN(7);
            int32_t x = in.ReadN(7);
            localPlayer->Move(x, z, teleport == 1);
        }
    }

    void Game::ReadIfSetPlayerHead()
    {
        int32_t interfaceID = in.ReadU16LEA();
        const auto& iface = IfType::instances[interfaceID];
        iface->modelType = IfType::MODEL_TYPE_PLAYER;

        if (localPlayer->transmogrify == nullptr) {
            iface->modelID = (localPlayer->colors[0] << 25) + (localPlayer->colors[4] << 20) +
                (localPlayer->appearances[0] << 15) + (localPlayer->appearances[8] << 10) +
                    (localPlayer->appearances[11] << 5) + localPlayer->appearances[1];
        } else {
            iface->modelID = static_cast<int32_t>(0x12345678L + localPlayer->transmogrify->uid);
        }
    }

    void Game::ReadPlayers()
    {
        int32_t count = in.ReadN(8);
        if (count < playerCount) {
            for (int32_t i = count; i < playerCount; i++) {
                entityRemovalIDs[entityRemovalCount++] = playerIDs[i];
            }
        }

        if (count > playerCount) {
            LOG_ERROR("%s Too many players", username.c_str());
        }

        playerCount = 0;

        for (int32_t i = 0; i < count; i++) {
            int32_t id = playerIDs[i];
            auto& player = players[id];

            if (in.ReadN(1) == 0) {
                playerIDs[playerCount++] = id;
                player->cycle = loopCycle;
            } else {
                int32_t type = in.ReadN(2);

                if (type == 0) {
                    playerIDs[playerCount++] = id;
                    player->cycle = loopCycle;

                    entityUpdateIDs[entityUpdateCount++] = id;
                } else if (type == 1) {
                    playerIDs[playerCount++] = id;
                    player->cycle = loopCycle;

                    player->Step(false, in.ReadN(3));

                    if (in.ReadN(1) == 1) {
                        entityUpdateIDs[entityUpdateCount++] = id;
                    }
                } else if (type == 2) {
                    playerIDs[playerCount++] = id;
                    player->cycle = loopCycle;

                    player->Step(true, in.ReadN(3));
                    player->Step(true, in.ReadN(3));

                    if (in.ReadN(1) == 1) {
                        entityUpdateIDs[entityUpdateCount++] = id;
                    }
                } else if (type == 3) {
                    entityRemovalIDs[entityRemovalCount++] = id;
                }
            }
        }
    }

    void Game::ReadNewPlayers()
    {
        while ((in.bitPosition + 10) < (packetSize * 8)) {
            int32_t id = in.ReadN(11);

            if (id == 2047) {
                break;
            }

            if (players[id] == nullptr) {
                players[id] = std::make_shared<PlayerEntity>();
                if (playerAppearanceBuffer[id].position > 0) {
                    players[id]->Read(playerAppearanceBuffer[id]);
                }
            }

            playerIDs[playerCount++] = id;
            auto& player = players[id];
            player->cycle = loopCycle;

            if (in.ReadN(1) == 1) {
                entityUpdateIDs[entityUpdateCount++] = id;
            }

            int32_t teleport = in.ReadN(1);
            int32_t z = in.ReadN(5);
            int32_t x = in.ReadN(5);

            if (z > 15) {
                z -= 32;
            }

            if (x > 15) {
                x -= 32;
            }

            player->Move(localPlayer->pathTileX[0] + x, localPlayer->pathTileZ[0] + z, teleport == 1);
        }
        in.AccessBytes();
    }

    void Game::ReadPlayerUpdates()
    {
        for (int32_t i = 0; i < entityUpdateCount; i++) {
            int32_t playerID = entityUpdateIDs[i];
            auto& player = players[playerID];
            int32_t updates = in.ReadU8();

            if ((updates & 0x40) != 0) {
                updates += in.ReadU8() << 8;
            }

            if ((updates & 0x400) != 0) {
                ReadPlayerForceMovement(*player);
            }

            if ((updates & 0x100) != 0) {
                ReadPlayerGraphic(*player);
            }

            if ((updates & 8) != 0) {
                ReadPlayerAnimation(*player);
            }

            if ((updates & 4) != 0) {
                ReadPlayerChatForced(*player);
            }

            if ((updates & 0x80) != 0) {
                ReadPlayerChat(*player);
            }

            if ((updates & 1) != 0) {
                ReadPlayerTargetEntity(*player);
            }

            if ((updates & 0x10) != 0) {
                ReadPlayerAppearance(playerID, *player);
            }

            if ((updates & 2) != 0) {
                ReadPlayerTargetTile(*player);
            }

            if ((updates & 0x20) != 0) {
                ReadPlayerDamage0(*player);
            }

            if ((updates & 0x200) != 0) {
                ReadPlayerDamage1(*player);
            }
        }
    }

    void Game::ReadPlayerForceMovement(PlayerEntity& player)
    {
        player.forceMoveStartSceneTileX = in.ReadU8S();
        player.forceMoveStartSceneTileZ = in.ReadU8S();
        player.forceMoveEndSceneTileX = in.ReadU8S();
        player.forceMoveEndSceneTileZ = in.ReadU8S();
        player.forceMoveEndCycle = in.ReadU16LEA() + loopCycle;
        player.forceMoveStartCycle = in.ReadU16A() + loopCycle;
        player.forceMoveFaceDirection = in.ReadU8S();
        player.ResetPath();
    }

    void Game::ReadPlayerGraphic(PlayerEntity& player)
    {
        player.spotanimID = in.ReadU16LE();
        player.spotanimOffset = in.ReadU16();
        player.spotanimLastCycle = loopCycle + in.ReadU16();
        player.spotanimFrame = 0;
        player.spotanimCycle = 0;

        if (player.spotanimLastCycle > loopCycle) {
            player.spotanimFrame = -1;
        }

        if (player.spotanimID == 65535) {
            player.spotanimID = -1;
        }
    }

    void Game::ReadPlayerAnimation(PlayerEntity& player)
    {
        int32_t seqID = in.ReadU16LE();

        if (seqID == 65535) {
            seqID = -1;
        }

        int32_t delay = in.ReadU8C();

        if ((seqID == player.primarySeqID) && (seqID != -1)) {
            int32_t style = SeqType::instances[seqID].replayStyle;

            if (style == 1) {
                player.primarySeqFrame = 0;
                player.primarySeqCycle = 0;
                player.primarySeqDelay = delay;
                player.primarySeqLoop = 0;
            }

            if (style == 2) {
                player.primarySeqLoop = 0;
            }
        } else if ((seqID == -1) || (player.primarySeqID == -1)
            || (SeqType::instances[seqID].priority >= SeqType::instances[player.primarySeqID].priority)) {
            player.primarySeqID = seqID;
            player.primarySeqFrame = 0;
            player.primarySeqCycle = 0;
            player.primarySeqDelay = delay;
            player.primarySeqLoop = 0;
            player.seqPathLength = player.pathLength;
            }
    }

    void Game::ReadPlayerChatForced(PlayerEntity& player)
    {
        player.chat = in.ReadString();

        if (!player.chat.empty() && player.chat[0] == '~') {
            player.chat = player.chat.substr(1);
            AddMessage(2, player.name, player.chat);
        } else if (&player == localPlayer.get()) {
            AddMessage(2, player.name, player.chat);
        }

        player.chatColor = 0;
        player.chatStyle = 0;
        player.chatTimer = 150;
    }

    void Game::ReadPlayerChat(PlayerEntity& player)
    {
        int32_t colorStyle = in.ReadU16LE();
        int32_t role = in.ReadU8();
        int32_t length = in.ReadU8C();
        int32_t start = in.position;

        if (!player.name.empty() && player.visible) {
            int64_t name37 = StringUtil::ToBase37(player.name);
            bool ignore = false;

            if (role <= 1) {
                for (int32_t i = 0; i < ignoreCount; i++) {
                    if (ignoreName37[i] != name37) {
                        continue;
                    }
                    ignore = true;
                    break;
                }
            }

            if (!ignore && (overrideChat == 0)) {
                chatBuffer.position = 0;
                in.ReadReversed(chatBuffer.data, 0, length);
                chatBuffer.position = 0;

                const auto& chat = ChatCompression::Unpack(length, chatBuffer);
                //chat = Censor.filter(chat);

                player.chat = chat;
                player.chatColor = colorStyle >> 8;
                player.chatStyle = colorStyle & 0xff;
                player.chatTimer = 150;

                if ((role == 2) || (role == 3)) {
                    AddMessage(1, "@cr2@" + player.name, chat);
                } else if (role == 1) {
                    AddMessage(1, "@cr1@" + player.name, chat);
                } else {
                    AddMessage(2, player.name, chat);
                }
            }
        }

        in.position = start + length;
    }

    void Game::ReadPlayerAppearance(int32_t playerID, PlayerEntity& player)
    {
        auto buffer = Buffer(in.ReadU8C());
        in.Read(buffer.data);
        playerAppearanceBuffer[playerID] = buffer;
        player.Read(buffer);
    }

    void Game::ReadPlayerTargetTile(PlayerEntity& player)
    {
        player.targetTileX = in.ReadU16LEA();
        player.targetTileZ = in.ReadU16LE();
    }

    void Game::ReadPlayerTargetEntity(PlayerEntity& player)
    {
        player.targetID = in.ReadU16LE();

        if (player.targetID == 65535) {
            player.targetID = -1;
        }
    }

    void Game::ReadPlayerDamage0(PlayerEntity& player)
    {
        int32_t damage = in.ReadU8();
        int32_t type = in.ReadU8A();
        player.Hit(type, damage);
        player.combatCycle = loopCycle + 300;
        player.health = in.ReadU8C();
        player.totalHealth = in.ReadU8();
    }

    void Game::ReadPlayerDamage1(PlayerEntity& player)
    {
        int32_t damage = in.ReadU8();
        int32_t type = in.ReadU8A();
        player.Hit(type, damage);
        player.combatCycle = loopCycle + 300;
        player.health = in.ReadU8();
        player.totalHealth = in.ReadU8C();
    }

    void Game::ReadChatFilterSettings()
    {
        publicChatSetting = in.ReadU8();
        privateChatSetting = in.ReadU8();
        tradeChatSetting = in.ReadU8();
        redrawPrivacySettings = true;
        redrawChatback = true;
    }


    void Game::ReadUpdateRunWeight()
    {
        if (selectedTab == 12) {
            redrawSidebar = true;
        }
        weightCarried = in.Read16();
    }

    void Game::ReadIfSetModel()
    {
        int32_t interfaceID = in.ReadU16LEA();
        int32_t modelID = in.ReadU16();
        IfType::instances[interfaceID]->modelType = IfType::MODEL_TYPE_NORMAL;
        IfType::instances[interfaceID]->modelID = modelID;
    }

    void Game::ReadIfSetColor()
    {
        int32_t interfaceID = in.ReadU16LEA();
        int32_t rgb555 = in.ReadU16LEA();
        int32_t r = (rgb555 >> 10) & 0x1f;
        int32_t g = (rgb555 >> 5) & 0x1f;
        int32_t b = rgb555 & 0x1f;
        IfType::instances[interfaceID]->color = (r << 19) + (g << 11) + (b << 3);
    }

    void Game::ReadUpdateInvFull()
    {
        redrawSidebar = true;
        int32_t interfaceID = in.ReadU16();
        const auto& iface = IfType::instances[interfaceID];
        int32_t lastSlot = in.ReadU16();

        for (int32_t slot = 0; slot < lastSlot; slot++) {
            int32_t objCount = in.ReadU8();

            if (objCount == 255) {
                objCount = in.Read32ME();
            }

            if (slot >= iface->inventorySlotObjID.size()) {
                in.ReadU16LEA();
            } else {
                iface->inventorySlotObjID[slot] = in.ReadU16LEA();
                iface->inventorySlotObjCount[slot] = objCount;
            }
        }

        // clear remaining slots
        for (int32_t slot = lastSlot; slot < iface->inventorySlotObjID.size(); slot++) {
            iface->inventorySlotObjID[slot] = 0;
            iface->inventorySlotObjCount[slot] = 0;
        }
    }

    void Game::ReadIfSetAngle()
    {
        int32_t zoom = in.ReadU16A();
        int32_t interfaceID = in.ReadU16();
        int32_t pitch = in.ReadU16();
        int32_t yaw = in.ReadU16LEA();
        IfType::instances[interfaceID]->modelPitch = pitch;
        IfType::instances[interfaceID]->modelYaw = yaw;
        IfType::instances[interfaceID]->modelZoom = zoom;
    }

    void Game::ReadMessageGame()
    {
        const std::string s = in.ReadString();

        if (s.ends_with(":tradereq:")) {
            const std::string name = s.substr(0, s.find(':'));
            const int64_t name37 = StringUtil::ToBase37(name);

            bool ignore = false;
            for (int32_t i = 0; i < ignoreCount; i++) {
                if (ignoreName37[i] == name37) {
                    ignore = true;
                    break;
                }
            }

            if (!ignore && (overrideChat == 0)) {
                AddMessage(4, name, "wishes to trade with you.");
            }
        } else if (s.ends_with(":duelreq:")) {
            const std::string name = s.substr(0, s.find(':'));
            const int64_t name37 = StringUtil::ToBase37(name);

            bool ignore = false;
            for (int32_t i = 0; i < ignoreCount; i++) {
                if (ignoreName37[i] == name37) {
                    ignore = true;
                    break;
                }
            }

            if (!ignore && (overrideChat == 0)) {
                AddMessage(8, name, "wishes to duel with you.");
            }
        } else if (s.ends_with(":chalreq:")) {
            const std::string name = s.substr(0, s.find(':'));
            const int64_t name37 = StringUtil::ToBase37(name);

            bool ignore = false;
            for (int32_t i = 0; i < ignoreCount; i++) {
                if (ignoreName37[i] == name37) {
                    ignore = true;
                    break;
                }
            }

            if (!ignore && (overrideChat == 0)) {
                const size_t colonPos = s.find(':');
                const std::string message = s.substr(colonPos + 1, s.length() - colonPos - 10);
                AddMessage(8, name, message);
            }
        } else {
            AddMessage(0, "", s);
        }
    }

    void Game::ReadSetPlayerOp()
    {
        int32_t option = in.ReadU8C();
        int32_t priority = in.ReadU8A();
        auto text = in.ReadString();
        if ((option >= 1) && (option <= 5)) {
            if (StringUtil::EqualsIgnoreCase(text, "null")) {
                text.clear();
            }
            playerOptions[option - 1] = text;
            playerOptionPushDown[option - 1] = priority == 0;
        }
    }

    void Game::ReadZoneClear()
    {
        baseX = in.ReadU8C();
        baseZ = in.ReadU8S();
        for (int32_t x = baseX; x < (baseX + 8); x++) {
            for (int32_t z = baseZ; z < (baseZ + 8); z++) {
                if (!levelObjStacks[currentLevel][x][z].isEmpty()) {
                    levelObjStacks[currentLevel][x][z].clear();
                    SortObjStacks(x, z);
                }
            }
        }
        for (auto loc = static_cast<SceneLocTemporary*>(temporaryLocs.peekFront()); loc != nullptr; loc = static_cast<SceneLocTemporary*>(temporaryLocs.prev())) {
            if ((loc->localX >= baseX) && (loc->localX < (baseX + 8)) && (loc->localZ >= baseZ) && (loc->localZ < (baseZ + 8)) && (loc->level == currentLevel)) {
                loc->duration = 0;
            }
        }
    }

    void Game::ReadIgnoreList()
    {
        ignoreCount = packetSize / 8;
        for (int32_t j1 = 0; j1 < ignoreCount; j1++) {
            ignoreName37[j1] = in.Read64();
        }
    }

    void Game::ReadFriendStatus()
    {
        int64_t name37 = in.Read64();
        int32_t world = in.ReadU8();
        auto name = StringUtil::FormatName(StringUtil::FromBase37(name37));

        for (int32_t i = 0; i < friendCount; i++) {
            if (name37 != friendName37[i]) {
                continue;
            }
            if (friendWorld[i] != world) {
                friendWorld[i] = world;
                redrawSidebar = true;
                if (world > 0) {
                    AddMessage(5, "", name + " has logged in.");
                }
                if (world == 0) {
                    AddMessage(5, "", name + " has logged out.");
                }
            }
            name.clear();
            break;
        }

        if ((!name.empty()) && (friendCount < 200)) {
            friendName37[friendCount] = name37;
            friendName[friendCount] = name;
            friendWorld[friendCount] = world;
            friendCount++;
            redrawSidebar = true;
        }

        for (bool sorted = false; !sorted; ) {
            sorted = true;
            for (int32_t i = 0; i < (friendCount - 1); i++) {
                if (((friendWorld[i] != nodeID) && (friendWorld[i + 1] == nodeID)) || ((friendWorld[i] == 0) && (friendWorld[i + 1] != 0))) {
                    int32_t tmp0 = friendWorld[i];
                    friendWorld[i] = friendWorld[i + 1];
                    friendWorld[i + 1] = tmp0;

                    auto tmp1 = friendName[i];
                    friendName[i] = friendName[i + 1];
                    friendName[i + 1] = tmp1;

                    int64_t tmp2 = friendName37[i];
                    friendName37[i] = friendName37[i + 1];
                    friendName37[i + 1] = tmp2;
                    redrawSidebar = true;
                    sorted = false;
                }
            }
        }
    }

    void Game::ReadUpdateRunEnergy()
    {
        if (selectedTab == 12) {
            redrawSidebar = true;
        }
        energy = in.ReadU8();
    }

    void Game::ReadIfTab()
    {
        int32_t interfaceID = in.ReadU16();
        int32_t tab = in.ReadU8A();
        if (interfaceID == 65535) {
            interfaceID = -1;
        }
        tabInterfaceID[tab] = interfaceID;
        redrawSidebar = true;
        redrawSideicons = true;
    }

    void Game::ReadIfSetPosition()
    {
        int32_t x = in.Read16();
        int32_t y = in.Read16LE();
        int32_t interfaceID = in.ReadU16LE();
        const auto& iface = IfType::instances[interfaceID];
        iface->x = x;
        iface->y = y;
    }

    void Game::ReadIfViewportOverlay()
    {
        int32_t interfaceID = in.Read16LE();
        if (interfaceID >= 0) {
            ResetInterfaceAnimation(interfaceID);
        }
        viewportOverlayInterfaceID = interfaceID;
    }

    void Game::ReadIfSetNPCHead()
    {
        int32_t npcID = in.ReadU16LEA();
        int32_t interfaceID = in.ReadU16LEA();
        IfType::instances[interfaceID]->modelType = IfType::MODEL_TYPE_NPC;
        IfType::instances[interfaceID]->modelID = npcID;
    }

    void Game::ReadMessagePublic()
    {
        int64_t name37 = in.Read64();
        int32_t messageID = in.Read32();
        int32_t role = in.ReadU8();
        bool ignore = false;
        for (int32_t i = 0; i < 100; i++) {
            if (messageIDs[i] == messageID) {
                ignore = true;
                break;
            }
        }

        if (role <= 1) {
            for (int32_t i = 0; i < ignoreCount; i++) {
                if (ignoreName37[i] == name37) {
                    ignore = true;
                    break;
                }
            }
        }

        if (!ignore && (overrideChat == 0)) {
            messageIDs[messageCounter] = messageID;
            messageCounter = (messageCounter + 1) % 100;
            auto message = ChatCompression::Unpack(packetSize - 13, in);

            if (role != 3) {
                //message = Censor.filter(message);
            }

            if ((role == 2) || (role == 3)) {
                AddMessage(7, "@cr2@" + StringUtil::FormatName(name37), message);
            } else if (role == 1) {
                AddMessage(7, "@cr1@" + StringUtil::FormatName(name37), message);
            } else {
                AddMessage(3, StringUtil::FormatName(name37), message);
            }
        }
    }

    void Game::ReadTabHint()
    {
        flashingTab = in.ReadU8S();
        if (flashingTab == selectedTab) {
            if (flashingTab == 3) {
                selectedTab = 1;
            } else {
                selectedTab = 3;
            }
            redrawSidebar = true;
        }
    }

    void Game::ReadIfSetObject()
    {
        int32_t interfaceID = in.ReadU16LE();
        int32_t zoom = in.ReadU16();
        int32_t objID = in.ReadU16();

        if (objID != 65535) {
            const auto& type = ObjType::Get(objID);
            const auto& iface = IfType::instances[interfaceID];
            iface->modelType = IfType::MODEL_TYPE_OBJ;
            iface->modelID = objID;
            iface->modelPitch = type->iconPitch;
            iface->modelYaw = type->iconYaw;
            iface->modelZoom = (type->iconZoom * 100) / zoom;
        } else {
            IfType::instances[interfaceID]->modelType = IfType::MODEL_TYPE_NONE;
        }
    }

    void Game::ReadIfSetHide()
    {
        bool hide = in.ReadU8() == 1;
        int32_t interfaceID = in.ReadU16();
        IfType::instances[interfaceID]->hide = hide;
    }

    void Game::ReadIfStopAnim()
    {
        int32_t interfaceID = in.ReadU16LE();
        ResetInterfaceAnimation(interfaceID);
        if (chatInterfaceID != -1) {
            chatInterfaceID = -1;
            redrawChatback = true;
        }
        if (chatbackInputType != 0) {
            chatbackInputType = 0;
            redrawChatback = true;
        }
        sidebarInterfaceID = interfaceID;
        redrawSidebar = true;
        redrawSideicons = true;
        viewportInterfaceID = -1;
        pressedContinueOption = false;
    }

    void Game::ReadIfSetText()
    {
        const auto& text = in.ReadString();
        int32_t interfaceID = in.ReadU16A();
        if ((interfaceID >= 0) && (interfaceID < IfType::instances.size())) {
            const auto& iface = IfType::instances[interfaceID];
            if (iface != nullptr) {
                iface->text = text;
                if (iface->parentID == tabInterfaceID[selectedTab]) {
                    redrawSidebar = true;
                }
            }
        }
    }

    void Game::ReadUpdateInvPartial()
    {
        redrawSidebar = true;
        int32_t interfaceID = in.ReadU16();
        const auto& iface = IfType::instances[interfaceID];

        while (in.position < packetSize) {
            int32_t slot = in.ReadUSmart();
            int32_t objID = in.ReadU16();
            int32_t objCount = in.ReadU8();
            if (objCount == 255) {
                objCount = in.Read32();
            }
            if ((slot >= 0) && (slot < iface->inventorySlotObjID.size())) {
                iface->inventorySlotObjID[slot] = objID;
                iface->inventorySlotObjCount[slot] = objCount;
            }
        }
    }

    void Game::ReadTabSelected()
    {
        selectedTab = in.ReadU8C();
        redrawSidebar = true;
        redrawSideicons = true;
    }

    void Game::ReadZonePacket(int32_t code)
    {
        switch (code) {
        case PacketIn::OBJ_ADD:
            ReadObjAdd();
            break;
        case PacketIn::OBJ_REVEAL:
            ReadObjReveal();
            break;
        case PacketIn::OBJ_COUNT:
            ReadObjCount();
            break;
        case PacketIn::OBJ_DEL:
            ReadObjDelete();
            break;
        case PacketIn::LOC_ADD:
            ReadLocAdd();
            break;
        case PacketIn::LOC_CHANGE:
            ReadLocChange();
            break;
        case PacketIn::LOC_DEL:
            ReadLocDelete();
            break;
        case PacketIn::LOC_PLAYER:
            ReadLocPlayer();
            break;
        case PacketIn::MAP_ANIM:
            ReadMapAnim();
            break;
        case PacketIn::MAP_SOUND:
            ReadMapSound();
            break;
        case PacketIn::MAP_PROJECTILE:
            ReadMapProjectile();
            break;
        }
    }


    void Game::ReadObjAdd()
    {
        int32_t objID = in.ReadU16LEA();
        int32_t objCount = in.ReadU16();
        int32_t pos = in. ReadU8();
        int32_t x = baseX + ((pos >> 4) & 7);
        int32_t z = baseZ + (pos & 7);
        if ((x >= 0) && (z >= 0) && (x < 104) && (z < 104)) {
            auto stack = new ObjEntity();
            stack->id = objID;
            stack->count = objCount;
            // No need to check isEmpty() - the DoublyLinkedList is already initialized
            // via Array3DIndexed default construction with proper head sentinel
            levelObjStacks[currentLevel][x][z]. pushBack(stack);
            SortObjStacks(x, z);
        }
    }

    void Game::ReadObjReveal()
    {
        int32_t objID = in.ReadU16A();
        int32_t pos = in.ReadU8S();
        int32_t x = baseX + ((pos >> 4) & 7);
        int32_t z = baseZ + (pos & 7);
        int32_t ownerPID = in.ReadU16A();
        int32_t objCount = in.ReadU16();
        if ((x >= 0) && (z >= 0) && (x < 104) && (z < 104) && (ownerPID != localPID)) {
            auto obj = new ObjEntity();
            obj->id = objID;
            obj->count = objCount;
            if (!levelObjStacks[currentLevel][x][z].isEmpty()) {
                levelObjStacks[currentLevel][x][z] = DoublyLinkedList();
            }
            levelObjStacks[currentLevel][x][z].pushBack(obj);
            SortObjStacks(x, z);
        }
    }

    void Game::ReadObjCount()
    {
        int32_t pos = in.ReadU8();
        int32_t x = baseX + ((pos >> 4) & 7);
        int32_t z = baseZ + (pos & 7);
        int32_t objID = in.ReadU16();
        int32_t oldCount = in.ReadU16();
        int32_t newCount = in.ReadU16();

        if ((x >= 0) && (z >= 0) && (x < 104) && (z < 104)) {
            auto& stacks = levelObjStacks[currentLevel][x][z];
            if (!stacks.isEmpty()) {
                for (auto* node = stacks.peekFront(); node != nullptr; node = stacks.prev()) {
                    auto* stack = static_cast<ObjEntity*>(node);
                    if ((stack->id != (objID & 0x7fff)) || (stack->count != oldCount)) {
                        continue;
                    }
                    stack->count = newCount;
                    break;
                }
                SortObjStacks(x, z);
            }
        }
    }

    void Game::ReadObjDelete()
    {
        int32_t pos = in.ReadU8A();
        int32_t x = baseX + ((pos >> 4) & 7);
        int32_t z = baseZ + (pos & 7);
        int32_t objID = in.ReadU16();
        if ((x >= 0) && (z >= 0) && (x < 104) && (z < 104)) {
            auto& list = levelObjStacks[currentLevel][x][z];
            if (!list.isEmpty()) {
                for (auto* node = list.peekFront(); node != nullptr; node = list.prev()) {
                    auto* obj = static_cast<ObjEntity*>(node);
                    if (obj->id != (objID & 0x7fff)) {
                        continue;
                    }
                    obj->unlink();
                    break;
                }
                if (list.peekFront() == nullptr) {
                    levelObjStacks[currentLevel][x][z].clear();
                }
                SortObjStacks(x, z);
            }
        }
    }

    void Game::ReadLocAdd()
    {
        int32_t pos = in.ReadU8A();
        int32_t x = baseX + ((pos >> 4) & 7);
        int32_t z = baseZ + (pos & 7);
        int32_t id = in.ReadU16LE();
        int32_t info = in.ReadU8S();
        int32_t kind = info >> 2;
        int32_t rotation = info & 3;
        int32_t classID = LOC_KIND_TO_CLASS_ID[kind];
        if ((x >= 0) && (z >= 0) && (x < 104) && (z < 104)) {
            AppendLoc(-1, id, rotation, classID, z, kind, currentLevel, x, 0);
        }
    }

    void Game::ReadLocChange()
    {
        int32_t pos = in.ReadU8S();
        int32_t x = baseX + ((pos >> 4) & 7);
        int32_t z = baseZ + (pos & 7);
        int32_t info = in.ReadU8S();
        int32_t kind = info >> 2;
        int32_t rotation = info & 3;
        int32_t classID = LOC_KIND_TO_CLASS_ID[kind];
        int32_t seqID = in.ReadU16A();

        if ((x < 0) || (z < 0) || (x >= 103) || (z >= 103)) {
            return;
        }

        int32_t heightmapSW = levelHeightmap[currentLevel][x][z];
        int32_t heightmapSE = levelHeightmap[currentLevel][x + 1][z];
        int32_t heightmapNE = levelHeightmap[currentLevel][x + 1][z + 1];
        int32_t heightmapNW = levelHeightmap[currentLevel][x][z + 1];

        if (classID == 0) {
            const auto& wall = scene->GetWall(currentLevel, x, z);

            if (wall != nullptr) {
                int32_t locID = (wall->bitset >> 14) & 0x7fff;

                if (kind == 2) {
                    wall->entityA = std::make_shared<LocEntity>(locID, 4 + rotation, 2, heightmapSE, heightmapNE, heightmapSW, heightmapNW, seqID, false);
                    wall->entityB = std::make_shared<LocEntity>(locID, (rotation + 1) & 3, 2, heightmapSE, heightmapNE, heightmapSW, heightmapNW, seqID, false);
                } else {
                    wall->entityA = std::make_shared<LocEntity>(locID, rotation, kind, heightmapSE, heightmapNE, heightmapSW, heightmapNW, seqID, false);
                }
            }
        }

        if (classID == 1) {
            const auto& deco = scene->GetWallDecoration(currentLevel, x, z);

            if (deco != nullptr) {
                deco->entity = std::make_shared<LocEntity>((deco->bitset >> 14) & 0x7fff, 0, 4, heightmapSE, heightmapNE, heightmapSW, heightmapNW, seqID, false);
            }
        }

        if (classID == 2) {
            const auto& loc = scene->GetLoc(currentLevel, x, z);

            if (kind == 11) {
                kind = 10;
            }

            if (loc != nullptr) {
                loc->entity =std::make_shared<LocEntity>((loc->bitset >> 14) & 0x7fff, rotation, kind, heightmapSE, heightmapNE, heightmapSW, heightmapNW, seqID, false);
            }
        }

        if (classID == 3) {
            const auto& deco = scene->GetGroundDecoration(z, x, currentLevel);

            if (deco != nullptr) {
                deco->entity = std::make_shared<LocEntity>((deco->bitset >> 14) & 0x7fff, rotation, 22,
                    heightmapSE, heightmapNE, heightmapSW, heightmapNW, seqID, false);
            }
        }
    }

    void Game::ReadLocDelete()
    {
        int32_t info = in.ReadU8C();
        int32_t kind = info >> 2;
        int32_t rotation = info & 3;
        int32_t classID = LOC_KIND_TO_CLASS_ID[kind];
        int32_t pos = in.ReadU8();
        int32_t x = baseX + ((pos >> 4) & 7);
        int32_t z = baseZ + (pos & 7);
        if ((x >= 0) && (z >= 0) && (x < 104) && (z < 104)) {
            AppendLoc(-1, -1, rotation, classID, z, kind, currentLevel, x, 0);
        }
    }

    void Game::ReadLocPlayer()
    {
        int32_t pos = in.ReadU8S();
        int32_t x = baseX + ((pos >> 4) & 7);
        int32_t z = baseZ + (pos & 7);
        int32_t pid = in.ReadU16();
        int8_t maxX = in.Read8S();
        int32_t delay = in.ReadU16LE();
        int8_t maxZ = in.Read8C();
        int32_t duration = in.ReadU16();
        int32_t info = in.ReadU8S();
        int32_t kind = info >> 2;
        int32_t rotation = info & 3;
        int32_t classID = LOC_KIND_TO_CLASS_ID[kind];
        int8_t minX = in.Read8();
        int32_t locID = in.ReadU16();
        int8_t minZ = in.Read8C();
        std::shared_ptr<PlayerEntity> player;

        if (pid == localPID) {
            player = localPlayer;
        } else {
            player = players[pid];
        }

        if (player != nullptr) {
            const auto& type = LocType::Get(locID);
            int32_t heightmapSW = levelHeightmap[currentLevel][x][z];
            int32_t heightmapSE = levelHeightmap[currentLevel][x + 1][z];
            int32_t heightmapNE = levelHeightmap[currentLevel][x + 1][z + 1];
            int32_t heightmapNW = levelHeightmap[currentLevel][x][z + 1];

            auto model = type->GetModel(kind, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, -1);

            if (model != nullptr) {
                AppendLoc(duration + 1, -1, 0, classID, z, 0, currentLevel, x, delay + 1);

                player->locStartCycle = delay + loopCycle;
                player->locStopCycle = duration + loopCycle;
                player->locModel = model;
                int32_t sizeX = type->sizeX;
                int32_t sizeZ = type->sizeZ;

                if ((rotation == 1) || (rotation == 3)) {
                    sizeX = type->sizeZ;
                    sizeZ = type->sizeX;
                }

                player->locOffsetX = (x * 128) + (sizeX * 64);
                player->locOffsetZ = (z * 128) + (sizeZ * 64);
                player->locOffsetY = GetHeightmapY(currentLevel, player->locOffsetX, player->locOffsetZ);

                if (minX > maxX) {
                    int8_t tmp = minX;
                    minX = maxX;
                    maxX = tmp;
                }

                if (minZ > maxZ) {
                    int8_t tmp = minZ;
                    minZ = maxZ;
                    maxZ = tmp;
                }

                player->minSceneTileX = x + minX;
                player->maxSceneTileX = x + maxX;
                player->minSceneTileZ = z + minZ;
                player->maxSceneTileZ = z + maxZ;
            }
        }
    }

    void Game::ReadZoneUpdate()
    {
        baseZ = in.ReadU8();
        baseX = in.ReadU8C();
        while (in.position < packetSize) {
            ReadZonePacket(in.ReadU8());
        }
    }

    void Game::ReadMapAnim()
    {
        int32_t pos = in.ReadU8();
        int32_t x = baseX + ((pos >> 4) & 7);
        int32_t z = baseZ + (pos & 7);
        int32_t id = in.ReadU16();
        int32_t y = in.ReadU8();
        int32_t delay = in.ReadU16();

        if ((x >= 0) && (z >= 0) && (x < 104) && (z < 104)) {
            x = (x * 128) + 64;
            z = (z * 128) + 64;
            auto anim = new SpotAnimEntity(currentLevel, loopCycle, delay, id, GetHeightmapY(currentLevel, x, z) - y, z, x);
            spotanims.pushBack(anim);
        }
    }

    void Game::ReadMapProjectile()
    {
        int32_t pos = in.ReadU8();
        int32_t srcX = baseX + ((pos >> 4) & 7);
        int32_t srcZ = baseZ + (pos & 7);
        int32_t dstX = srcX + in.Read8();
        int32_t dstZ = srcZ + in.Read8();
        int32_t targetID = in.Read16();
        int32_t spotanimID = in.ReadU16();
        int32_t srcY = in.ReadU8() * 4;
        int32_t dstY = in.ReadU8() * 4;
        int32_t delay = in.ReadU16();
        int32_t duration = in.ReadU16();
        int32_t peakPitch = in.ReadU8();
        int32_t arcSize = in.ReadU8();
        if ((srcX >= 0) && (srcZ >= 0) && (srcX < 104) && (srcZ < 104) && (dstX >= 0) && (dstZ >= 0) && (dstX < 104) && (dstZ < 104) && (spotanimID != 65535)) {
            srcX = (srcX * 128) + 64;
            srcZ = (srcZ * 128) + 64;
            dstX = (dstX * 128) + 64;
            dstZ = (dstZ * 128) + 64;
            auto projectile = new ProjectileEntity(peakPitch, dstY, delay + loopCycle, duration + loopCycle, arcSize, currentLevel, GetHeightmapY(currentLevel, srcX, srcZ) - srcY, srcZ, srcX, targetID, spotanimID);
            projectile->UpdateVelocity(delay + loopCycle, dstZ, GetHeightmapY(currentLevel, dstX, dstZ) - dstY, dstX);
            projectiles.pushBack(projectile);
        }
    }

    void Game::ReadCameraReset()
    {
        cutscene = false;
        for (int32_t l = 0; l < 5; l++) {
            cameraModifierEnabled[l] = false;
        }
    }

    void Game::ReadInventoryClear()
    {
        int32_t interfaceID = in.ReadU16LE();
        const auto& iface = IfType::instances[interfaceID];
        for (int32_t slot = 0; slot < iface->inventorySlotObjID.size(); slot++) {
            iface->inventorySlotObjID[slot] = -1;
            iface->inventorySlotObjID[slot] = 0;
        }
    }

    void Game::ReadUpdateStat()
    {
        redrawSidebar = true;
        int32_t skill = in.ReadU8();
        int32_t experience = in.Read32RME();
        int32_t level = in.ReadU8();
        skillExperience[skill] = experience;
        skillLevel[skill] = level;
        skillBaseLevel[skill] = 1;
        for (int32_t i = 0; i < 98; i++) {
            if (experience >= levelExperience[i]) {
                skillBaseLevel[skill] = i + 2;
            }
        }
    }

    void Game::ReadCameraSetPos()
    {
        cutscene = true;
        cutsceneSrcLocalTileX = in.ReadU8();
        cutsceneSrcLocalTileZ = in.ReadU8();
        cutsceneSrcHeight = in.ReadU16();
        cutsceneMoveSpeed = in.ReadU8();
        cutsceneMoveAcceleration = in.ReadU8();

        if (cutsceneMoveAcceleration >= 100) {
            cameraX = (cutsceneSrcLocalTileX * 128) + 64;
            cameraZ = (cutsceneSrcLocalTileZ * 128) + 64;
            cameraY = GetHeightmapY(currentLevel, cameraX, cameraZ) - cutsceneSrcHeight;
        }
    }

    void Game::ReadCameraLookAt()
    {
        cutscene = true;
        cutsceneDstLocalTileX = in.ReadU8();
        cutsceneDstLocalTileZ = in.ReadU8();
        cutsceneDstHeight = in.ReadU16();
        cutsceneRotateSpeed = in.ReadU8();
        cutsceneRotateAcceleration = in.ReadU8();

        if (cutsceneRotateAcceleration >= 100) {
            int32_t sceneX = (cutsceneDstLocalTileX * 128) + 64;
            int32_t sceneZ = (cutsceneDstLocalTileZ * 128) + 64;
            int32_t sceneY = GetHeightmapY(currentLevel, sceneX, sceneZ) - cutsceneDstHeight;
            int32_t deltaX = sceneX - cameraX;
            int32_t deltaY = sceneY - cameraY;
            int32_t deltaZ = sceneZ - cameraZ;
            auto distance = static_cast<int32_t>(SDL_sqrt((deltaX * deltaX) + (deltaZ * deltaZ)));
            cameraPitch = static_cast<int32_t>(SDL_atan2(deltaY, distance) * 325.95) & 0x7ff;
            cameraYaw = static_cast<int32_t>(SDL_atan2(deltaX, deltaZ) * -325.95) & 0x7ff;
            if (cameraPitch < 128) {
                cameraPitch = 128;
            }
            if (cameraPitch > 383) {
                cameraPitch = 383;
            }
        }
    }

    void Game::ReadCameraShake()
    {
        int32_t type = in.ReadU8();
        int32_t jitterScale = in.ReadU8();
        int32_t wobbleScale = in.ReadU8();
        int32_t wobbleSpeed = in.ReadU8();
        cameraModifierEnabled[type] = true;
        cameraModifierJitter[type] = jitterScale;
        cameraModifierWobbleScale[type] = wobbleScale;
        cameraModifierWobbleSpeed[type] = wobbleSpeed;
        cameraModifierCycle[type] = 0;
    }

    void Game::OpenChatInput(int32_t type)
    {
        showSocialInput = false;
        chatbackInputType = type;
        chatbackInput = "";
        redrawChatback = true;
    }

    void Game::ReadSyncNPCs()
    {
        entityRemovalCount = 0;
        entityUpdateCount = 0;

        ReadNPCs();
        ReadNewNPCs();
        ReadNPCUpdates();

        for (int32_t k = 0; k < entityRemovalCount; k++) {
            int32_t id = entityRemovalIDs[k];

            if (npcs[id]->cycle != loopCycle) {
                npcs[id]->type = nullptr;
                npcs[id] = nullptr;
            }
        }

        if (in.position != this->packetSize) {
            LOG_ERROR("%s size mismatch in getnpcpos - pos:%i psize:%i", username.c_str(), in.position, this->packetSize);
        }

        for (int32_t i1 = 0; i1 < npcCount; i1++) {
            if (npcs[npcIDs[i1]] == nullptr) {
                LOG_ERROR("%s null entry in npc list - pos:%i size:%i", username.c_str(), i1, npcCount);
            }
        }
    }

    void Game::ReadNPCs()
    {
        in.AccessBits();
        int32_t count = in.ReadN(8);

        if (count < npcCount) {
            for (int32_t l = count; l < npcCount; l++) {
                entityRemovalIDs[entityRemovalCount++] = npcIDs[l];
            }
        }

        if (count > npcCount) {
            LOG_ERROR("%s Too many npcs", username.c_str());
        }

        npcCount = 0;
        for (int32_t i = 0; i < count; i++) {
            int32_t id = npcIDs[i];
            const auto& npc = npcs[id];

            if (in.ReadN(1) == 0) {
                npcIDs[npcCount++] = id;
                npc->cycle = loopCycle;
            } else {
                int32_t type = in.ReadN(2);

                if (type == 0) {
                    npcIDs[npcCount++] = id;
                    npc->cycle = loopCycle;
                    entityUpdateIDs[entityUpdateCount++] = id;
                } else if (type == 1) {
                    npcIDs[npcCount++] = id;
                    npc->cycle = loopCycle;

                    npc->Step(false, in.ReadN(3));

                    if (in.ReadN(1) == 1) {
                        entityUpdateIDs[entityUpdateCount++] = id;
                    }
                } else if (type == 2) {
                    npcIDs[npcCount++] = id;
                    npc->cycle = loopCycle;

                    npc->Step(true, in.ReadN(3));
                    npc->Step(true, in.ReadN(3));

                    if (in.ReadN(1) == 1) {
                        entityUpdateIDs[entityUpdateCount++] = id;
                    }
                } else if (type == 3) {
                    entityRemovalIDs[entityRemovalCount++] = id;
                }
            }
        }
    }

    void Game::ReadNewNPCs()
    {
        while ((in.bitPosition + 21) < (this->packetSize * 8)) {
            int32_t id = in.ReadN(14);

            if (id == 16383) {
                break;
            }

            if (npcs[id] == nullptr) {
                npcs[id] = std::make_shared<NPCEntity>();
            }

            const auto& npc = npcs[id];
            npcIDs[npcCount++] = id;
            npc->cycle = loopCycle;

            int32_t z = in.ReadN(5);
            int32_t x = in.ReadN(5);

            if (z > 15) {
                z -= 32;
            }

            if (x > 15) {
                x -= 32;
            }

            int32_t teleport = in.ReadN(1);
            npc->type = NPCType::Get(in.ReadN(12));

            if (in.ReadN(1) == 1) {
                entityUpdateIDs[entityUpdateCount++] = id;
            }

            npc->size = npc->type->size;
            npc->turnSpeed = npc->type->turnSpeed;
            npc->seqWalkID = npc->type->seqWalkID;
            npc->seqTurnAroundID = npc->type->seqTurnAroundID;
            npc->seqTurnLeftID = npc->type->seqTurnLeftID;
            npc->seqTurnRightID = npc->type->seqTurnRightID;
            npc->seqStandID = npc->type->seqStandID;
            npc->Move(localPlayer->pathTileX[0] + x, localPlayer->pathTileZ[0] + z, teleport == 1);
        }
    }

    void Game::ReadNPCUpdates()
    {
        in.AccessBytes();

        for (int32_t i = 0; i < entityUpdateCount; i++) {
            const auto& npc = npcs[entityUpdateIDs[i]];
            int32_t updates = in.ReadU8();

            if ((updates & 0x10) != 0) {
                ReadNPCAnimation(*npc);
            }

            if ((updates & 0x8) != 0) {
                ReadNPCDamage0(*npc);
            }

            if ((updates & 0x80) != 0) {
                ReadNPCGraphic(*npc);
            }

            if ((updates & 0x20) != 0) {
                ReadNPCTargetEntity(*npc);
            }

            if ((updates & 0x1) != 0) {
                ReadNPCChat(*npc);
            }

            if ((updates & 0x40) != 0) {
                ReadNPCDamage1(*npc);
            }

            if ((updates & 0x2) != 0) {
                ReadNPCTransform(*npc);
            }

            if ((updates & 0x4) != 0) {
                ReadNPCTargetTile(*npc);
            }
        }
    }

    void Game::ReadNPCAnimation(NPCEntity& npc)
    {
        int32_t seqID = in.ReadU16LE();

        if (seqID == 65535) {
            seqID = -1;
        }

        int32_t delay = in.ReadU8();
        if ((seqID == npc.primarySeqID) && (seqID != -1)) {
            int32_t style = SeqType::instances[seqID].replayStyle;

            if (style == 1) {
                npc.primarySeqFrame = 0;
                npc.primarySeqCycle = 0;
                npc.primarySeqDelay = delay;
                npc.primarySeqLoop = 0;
            }

            if (style == 2) {
                npc.primarySeqLoop = 0;
            }
        } else if ((seqID == -1) || (npc.primarySeqID == -1) || (SeqType::instances[seqID].priority >= SeqType::instances[npc.primarySeqID].priority)) {
            npc.primarySeqID = seqID;
            npc.primarySeqFrame = 0;
            npc.primarySeqCycle = 0;
            npc.primarySeqDelay = delay;
            npc.primarySeqLoop = 0;
            npc.seqPathLength = npc.pathLength;
        }
    }

    void Game::ReadNPCDamage0(NPCEntity& npc)
    {
        int32_t damage = in.ReadU8A();
        int32_t type = in.ReadU8C();
        npc.Hit(type, damage);
        npc.combatCycle = loopCycle + 300;
        npc.health = in.ReadU8A();
        npc.totalHealth = in.ReadU8();
    }

    void Game::ReadNPCGraphic(NPCEntity& npc)
    {
        npc.spotanimID = in.ReadU16();
        npc.spotanimOffset = in.ReadU16();
        npc.spotanimLastCycle = loopCycle + in.ReadU16();
        npc.spotanimFrame = 0;
        npc.spotanimCycle = 0;
        if (npc.spotanimLastCycle > loopCycle) {
            npc.spotanimFrame = -1;
        }
        if (npc.spotanimID == 65535) {
            npc.spotanimID = -1;
        }
    }

    void Game::ReadNPCTargetEntity(NPCEntity& npc)
    {
        npc.targetID = in.ReadU16();
        if (npc.targetID == 65535) {
            npc.targetID = -1;
        }
    }

    void Game::ReadNPCChat(NPCEntity& npc)
    {
        npc.chat = in.ReadString();
        npc.chatTimer = 100;
    }

    void Game::ReadNPCDamage1(NPCEntity& npc)
    {
        int32_t damage = in.ReadU8C();
        int32_t type = in.ReadU8S();
        npc.Hit(type, damage);
        npc.combatCycle = loopCycle + 300;
        npc.health = in.ReadU8S();
        npc.totalHealth = in.ReadU8C();
    }

    void Game::ReadNPCTransform(NPCEntity& npc)
    {
        npc.type = NPCType::Get(in.ReadU16LEA());
        npc.size = npc.type->size;
        npc.turnSpeed = npc.type->turnSpeed;
        npc.seqWalkID = npc.type->seqWalkID;
        npc.seqTurnAroundID = npc.type->seqTurnAroundID;
        npc.seqTurnLeftID = npc.type->seqTurnLeftID;
        npc.seqTurnRightID = npc.type->seqTurnRightID;
        npc.seqStandID = npc.type->seqStandID;
    }

    void Game::ReadNPCTargetTile(NPCEntity& npc)
    {
        npc.targetTileX = in.ReadU16LE();
        npc.targetTileZ = in.ReadU16LE();
    }

    void Game::ReadViewportInterface()
    {
        int32_t interfaceID = in.ReadU16();
        ResetInterfaceAnimation(interfaceID);
        OpenViewportInterface(interfaceID);
    }

    void Game::ReadVarpLarge()
    {
        int32_t varpID = in.ReadU16LE();
        int32_t value = in.Read32RME();
        varCache[varpID] = value;

        if (varps[varpID] != value) {
            varps[varpID] = value;
            UpdateVarp(varpID);
            redrawSidebar = true;
            if (stickyChatInterfaceID != -1) {
                redrawChatback = true;
            }
        }
    }

    void Game::ReadVarpSmall()
    {
        int32_t varpID = in.ReadU16LE();
        int8_t value = in.Read8();
        varCache[varpID] = value;

        if (varps[varpID] != value) {
            varps[varpID] = value;
            UpdateVarp(varpID);
            redrawSidebar = true;
            if (stickyChatInterfaceID != -1) {
                redrawChatback = true;
            }
        }
    }

    void Game::ReadIfSetScrollPos()
    {
        int32_t interfaceID = in.ReadU16LE();
        int32_t scrollPos = in.ReadU16A();
        const auto& iface = IfType::instances[interfaceID];
        if ((iface != nullptr) && (iface->type == IfType::TYPE_PARENT)) {
            if (scrollPos < 0) {
                scrollPos = 0;
            }
            if (scrollPos > (iface->scrollableHeight - iface->height)) {
                scrollPos = iface->scrollableHeight - iface->height;
            }
            iface->scrollPosition = scrollPos;
        }
    }

    void Game::RestoreVarCache()
    {
        for (int32_t id = 0; id < varps.size(); id++) {
            if (varps[id] != varCache[id]) {
                varps[id] = varCache[id];
                UpdateVarp(id);
                redrawSidebar = true;
            }
        }
    }

    void Game::DrawTileHint()
    {
        if (hintType != 2) {
            return;
        }

        ProjectFromGround(((hintTileX - sceneBaseTileX) << 7) + hintOffsetX, hintHeight * 2, ((hintTileZ - sceneBaseTileZ) << 7) + hintOffsetZ);

        if ((projectX > -1) && ((loopCycle % 20) < 10)) {
            imageHeadicons[2]->Draw(projectX - 12, projectY - 28);
        }
    }

    void Game::DrawHealth(const std::shared_ptr<PathingEntity>& entity)
    {
        if (entity->combatCycle > loopCycle) {
            ProjectFromGround(entity, entity->height + 15);

            if (projectX > -1) {
                int32_t w = (entity->health * 30) / entity->totalHealth;

                if (w > 30) {
                    w = 30;
                }

                Draw2D::FillRect(projectX - 15, projectY - 3, w, 5, 65280);
                Draw2D::FillRect((projectX - 15) + w, projectY - 3, 30 - w, 5, 0xff0000);
            }
        }
    }

    void Game::DrawHitmarks(const std::shared_ptr<PathingEntity>& entity)
    {
        for (int32_t j = 0; j < 4; j++) {
            if (entity->damageCycle[j] <= loopCycle) {
                continue;
            }

            ProjectFromGround(entity, entity->height / 2);

            if (projectX > -1) {
                if (j == 1) {
                    projectY -= 20;
                }
                if (j == 2) {
                    projectX -= 15;
                    projectY -= 10;
                }
                if (j == 3) {
                    projectX += 15;
                    projectY -= 10;
                }
                imageHitmarks[entity->damageType[j]]->Draw(projectX - 12, projectY - 12);
                fontPlain11->DrawStringCenter(std::to_string(entity->damage[j]), projectX, projectY + 4, 0);
                fontPlain11->DrawStringCenter(std::to_string(entity->damage[j]), projectX - 1, projectY + 3, 0xffffff);
            }
        }
    }

    void Game::ReadHintArrow()
    {
        hintType = in.ReadU8();

        if (hintType == 1) {
            hintNPC = in.ReadU16();
        }

        if ((hintType >= 2) && (hintType <= 6)) {
            if (hintType == 2) {
                hintOffsetX = 64;
                hintOffsetZ = 64;
            }
            if (hintType == 3) {
                hintOffsetX = 0;
                hintOffsetZ = 64;
            }
            if (hintType == 4) {
                hintOffsetX = 128;
                hintOffsetZ = 64;
            }
            if (hintType == 5) {
                hintOffsetX = 64;
                hintOffsetZ = 0;
            }
            if (hintType == 6) {
                hintOffsetX = 64;
                hintOffsetZ = 128;
            }
            hintType = 2;
            hintTileX = in.ReadU16();
            hintTileZ = in.ReadU16();
            hintHeight = in.ReadU8();
        }

        if (hintType == 10) {
            hintPlayer = in.ReadU16();
        }
    }

    void Game::ReadIfViewportAndSidebar()
    {
        int32_t viewportInterfaceID = in.ReadU16A();
        int32_t sidebarInterfaceID = in.ReadU16();

        if (chatInterfaceID != -1) {
            chatInterfaceID = -1;
            redrawChatback = true;
        }

        if (chatbackInputType != 0) {
            chatbackInputType = 0;
            redrawChatback = true;
        }

        this->viewportInterfaceID = viewportInterfaceID;
        this->sidebarInterfaceID = sidebarInterfaceID;
        redrawSidebar = true;
        redrawSideicons = true;
        pressedContinueOption = false;
    }

    void Game::UseReportAbuseOption(const std::string& input)
    {
        if (viewportInterfaceID == -1) {
            CloseInterfaces();
            reportAbuseInput = input;
            reportAbuseMuteOption = false;
            for (const auto & instance : IfType::instances) {
                if ((instance == nullptr) || (instance->contentType != 600)) {
                    continue;
                }
                reportAbuseInterfaceID = viewportInterfaceID = instance->parentID;
                break;
            }
        } else {
            AddMessage(0, "", "Please close the interface you have open before using 'report abuse'");
        }
    }

    void Game::CloseInterfaces()
    {
        out.WriteOp(130);
        if (sidebarInterfaceID != -1) {
            sidebarInterfaceID = -1;
            redrawSidebar = true;
            pressedContinueOption = false;
            redrawSideicons = true;
        }
        if (chatInterfaceID != -1) {
            chatInterfaceID = -1;
            redrawChatback = true;
            pressedContinueOption = false;
        }
        viewportInterfaceID = -1;
    }

    void Game::ReadIfSetAnim()
    {
        int32_t interfaceID = in.ReadU16();
        int32_t seqID = in.Read16();
        const auto& iface = IfType::instances[interfaceID];
        iface->seqID = seqID;
        if (seqID == -1) {
            iface->seqFrame = 0;
            iface->seqCycle = 0;
        }
    }

    void Game::ReadIfChat()
    {
        int32_t interfaceID = in.ReadU16LE();
        ResetInterfaceAnimation(interfaceID);
        if (sidebarInterfaceID != -1) {
            sidebarInterfaceID = -1;
            redrawSidebar = true;
            redrawSideicons = true;
        }
        chatInterfaceID = interfaceID;
        redrawChatback = true;
        viewportInterfaceID = -1;
        pressedContinueOption = false;
    }

    void Game::OpenViewportInterface(int32_t interfaceID)
    {
        if (sidebarInterfaceID != -1) {
            sidebarInterfaceID = -1;
            redrawSidebar = true;
            redrawSideicons = true;
        }
        if (chatInterfaceID != -1) {
            chatInterfaceID = -1;
            redrawChatback = true;
        }
        if (chatbackInputType != 0) {
            chatbackInputType = 0;
            redrawChatback = true;
        }
        viewportInterfaceID = interfaceID;
        pressedContinueOption = false;
    }

    void Game::ResetInterfaceAnimation(int32_t interfaceID)
    {
        const auto& iface = IfType::instances[interfaceID];

        for (int32_t i = 0; i < iface->childID.size(); i++) {
            if (iface->childID[i] == -1) {
                break;
            }

            const auto& child = IfType::instances[iface->childID[i]];

            if (child->type == IfType::TYPE_UNUSED) {
                ResetInterfaceAnimation(child->id);
            }

            child->seqFrame = 0;
            child->seqCycle = 0;
        }
    }

    void Game::PromptMessageFriend(int64_t name37)
    {
        int32_t friendSlot = -1;

        for (int32_t i = 0; i < friendCount; i++) {
            if (friendName37[i] == name37) {
                friendSlot = i;
                break;
            }
        }

        if ((friendSlot != -1) && (friendWorld[friendSlot] > 0)) {
            redrawChatback = true;
            chatbackInputType = 0;
            showSocialInput = true;
            socialInput = "";
            socialAction = 3;
            inputFriendName37 = friendName37[friendSlot];
            socialMessage = "Enter message to send to " + friendName[friendSlot];
        }
    }

    void Game::ReadSynthSound()
    {
        int32_t waveID = in.ReadU16();
        int32_t loopCount = in.ReadU8();
        int32_t delay = in.ReadU16();

        if (waveEnabled && !lowmem && (waveCount < MAX_WAVES))
        {
            waveIDs[waveCount] = waveID;
            waveLoops[waveCount] = loopCount;
            waveDelay[waveCount] = delay + SoundTrack::delays[waveID];
            waveCount++;
        }
    }

    void Game::SetWaveVolume(int32_t volume)
    {
        if (audioManager != nullptr)
        {
            audioManager->SetSoundVolume(volume);
        }
    }

    void Game::MidiVol(bool enabled, int32_t volumeMillibels)
    {
        if (audioManager != nullptr && enabled)
        {
            audioManager->SetMIDIVolume(volumeMillibels);
        }
    }

    void Game::ReadMidiSong()
    {
        int32_t next = in.ReadU16LE();
        if (next == 65535)
        {
            next = -1;
        }

        if ((next != nextSong) && midiEnabled && !lowmem && (nextSongDelay == 0))
        {
            song = next;
            songFading = true;
            ondemand->Request(2, song);
        }
        nextSong = next;
    }

    void Game::ReadMidiJingle()
    {
        int32_t next = in.ReadU16LEA();
        int32_t delay = in.ReadU16A();

        if (midiEnabled && !lowmem)
        {
            song = next;
            songFading = false;
            ondemand->Request(2, song);
            nextSongDelay = delay;
        }
    }

    void Game::ReadMapSound()
    {
        int32_t pos = in.ReadU8();
        int32_t x = baseX + ((pos >> 4) & 7);
        int32_t z = baseZ + (pos & 7);
        int32_t waveID = in.ReadU16();
        int32_t info = in.ReadU8();
        int32_t maxDist = (info >> 4) & 0xf;
        int32_t loopCount = info & 0b111;

        // Check if player is within range of the sound
        if (localPlayer != nullptr &&
            (localPlayer->pathTileX[0] >= (x - maxDist)) &&
            (localPlayer->pathTileX[0] <= (x + maxDist)) &&
            (localPlayer->pathTileZ[0] >= (z - maxDist)) &&
            (localPlayer->pathTileZ[0] <= (z + maxDist)) &&
            waveEnabled && !lowmem && (waveCount < MAX_WAVES))
        {
            waveIDs[waveCount] = waveID;
            waveLoops[waveCount] = loopCount;
            waveDelay[waveCount] = SoundTrack::delays[waveID];
            waveCount++;
        }
    }

    void Game::PlayMidi(bool fade, std::vector<int8_t>& data)
    {
        if (audioManager != nullptr)
        {
            audioManager->PlayMIDI(data, fade);
        }
    }

    void Game::StopMidi()
    {
        if (audioManager != nullptr)
        {
            audioManager->StopMIDI();
        }
    }

    void Game::Debug()
    {
        LOG_INFO("============");
        if (ondemand != nullptr) {
            LOG_INFO("Od-cycle:%d", ondemand->cycle);
            LOG_INFO("Od-remaining:%d", ondemand->Remaining());
            for (const auto& request : ondemand->requests) {
                LOG_INFO("Request: store=%d file=%d important=%s cycle=%d",
                         request->store, request->file,
                         request->important ? "true" : "false",
                         request->cycle);
            }
        }
        LOG_INFO("loop-cycle:%d", loopCycle);
        LOG_INFO("ptype:%d", packetType);
        LOG_INFO("psize:%d", packetSize);
        LOG_INFO("scene-state:%d", sceneState);
        LOG_INFO("draw-state:%d", CheckScene());
        if (connection) {
            connection->Debug();
        }
        debug = true;
    }

    void Game::UpdateAudio()
    {
        for (int32_t wave = 0; wave < waveCount; wave++)
        {
            if (waveDelay[wave] > 0)
            {
                waveDelay[wave]--;
                continue;
            }

            bool failed = false;

            if (audioManager != nullptr)
            {
                if (!audioManager->PlaySound(waveIDs[wave], waveLoops[wave]))
                {
                    failed = true;
                }
            }
            else
            {
                failed = true;
            }

            if (!failed || (waveDelay[wave] == -5))
            {
                waveCount--;
                for (int32_t i = wave; i < waveCount; i++)
                {
                    waveIDs[i] = waveIDs[i + 1];
                    waveLoops[i] = waveLoops[i + 1];
                    waveDelay[i] = waveDelay[i + 1];
                }
                wave--;
            }
            else
            {
                waveDelay[wave] = -5;
            }
        }

        if (nextSongDelay > 0)
        {
            nextSongDelay -= 20;

            if (nextSongDelay < 0)
            {
                nextSongDelay = 0;
            }

            if ((nextSongDelay == 0) && midiEnabled && !lowmem)
            {
                song = nextSong;
                songFading = true;
                ondemand->Request(2, song);
            }
        }
    }

    void Game::PushPlayers(bool local)
    {
        if (((localPlayer->x >> 7) == flagSceneTileX) && ((localPlayer->z >> 7) == flagSceneTileZ)) {
            flagSceneTileX = 0;
        }

        int32_t count = this->playerCount;

        if (local) {
            count = 1;
        }

        for (int32_t i = 0; i < count; i++) {
            std::shared_ptr<PlayerEntity> player;
            int32_t index;

            if (local) {
                player = localPlayer;
                index = LOCAL_PLAYER_INDEX << 14;
            } else {
                player = players[playerIDs[i]];
                index = playerIDs[i] << 14;
            }

            if ((player == nullptr) || !player->IsVisible()) {
                continue;
            }

            PlayerEntity::lowmem = ((lowmem && (playerCount > 50)) || (playerCount > 200)) && !local && (player->secondarySeqID == player->seqStandID);

            int32_t stx = player->x >> 7;
            int32_t stz = player->z >> 7;

            if ((stx < 0) || (stx >= 104) || (stz < 0) || (stz >= 104)) {
                continue;
            }

            // transform player into object
            if ((player->locModel != nullptr) && (loopCycle >= player->locStartCycle) && (loopCycle < player->locStopCycle)) {
                PlayerEntity::lowmem = false;
                player->y = GetHeightmapY(currentLevel, player->x, player->z);
                scene->AddTemporary(player, currentLevel, player->minSceneTileX,
                    player->minSceneTileZ, player->maxSceneTileX, player->maxSceneTileZ,
                    player->x, player->z, player->y, player->yaw, index);
                continue;
            }

            if (((player->x & 0x7f) == 64) && ((player->z & 0x7f) == 64)) {
                if (tileLastOccupiedCycle[stx][stz] == sceneCycle) {
                    continue;
                }
                tileLastOccupiedCycle[stx][stz] = sceneCycle;
            }

            player->y = GetHeightmapY(currentLevel, player->x, player->z);
            scene->AddTemporary(player, currentLevel, player->x, player->z, player->y, player->yaw, index, player->needsForwardDrawPadding, 60);
        }
    }

    void Game::UpdatePlayers()
    {
        for (int32_t i = -1; i < playerCount; i++) {
            int32_t playerID;
            if (i == -1) {
                playerID = LOCAL_PLAYER_INDEX;
            } else {
                playerID = playerIDs[i];
            }
            auto player = players[playerID];
            if (player != nullptr) {
                UpdateEntity(player);
            }
        }
    }

    void Game::UpdateNPCs()
    {
        for (int32_t i = 0; i < npcCount; i++) {
            const auto& npc = npcs[npcIDs[i]];
            if (npc != nullptr) {
                UpdateEntity(npc);
            }
        }
    }

    void Game::PushNPCs(bool important)
    {
        for (int32_t i = 0; i < npcCount; i++) {
            const auto& npc = npcs[npcIDs[i]];
            int32_t bitset = 0x20000000 + (npcIDs[i] << 14);

            if (npc == nullptr || !npc->IsVisible() || (npc->type->important != important)) {
                continue;
            }

            int32_t x = npc->x >> 7;
            int32_t z = npc->z >> 7;

            if ((x < 0) || (x >= 104) || (z < 0) || (z >= 104)) {
                continue;
            }

            if ((npc->size == 1) && ((npc->x & 0x7f) == 64) && ((npc->z & 0x7f) == 64)) {
                if (tileLastOccupiedCycle[x][z] == sceneCycle) {
                    continue;
                }
                tileLastOccupiedCycle[x][z] = sceneCycle;
            }

            if (!npc->type->interactable) {
                bitset += 0x80000000;
            }

            scene->AddTemporary(npc, currentLevel, npc->x, npc->z, GetHeightmapY(currentLevel, npc->x, npc->z),
                npc->yaw, bitset, npc->needsForwardDrawPadding, ((npc->size - 1) * 64) + 60);
        }
    }

    void Game::UpdateIdleCycles()
    {
        idleCycles++;

        if (idleCycles > 4500) {
            idleTimeout = 250;
            idleCycles -= 500;
            out.WriteOp(202);
        }
    }

    void Game::UpdateEntityChats()
    {
        for (int32_t i = -1; i < playerCount; i++) {
            int32_t playerID;

            if (i == -1) {
                playerID = LOCAL_PLAYER_INDEX;
            } else {
                playerID = playerIDs[i];
            }

            const auto& player = players[playerID];

            if ((player != nullptr) && (player->chatTimer > 0)) {
                player->chatTimer--;
                if (player->chatTimer == 0) {
                    player->chat.clear();
                }
            }
        }

        for (int32_t i = 0; i < npcCount; i++) {
            int32_t id = npcIDs[i];
            const auto& npc = npcs[id];

            if ((npc != nullptr) && (npc->chatTimer > 0)) {
                npc->chatTimer--;
                if (npc->chatTimer == 0) {
                    npc->chat.clear();
                }
            }
        }
    }

    void Game::UpdateEntity(const std::shared_ptr<PathingEntity>& entity)
    {
        if ((entity->x < 128) || (entity->z < 128) || (entity->x >= 13184) || (entity->z >= 13184)) {
            entity->primarySeqID = -1;
            entity->spotanimID = -1;
            entity->forceMoveEndCycle = 0;
            entity->forceMoveStartCycle = 0;
            entity->x = (entity->pathTileX[0] * 128) + (entity->size * 64);
            entity->z = (entity->pathTileZ[0] * 128) + (entity->size * 64);
            entity->ResetPath();
        }
        if ((entity == localPlayer) && ((entity->x < 1536) || (entity->z < 1536) || (entity->x >= 11776) || (entity->z >= 11776))) {
            entity->primarySeqID = -1;
            entity->spotanimID = -1;
            entity->forceMoveEndCycle = 0;
            entity->forceMoveStartCycle = 0;
            entity->x = (entity->pathTileX[0] * 128) + (entity->size * 64);
            entity->z = (entity->pathTileZ[0] * 128) + (entity->size * 64);
            entity->ResetPath();
        }
        if (entity->forceMoveEndCycle > loopCycle) {
            UpdateForceMovement(*entity);
        } else if (entity->forceMoveStartCycle >= loopCycle) {
            StartForceMovement(*entity);
        } else {
            UpdateMovement(*entity);
        }
        UpdateFacingDirection(*entity);
        UpdateSequences(*entity);
    }

    void Game::UpdateSequences(PathingEntity& e) const
    {
        e.needsForwardDrawPadding = false;

        if (e.secondarySeqID != -1) {
            auto& seq = SeqType::instances[e.secondarySeqID];
            e.secondarySeqCycle++;

            if ((e.secondarySeqFrame < seq.frameCount) && (e.secondarySeqCycle > seq.GetFrameDuration(e.secondarySeqFrame))) {
                e.secondarySeqCycle = 0;
                e.secondarySeqFrame++;
            }

            if (e.secondarySeqFrame >= seq.frameCount) {
                e.secondarySeqCycle = 0;
                e.secondarySeqFrame = 0;
            }
        }

        if (e.spotanimID != -1 && (loopCycle >= e.spotanimLastCycle)) {
            if (e.spotanimFrame < 0) {
                e.spotanimFrame = 0;
            }

            auto& seq = SpotAnimType::instances[e.spotanimID].seq;

            for (e.spotanimCycle++; (e.spotanimFrame < seq.frameCount) && (e.spotanimCycle > seq.GetFrameDuration(e.spotanimFrame)); e.spotanimFrame++) {
                e.spotanimCycle -= seq.GetFrameDuration(e.spotanimFrame);
            }

            if (e.spotanimFrame >= seq.frameCount && ((e.spotanimFrame < 0) || (e.spotanimFrame >= seq.frameCount))) {
                e.spotanimID = -1;
            }
        }

        if ((e.primarySeqID != -1) && (e.primarySeqDelay <= 1)) {
            auto& seq = SeqType::instances[e.primarySeqID];

            // we're moving, and it's not due to a force move:
            //  pause primary sequence
            if ((seq.moveStyle == 1) && (e.seqPathLength > 0) && (e.forceMoveEndCycle <= loopCycle) && (e.forceMoveStartCycle < loopCycle)) {
                e.primarySeqDelay = 1;
                return;
            }
        }

        if ((e.primarySeqID != -1) && (e.primarySeqDelay == 0)) {
            auto& seq = SeqType::instances[e.primarySeqID];

            for (e.primarySeqCycle++; (e.primarySeqFrame < seq.frameCount) && (e.primarySeqCycle > seq.GetFrameDuration(e.primarySeqFrame)); e.primarySeqFrame++) {
                e.primarySeqCycle -= seq.GetFrameDuration(e.primarySeqFrame);
            }

            if (e.primarySeqFrame >= seq.frameCount) {
                e.primarySeqFrame -= seq.loopFrameCount;
                e.primarySeqLoop++;

                if (e.primarySeqLoop >= seq.loopCount) {
                    e.primarySeqID = -1;
                }
                if ((e.primarySeqFrame < 0) || (e.primarySeqFrame >= seq.frameCount)) {
                    e.primarySeqID = -1;
                }
            }
            e.needsForwardDrawPadding = seq.forwardRenderPadding;
        }

        if (e.primarySeqDelay > 0) {
            e.primarySeqDelay--;
        }
    }

    void Game::UpdateMovement(PathingEntity& entity)
    {
        entity.secondarySeqID = entity.seqStandID;

        if (entity.pathLength == 0) {
            entity.seqTrigger = 0;
            return;
        }

        if ((entity.primarySeqID != -1) && (entity.primarySeqDelay == 0)) {
            auto& seq = SeqType::instances[entity.primarySeqID];

            // if we're moving, and our move style is 0:
            //      Move faster[, and look a tile/entity if applicable.] (Side effects of seqTrigger)
            if ((entity.seqPathLength > 0) && (seq.moveStyle == 0)) {
                entity.seqTrigger++;
                return;
            }

            // if we're stationary, and our idle style is 0:
            //      Look at a tile/entity if applicable. (Side effect of seqTrigger)
            if ((entity.seqPathLength <= 0) && (seq.idleStyle == 0)) {
                entity.seqTrigger++;
                return;
            }
        }

        int32_t x = entity.x;
        int32_t z = entity.z;
        int32_t dstX = (entity.pathTileX[entity.pathLength - 1] * 128) + (entity.size * 64);
        int32_t dstZ = (entity.pathTileZ[entity.pathLength - 1] * 128) + (entity.size * 64);

        if (((dstX - x) > 256) || ((dstX - x) < -256) || ((dstZ - z) > 256) || ((dstZ - z) < -256)) {
            entity.x = dstX;
            entity.z = dstZ;
            return;
        }

        if (x < dstX) {
            if (z < dstZ) {
                entity.dstYaw = 1280;
            } else if (z > dstZ) {
                entity.dstYaw = 1792;
            } else {
                entity.dstYaw = 1536;
            }
        } else if (x > dstX) {
            if (z < dstZ) {
                entity.dstYaw = 768;
            } else if (z > dstZ) {
                entity.dstYaw = 256;
            } else {
                entity.dstYaw = 512;
            }
        } else if (z < dstZ) {
            entity.dstYaw = 1024;
        } else {
            entity.dstYaw = 0;
        }

        int32_t remainingYaw = (entity.dstYaw - entity.yaw) & 0x7ff;

        if (remainingYaw > 1024) {
            remainingYaw -= 2048;
        }

        int32_t seqID = entity.seqTurnAroundID;

        // Since the game uses a left-handed coordinate system, an increasing angle goes clockwise.
        // See PreviewSinCos2D

        // yaw >= -45 deg && yaw <= 45 deg
        if ((remainingYaw >= -256) && (remainingYaw <= 256)) {
            seqID = entity.seqWalkID;
        }
        // yaw >= 45 deg && yaw <= 135 deg
        else if ((remainingYaw >= 256) && (remainingYaw < 768)) {
            seqID = entity.seqTurnRightID;
        }
        // yaw >= -135 deg && yaw <= -45 deg
        else if ((remainingYaw >= -768) && (remainingYaw <= -256)) {
            seqID = entity.seqTurnLeftID;
        }

        if (seqID == -1) {
            seqID = entity.seqWalkID;
        }

        entity.secondarySeqID = seqID;

        int32_t moveSpeed = 4;

        if ((entity.yaw != entity.dstYaw) && (entity.targetID == -1) && (entity.turnSpeed != 0)) {
            moveSpeed = 2;
        }

        if (entity.pathLength > 2) {
            moveSpeed = 6;
        }

        if (entity.pathLength > 3) {
            moveSpeed = 8;
        }

        if ((entity.seqTrigger > 0) && (entity.pathLength > 1)) {
            moveSpeed = 8;
            entity.seqTrigger--;
        }

        if (entity.pathRunning[entity.pathLength - 1]) {
            moveSpeed <<= 1;
        }

        if ((moveSpeed >= 8) && (entity.secondarySeqID == entity.seqWalkID) && (entity.seqRunID != -1)) {
            entity.secondarySeqID = entity.seqRunID;
        }

        if (x < dstX) {
            entity.x += moveSpeed;
            if (entity.x > dstX) {
                entity.x = dstX;
            }
        } else if (x > dstX) {
            entity.x -= moveSpeed;
            if (entity.x < dstX) {
                entity.x = dstX;
            }
        }
        if (z < dstZ) {
            entity.z += moveSpeed;
            if (entity.z > dstZ) {
                entity.z = dstZ;
            }
        } else if (z > dstZ) {
            entity.z -= moveSpeed;
            if (entity.z < dstZ) {
                entity.z = dstZ;
            }
        }
        if ((entity.x == dstX) && (entity.z == dstZ)) {
            entity.pathLength--;
            if (entity.seqPathLength > 0) {
                entity.seqPathLength--;
            }
        }
    }

    /**
     * Updates the components sequences if there are any. This method requires that the initial <code>id</code> belong
     * to a component of type <code>1</code> (PARENT).
     *
     * @param delta the delta.
     * @param id    the parent interface id.
     * @return <code>true</code> if there was a sequence which updated.
     */
    bool Game::UpdateInterfaceAnimation(int32_t delta, int32_t id)
    {
        bool updated = false;
        const auto& parent = IfType::instances[id];

        for (int32_t k = 0; k < parent->childID.size(); k++) {
            if (parent->childID[k] == -1) {
                break;
            }

            const auto& child = IfType::instances[parent->childID[k]];

            if (child->type == IfType::TYPE_UNUSED) {
                updated |= UpdateInterfaceAnimation(delta, child->id);
            }

            if ((child->type == IfType::TYPE_MODEL) && ((child->seqID != -1) || (child->activeSeqID != -1))) {
                bool active = ExecuteInterfaceScript(*child);
                int32_t seqID;

                if (active) {
                    seqID = child->activeSeqID;
                } else {
                    seqID = child->seqID;
                }

                if (seqID != -1) {
                    auto& type = SeqType::instances[seqID];
                    for (child->seqCycle += delta; child->seqCycle > type.GetFrameDuration(child->seqFrame); ) {
                        child->seqCycle -= type.GetFrameDuration(child->seqFrame) + 1;
                        child->seqFrame++;
                        if (child->seqFrame >= type.frameCount) {
                            child->seqFrame -= type.loopFrameCount;
                            if ((child->seqFrame < 0) || (child->seqFrame >= type.frameCount)) {
                                child->seqFrame = 0;
                            }
                        }
                        updated = true;
                    }
                }
            }
        }
        return updated;
    }

    void Game::UpdateFacingDirection(PathingEntity& e) const
    {
        if (e.turnSpeed == 0) {
            return;
        }

        if ((e.targetID != -1) && (e.targetID < 32768)) {
            const auto& npc = npcs[e.targetID];

            if (npc != nullptr) {
                int32_t dstX = e.x - npc->x;
                int32_t dstZ = e.z - npc->z;

                if ((dstX != 0) || (dstZ != 0)) {
                    e.dstYaw = static_cast<int>(SDL_atan2(dstX, dstZ) * 325.949) & 0x7ff;
                }
            }
        }
        if (e.targetID >= 32768) {
            int32_t id = e.targetID - 32768;

            if (id == localPID) {
                id = LOCAL_PLAYER_INDEX;
            }

            auto& player = players[id];

            if (player != nullptr) {
                int32_t dstX = e.x - player->x;
                int32_t dstZ = e.z - player->z;

                if ((dstX != 0) || (dstZ != 0)) {
                    e.dstYaw = (static_cast<int32_t>(std::atan2(dstX, dstZ) * 325.94900000000001)) & 0x7FF;
                }
            }
        }

        if (((e.targetTileX != 0) || (e.targetTileZ != 0)) && ((e.pathLength == 0) || (e.seqTrigger > 0))) {
            int32_t dstX = e.x - ((e.targetTileX - sceneBaseTileX - sceneBaseTileX) * 64);
            int32_t dstZ = e.z - ((e.targetTileZ - sceneBaseTileZ - sceneBaseTileZ) * 64);

            if ((dstX != 0) || (dstZ != 0)) {
                e.dstYaw = (static_cast<int32_t>(std::atan2(dstX, dstZ) * 325.94900000000001)) & 0x7FF;
            }

            e.targetTileX = 0;
            e.targetTileZ = 0;
        }

        int32_t remainingYaw = (e.dstYaw - e.yaw) & 0x7ff;

        if (remainingYaw != 0) {
            if ((remainingYaw < e.turnSpeed) || (remainingYaw > (2048 - e.turnSpeed))) {
                e.yaw = e.dstYaw;
            } else if (remainingYaw > 1024) {
                e.yaw -= e.turnSpeed;
            } else {
                e.yaw += e.turnSpeed;
            }

            e.yaw &= 0x7ff;

            if ((e.secondarySeqID == e.seqStandID) && (e.yaw != e.dstYaw)) {
                if (e.seqTurnID != -1) {
                    e.secondarySeqID = e.seqTurnID;
                    return;
                }

                e.secondarySeqID = e.seqWalkID;
            }
        }
    }

    void Game::UpdateForceMovement(PathingEntity& entity) const
    {
        int32_t cycleDelta = entity.forceMoveEndCycle - loopCycle;
        int32_t dstX = (entity.forceMoveStartSceneTileX * 128) + (entity.size * 64);
        int32_t dstZ = (entity.forceMoveStartSceneTileZ * 128) + (entity.size * 64);

        entity.x += (dstX - entity.x) / cycleDelta;
        entity.z += (dstZ - entity.z) / cycleDelta;

        entity.seqTrigger = 0;

        if (entity.forceMoveFaceDirection == 0) {
            entity.dstYaw = 1024;
        }
        if (entity.forceMoveFaceDirection == 1) {
            entity.dstYaw = 1536;
        }
        if (entity.forceMoveFaceDirection == 2) {
            entity.dstYaw = 0;
        }
        if (entity.forceMoveFaceDirection == 3) {
            entity.dstYaw = 512;
        }
    }

    void Game::StartForceMovement(PathingEntity& entity) const
    {
        if ((entity.forceMoveStartCycle == loopCycle) || (entity.primarySeqID == -1) || (entity.primarySeqDelay != 0) || ((entity.primarySeqCycle + 1) > SeqType::instances[entity.primarySeqID].GetFrameDuration(entity.primarySeqFrame))) {
            int32_t duration = entity.forceMoveStartCycle - entity.forceMoveEndCycle;
            int32_t cycleDelta = loopCycle - entity.forceMoveEndCycle;
            int32_t dx0 = (entity.forceMoveStartSceneTileX * 128) + (entity.size * 64);
            int32_t dz0 = (entity.forceMoveStartSceneTileZ * 128) + (entity.size * 64);
            int32_t dx1 = (entity.forceMoveEndSceneTileX * 128) + (entity.size * 64);
            int32_t dz1 = (entity.forceMoveEndSceneTileZ * 128) + (entity.size * 64);
            entity.x = ((dx0 * (duration - cycleDelta)) + (dx1 * cycleDelta)) / duration;
            entity.z = ((dz0 * (duration - cycleDelta)) + (dz1 * cycleDelta)) / duration;
        }

        entity.seqTrigger = 0;

        if (entity.forceMoveFaceDirection == 0) {
            entity.dstYaw = 1024;
        }
        if (entity.forceMoveFaceDirection == 1) {
            entity.dstYaw = 1536;
        }
        if (entity.forceMoveFaceDirection == 2) {
            entity.dstYaw = 0;
        }
        if (entity.forceMoveFaceDirection == 3) {
            entity.dstYaw = 512;
        }
        entity.yaw = entity.dstYaw;
    }

    void Game::ResetAnimations()
    {
        for (const auto& player : players) {
            if (player != nullptr) {
                player->primarySeqID = -1;
            }
        }
        for (const auto& npc : npcs) {
            if (npc != nullptr) {
                npc->primarySeqID = -1;
            }
        }
    }

    void Game::AppendLoc(int32_t duration, int32_t id, int32_t rotation, int32_t classID, int32_t z, int32_t kind,
        int32_t level, int32_t x, int32_t delay)
    {
        SceneLocTemporary* loc = nullptr;
        int32_t searchCount = 0;
        for (auto other = static_cast<SceneLocTemporary*>(temporaryLocs.peekFront()); other != nullptr; other = static_cast<SceneLocTemporary*>(temporaryLocs.prev())) {
            searchCount++;
            if ((other->level != level) || (other->localX != x) || (other->localZ != z) || (other->classID != classID)) {
                continue;
            }
            loc = other;
            break;
        }
        if (loc == nullptr) {
            loc = new SceneLocTemporary();
            loc->level = level;
            loc->classID = classID;
            loc->localX = x;
            loc->localZ = z;
            StoreLoc(*loc);
            temporaryLocs.pushBack(loc);
        }
        loc->id = id;
        loc->kind = kind;
        loc->rotation = rotation;
        loc->delay = delay;
        loc->duration = duration;
    }

    void Game::ShiftScene()
    {
        int32_t dtx = sceneBaseTileX - scenePrevBaseTileX;
        int32_t dtz = sceneBaseTileZ - scenePrevBaseTileZ;
        scenePrevBaseTileX = sceneBaseTileX;
        scenePrevBaseTileZ = sceneBaseTileZ;

        for (int32_t i = 0; i < 16384; i++) {
            const auto& npc = npcs[i];

            if (npc != nullptr) {
                for (int32_t j = 0; j < 10; j++) {
                    npc->pathTileX[j] -= dtx;
                    npc->pathTileZ[j] -= dtz;
                }
                npc->x -= dtx * 128;
                npc->z -= dtz * 128;
            }
        }

        for (int32_t i = 0; i < MAX_PLAYER_COUNT; i++) {
            const auto& player = players[i];

            if (player != nullptr) {
                for (int32_t i31 = 0; i31 < 10; i31++) {
                    player->pathTileX[i31] -= dtx;
                    player->pathTileZ[i31] -= dtz;
                }
                player->x -= dtx * 128;
                player->z -= dtz * 128;
            }
        }

        awaitingSync = true;

        int8_t x0 = 0;
        int8_t x1 = 104;
        int8_t dirX = 1;

        if (dtx < 0) {
            x0 = 103;
            x1 = -1;
            dirX = -1;
        }

        int8_t z0 = 0;
        int8_t z1 = 104;
        int8_t dirZ = 1;

        if (dtz < 0) {
            z0 = 103;
            z1 = -1;
            dirZ = -1;
        }

        for (int32_t x = x0; x != x1; x += dirX) {
            for (int32_t z = z0; z != z1; z += dirZ) {
                int32_t dstX = x + dtx;
                int32_t dstZ = z + dtz;

                for (int32_t level = 0; level < 4; level++) {
                    if ((dstX >= 0) && (dstZ >= 0) && (dstX < 104) && (dstZ < 104)) {
                        levelObjStacks[level][x][z] = std::move(levelObjStacks[level][dstX][dstZ]);
                    } else {
                        levelObjStacks[level][x][z].clear();
                    }
                }
            }
        }

        for (auto loc = static_cast<SceneLocTemporary*>(temporaryLocs.peekFront()); loc != nullptr; loc = static_cast<SceneLocTemporary*>(temporaryLocs.prev())) {
            loc->localX -= dtx;
            loc->localZ -= dtz;
            if ((loc->localX < 0) || (loc->localZ < 0) || (loc->localX >= 104) || (loc->localZ >= 104)) {
                loc->unlink();
                delete loc;
            }
        }

        if (flagSceneTileX != 0) {
            flagSceneTileX -= dtx;
            flagSceneTileZ -= dtz;
        }
    }

    void Game::ApplyCameraAdjustments()
    {
        for (int32_t type = 0; type < 5; type++) {
            if (!cameraModifierEnabled[type]) {
                continue;
            }

            auto adjustment = static_cast<int32_t>(((SDL_randf() * static_cast<float>((cameraModifierJitter[type] * 2) + 1)) - static_cast<float>(cameraModifierJitter[type]))
                + (SDL_sinf(static_cast<float>(cameraModifierCycle[type]) * (static_cast<float>(cameraModifierWobbleSpeed[type]) / 100.0f)) * static_cast<float>(cameraModifierWobbleScale[type])));

            switch (type) {
            case 0:
                cameraX += adjustment;
                break;
            case 1:
                cameraY += adjustment;
                break;
            case 2:
                cameraZ += adjustment;
                break;
            case 3:
                cameraYaw = (cameraYaw + adjustment) & 0x7ff;
                break;
            case 4:
                cameraPitch += adjustment;
                if (cameraPitch < 128) {
                    cameraPitch = 128;
                }
                if (cameraPitch > 383) {
                    cameraPitch = 383;
                }
                break;
            default:
                LOG_ERROR("ApplyCameraAdjustments missing type: %i", type);
            }
        }
    }

    /**
     * @param entity the entity.
     * @param height the height off the ground.
     * @see #projectX
     * @see #projectY
     */
    void Game::ProjectFromGround(const std::shared_ptr<PathingEntity>& entity, int32_t height)
    {
        ProjectFromGround(entity->x, height, entity->z);
    }

    void Game::ProjectFromGround(int32_t x, int32_t height, int32_t z)
    {
        if (x < 128 || z < 128 || x > 13056 || z > 13056) {
            projectX = -1;
            projectY = -1;
            return;
        }
        Project(x, GetHeightmapY(currentLevel, x, z) - height, z);
    }

    void Game::Project(int32_t x, int32_t y, int32_t z)
    {
        x -= cameraX;
        y -= cameraY;
        z -= cameraZ;

        int32_t sinPitch = Draw3D::sin[cameraPitch];
        int32_t cosPitch = Draw3D::cos[cameraPitch];
        int32_t sinYaw = Draw3D::sin[cameraYaw];
        int32_t cosYaw = Draw3D::cos[cameraYaw];

        int32_t tmp = ((z * sinYaw) + (x * cosYaw)) >> 16;
        z = ((z * cosYaw) - (x * sinYaw)) >> 16;
        x = tmp;

        tmp = ((y * cosPitch) - (z * sinPitch)) >> 16;
        z = ((y * sinPitch) + (z * cosPitch)) >> 16;
        y = tmp;

        if (z >= 50) {
            projectX = Draw3D::centerX + ((x << 9) / z);
            projectY = Draw3D::centerY + ((y << 9) / z);
        } else {
            projectX = -1;
            projectY = -1;
        }
    }

    bool Game::IsFriend(const std::string& name) const
    {
        if (name.empty()) {
            return false;
        }
        for (int32_t i = 0; i < friendCount; i++) {
            if (StringUtil::EqualsIgnoreCase(name, friendName[i])) {
                return true;
            }
        }
        return StringUtil::EqualsIgnoreCase(name, localPlayer->name);
    }

    void Game::ClearTileFlags()
    {
        for (int32_t level = 0; level < 4; level++) {
            for (int32_t x = 0; x < 104; x++) {
                for (int32_t z = 0; z < 104; z++) {
                    levelTileFlags[level][x][z] = 0;
                }
            }
        }
    }

    bool Game::IsAddFriendOption(int32_t option)
    {
        if (option < 0) {
            return false;
        }
        int32_t action = menuAction[option];
        if (action >= 2000) {
            action -= 2000;
        }
        return action == 337;
    }

    void Game::AddFriend(int64_t name37)
    {
        if (name37 == 0L) {
            return;
        }
        if ((friendCount >= 100) && (isMember != 1)) {
            AddMessage(0, "", "Your friendlist is full. Max of 100 for free users, and 200 for members");
            return;
        }
        if (friendCount >= 200) {
            AddMessage(0, "", "Your friendlist is full. Max of 100 for free users, and 200 for members");
            return;
        }
        std::string s = StringUtil::FormatName(name37);
        for (int32_t i = 0; i < friendCount; i++) {
            if (friendName37[i] == name37) {
                AddMessage(0, "", s + " is already on your friend list");
                return;
            }
        }
        for (int32_t j = 0; j < ignoreCount; j++) {
            if (ignoreName37[j] == name37) {
                AddMessage(0, "", "Please remove " + s + " from your ignore list first");
                return;
            }
        }
        if (s != localPlayer->name) {
            friendName[friendCount] = s;
            friendName37[friendCount] = name37;
            friendWorld[friendCount] = 0;
            friendCount++;
            redrawSidebar = true;
            out.WriteOp(188);
            out.Write64(name37);
        }
    }

    void Game::AddIgnore(int64_t name37)
    {
        if (name37 == 0L) {
            return;
        }
        if (ignoreCount >= 100) {
            AddMessage(0, "", "Your ignore list is full. Max of 100 hit");
            return;
        }
        std::string s = StringUtil::FormatName(name37);
        for (int32_t j = 0; j < ignoreCount; j++) {
            if (ignoreName37[j] == name37) {
                AddMessage(0, "", s + " is already on your ignore list");
                return;
            }
        }
        for (int32_t k = 0; k < friendCount; k++) {
            if (friendName37[k] == name37) {
                AddMessage(0, "", "Please remove " + s + " from your friend list first");
                return;
            }
        }
        ignoreName37[ignoreCount++] = name37;
        redrawSidebar = true;
        out.WriteOp(133);
        out.Write64(name37);
    }

    void Game::PushProjectiles()
    {
        for (auto* node = projectiles.peekFront(); node != nullptr; node = projectiles.prev()) {
            auto* proj = static_cast<ProjectileEntity*>(node);
            if ((proj->level != currentLevel) || (loopCycle > proj->lastCycle)) {
                proj->unlink();
            } else if (loopCycle >= proj->startCycle) {
                if (proj->target > 0) {
                    const auto& npc = npcs[proj->target - 1];
                    if ((npc != nullptr) && (npc->x >= 0) && (npc->x < 13312) && (npc->z >= 0) && (npc->z < 13312)) {
                        proj->UpdateVelocity(loopCycle, npc->z, GetHeightmapY(proj->level, npc->x, npc->z) - proj->offsetY, npc->x);
                    }
                }
                if (proj->target < 0) {
                    int32_t pid = -proj->target - 1;
                    std::shared_ptr<PlayerEntity> player;
                    if (pid == localPID) {
                        player = localPlayer;
                    } else {
                        player = players[pid];
                    }
                    if ((player != nullptr) && (player->x >= 0) && (player->x < 13312) && (player->z >= 0) && (player->z < 13312)) {
                        proj->UpdateVelocity(loopCycle, player->z, GetHeightmapY(proj->level, player->x, player->z) - proj->offsetY, player->x);
                    }
                }
                proj->Update(delta);
                scene->AddTemporary(proj, currentLevel, static_cast<int32_t>(proj->x), static_cast<int32_t>(proj->z),
                    static_cast<int32_t>(proj->y), proj->yaw, -1, false, 60);
            }
        }
    }

    void Game::PushSpotanims()
    {
        for (auto anim = static_cast<SpotAnimEntity*>(spotanims.peekFront()); anim != nullptr; anim = static_cast<SpotAnimEntity*>(spotanims.prev())) {
            if ((anim->level != currentLevel) || anim->seqComplete) {
                anim->unlink();
                delete anim;
            } else if (loopCycle >= anim->startCycle) {
                anim->Update(delta);
                if (anim->seqComplete) {
                    anim->unlink();
                    delete anim;
                } else {
                    scene->AddTemporary(anim, anim->level, anim->x, anim->z, anim->y, 0, -1, false, 60);
                }
            }
        }
    }

    /**
     * Applies the cutscene camera properties to the main camera.
     *
     * @see #cutscene
     */
    void Game::ApplyCutscene()
    {
        // the following code updates the position of the camera
        int32_t x = (cutsceneSrcLocalTileX * 128) + 64;
        int32_t z = (cutsceneSrcLocalTileZ * 128) + 64;
        int32_t y = GetHeightmapY(currentLevel, x, z) - cutsceneSrcHeight;

        if (cameraX < x) {
            cameraX += cutsceneMoveSpeed + (((x - cameraX) * cutsceneMoveAcceleration) / 1000);
            if (cameraX > x) {
                cameraX = x;
            }
        }
        if (cameraX > x) {
            cameraX -= cutsceneMoveSpeed + (((cameraX - x) * cutsceneMoveAcceleration) / 1000);
            if (cameraX < x) {
                cameraX = x;
            }
        }
        if (cameraY < y) {
            cameraY += cutsceneMoveSpeed + (((y - cameraY) * cutsceneMoveAcceleration) / 1000);
            if (cameraY > y) {
                cameraY = y;
            }
        }
        if (cameraY > y) {
            cameraY -= cutsceneMoveSpeed + (((cameraY - y) * cutsceneMoveAcceleration) / 1000);
            if (cameraY < y) {
                cameraY = y;
            }
        }
        if (cameraZ < z) {
            cameraZ += cutsceneMoveSpeed + (((z - cameraZ) * cutsceneMoveAcceleration) / 1000);
            if (cameraZ > z) {
                cameraZ = z;
            }
        }
        if (cameraZ > z) {
            cameraZ -= cutsceneMoveSpeed + (((cameraZ - z) * cutsceneMoveAcceleration) / 1000);
            if (cameraZ < z) {
                cameraZ = z;
            }
        }

        // the following code updates the angle of the camera

        x = (cutsceneDstLocalTileX * 128) + 64;
        z = (cutsceneDstLocalTileZ * 128) + 64;
        y = GetHeightmapY(currentLevel, x, z) - cutsceneDstHeight;

        int32_t deltaX = x - cameraX;
        int32_t deltaY = y - cameraY;
        int32_t deltaZ = z - cameraZ;

        int32_t distance = static_cast<int32_t>(SDL_sqrt((deltaX * deltaX) + (deltaZ * deltaZ)));
        int32_t pitch = static_cast<int32_t>(SDL_atan2(deltaY, distance) * 325.95) & 0x7ff;
        int32_t yaw = static_cast<int32_t>(SDL_atan2(deltaX, deltaZ) * -325.95) & 0x7ff;

        if (pitch < 128) {
            pitch = 128;
        }

        if (pitch > 383) {
            pitch = 383;
        }

        if (cameraPitch < pitch) {
            cameraPitch += cutsceneRotateSpeed + (((pitch - cameraPitch) * cutsceneRotateAcceleration) / 1000);
            if (cameraPitch > pitch) {
                cameraPitch = pitch;
            }
        }

        if (cameraPitch > pitch) {
            cameraPitch -= cutsceneRotateSpeed + (((cameraPitch - pitch) * cutsceneRotateAcceleration) / 1000);
            if (cameraPitch < pitch) {
                cameraPitch = pitch;
            }
        }

        int32_t deltaYaw = yaw - cameraYaw;

        if (deltaYaw > 1024) {
            deltaYaw -= 2048;
        }

        if (deltaYaw < -1024) {
            deltaYaw += 2048;
        }

        if (deltaYaw > 0) {
            cameraYaw += cutsceneRotateSpeed + ((deltaYaw * cutsceneRotateAcceleration) / 1000);
            cameraYaw &= 0x7ff;
        }

        if (deltaYaw < 0) {
            cameraYaw -= cutsceneRotateSpeed + ((-deltaYaw * cutsceneRotateAcceleration) / 1000);
            cameraYaw &= 0x7ff;
        }

        int32_t tmp = yaw - cameraYaw;

        if (tmp > 1024) {
            tmp -= 2048;
        }

        if (tmp < -1024) {
            tmp += 2048;
        }

        if (((tmp < 0) && (deltaYaw > 0)) || ((tmp > 0) && (deltaYaw < 0))) {
            cameraYaw = yaw;
        }
    }

    int32_t Game::GetTopLevelCutscene()
    {
        int32_t y = GetHeightmapY(currentLevel, cameraX, cameraZ);

        if (((y - cameraY) < 800) && ((levelTileFlags[currentLevel][cameraX >> 7][cameraZ >> 7] & 4) != 0)) {
            return currentLevel;
        } else {
            return 3;
        }
    }

    void Game::RemoveIgnore(int64_t name37)
    {
        if (name37 == 0L) {
            return;
        }
        for (int32_t j = 0; j < ignoreCount; j++) {
            if (ignoreName37[j] == name37) {
                ignoreCount--;
                redrawSidebar = true;
                for (int32_t k = j; k < ignoreCount; k++) {
                    ignoreName37[k] = ignoreName37[k + 1];
                }
                out.WriteOp(74);
                out.Write64(name37);
                return;
            }
        }
    }

    void Game::RemoveFriend(int64_t name37)
    {
        if (name37 == 0L) {
            return;
        }
        for (int32_t i = 0; i < friendCount; i++) {
            if (friendName37[i] != name37) {
                continue;
            }
            friendCount--;
            redrawSidebar = true;
            for (int32_t j = i; j < friendCount; j++) {
                friendName[j] = friendName[j + 1];
                friendWorld[j] = friendWorld[j + 1];
                friendName37[j] = friendName37[j + 1];
            }
            out.WriteOp(215);
            out.Write64(name37);
            break;
        }
    }

    void Game::DrawError()
    {
        Draw2D::Bind(drawSurface);
        Draw2D::Clear();

        static bool errorInitialized = false;
        if (!errorInitialized) {
            errorInitialized = true;
            StopFlames();
        }

        int32_t y = 35;

        auto drawText = [this](const char* text, int32_t x, int32_t y, SDL_Color color) {
            SDL_Surface* surface = TTF_RenderText_Solid(helveticaFont, text, strlen(text), color);
            if (surface) {
                const SDL_Rect dst = { x, y, surface->w, surface->h };
                SDL_BlitSurface(surface, nullptr, drawSurface, &dst);
                SDL_DestroySurface(surface);
            }
        };

        constexpr SDL_Color yellow = { 255, 255, 0 };
        constexpr SDL_Color white = { 255, 255, 255 };

        if (errorLoading) {
            drawText("Sorry, an error has occured whilst loading RuneScape", 30, y, yellow);
            y += 50;
            drawText("To fix this try the following (in order):", 30, y, white);
            y += 50;
            drawText("1: Try closing ALL open web-browser windows, and reloading", 30, y, white);
            y += 30;
            drawText("2: Try clearing your web-browsers cache from tools->internet options", 30, y, white);
            y += 30;
            drawText("3: Try using a different game-world", 30, y, white);
            y += 30;
            drawText("4: Try rebooting your computer", 30, y, white);
            y += 30;
            drawText("5: Try selecting a different version of Java from the play-game menu", 30, y, white);
        }

        if (errorHost) {
            drawText("Error - unable to load game!", 50, 50, white);
            drawText("To play RuneScape make sure you play from", 50, 100, white);
            drawText("http://www.runescape.com", 50, 150, white);
        }

        if (errorStarted) {
            drawText("Error a copy of RuneScape already appears to be loaded", 30, y, yellow);
            y += 50;
            drawText("To fix this try the following (in order):", 30, y, white);
            y += 50;
            drawText("1: Try closing ALL open web-browser windows, and reloading", 30, y, white);
            y += 30;
            drawText("2: Try rebooting your computer, and reloading", 30, y, white);
        }

        PresentFrame();
    }

    void Game::ValidateCharacterDesign()
    {
        updateDesignModel = true;
        for (int32_t part = 0; part < 7; part++) {
            designIdentikits[part] = -1;
            for (int32_t kit = 0; kit < IdkType::count; kit++) {
                if (IdkType::instances[kit].selectable || (IdkType::instances[kit].type != (part + (designGenderMale ? 0 : 7)))) {
                    continue;
                }
                designIdentikits[part] = kit;
                break;
            }
        }
    }

    void Game::DrawParentInterface(IfType& parent, int32_t px, int32_t py, int32_t scrollY)
    {
        if ((parent.type != IfType::TYPE_PARENT) || (parent.childID.empty())) {
            return;
        }

        if (parent.hide && (viewportHoveredInterfaceID != parent.id) && (sidebarHoveredInterfaceID != parent.id) && (chatHoveredInterfaceID != parent.id)) {
            return;
        }

        int32_t left = Draw2D::left;
        int32_t top = Draw2D::top;
        int32_t right = Draw2D::right;
        int32_t bottom = Draw2D::bottom;

        Draw2D::SetBounds(px, py, px + parent.width, py + parent.height);

        for (int32_t i = 0; i < parent.childID.size(); i++) {
            int32_t x = parent.childX[i] + px;
            int32_t y = (parent.childY[i] + py) - scrollY;

            const auto& child = IfType::instances[parent.childID[i]];

            x += child->x;
            y += child->y;

            if (child->contentType > 0) {
                UpdateInterfaceContent(*child);
            }

            if (child->type == IfType::TYPE_PARENT) {
                if (child->scrollPosition > (child->scrollableHeight - child->height)) {
                    child->scrollPosition = child->scrollableHeight - child->height;
                }

                if (child->scrollPosition < 0) {
                    child->scrollPosition = 0;
                }

                DrawParentInterface(*child, x, y, child->scrollPosition);

                if (child->scrollableHeight > child->height) {
                    DrawScrollbar(x + child->width, y, child->height, child->scrollableHeight, child->scrollPosition);
                }
            } else if (child->type == IfType::TYPE_INVENTORY) {
                DrawInterfaceInventory(parent, x, y, *child);
            } else if (child->type == IfType::TYPE_RECT) {
                DrawInterfaceRect(x, y, *child);
            } else if (child->type == IfType::TYPE_TEXT) {
                DrawInterfaceText(x, y, *child);
            } else if (child->type == IfType::TYPE_IMAGE) {
                DrawInterfaceImage(x, y, *child);
            } else if (child->type == IfType::TYPE_MODEL) {
                DrawInterfaceModel(x, y, *child);
            } else if (child->type == IfType::TYPE_INVENTORY_TEXT) {
                DrawInterfaceInventoryText(x, y, *child);
            }
        }
        Draw2D::SetBounds(left, top, right, bottom);
    }


    void Game::UpdateInterfaceContent(IfType& iface)
    {
        int32_t type = iface.contentType;

        if (((type >= 1) && (type <= 100)) || ((type >= 701) && (type <= 800))) {
            if ((type == 1) && (friendlistStatus == 0)) {
                iface.text = "Loading friend list";
                iface.optionType = 0;
                return;
            }

            if ((type == 1) && (friendlistStatus == 1)) {
                iface.text = "Connecting to friendserver";
                iface.optionType = 0;
                return;
            }

            if ((type == 2) && (friendlistStatus != 2)) {
                iface.text = "Please wait...";
                iface.optionType = 0;
                return;
            }

            int32_t count = friendCount;

            if (friendlistStatus != 2) {
                count = 0;
            }

            if (type > 700) {
                type -= 601;
            } else {
                type--;
            }

            if (type >= count) {
                iface.text = "";
                iface.optionType = 0;
            } else {
                iface.text = friendName[type];
                iface.optionType = 1;
            }
            return;
        }

        if (((type >= 101) && (type <= 200)) || ((type >= 801) && (type <= 900))) {
            int32_t count = friendCount;

            if (friendlistStatus != 2) {
                count = 0;
            }

            if (type > 800) {
                type -= 701;
            } else {
                type -= 101;
            }

            if (type >= count) {
                iface.text = "";
                iface.optionType = 0;
                return;
            }

            if (friendWorld[type] == 0) {
                iface.text = "@red@Offline";
            } else if (friendWorld[type] == nodeID) {
                iface.text = "@gre@World-" + std::to_string(friendWorld[type] - 9);
            } else {
                iface.text = "@yel@World-" + std::to_string(friendWorld[type] - 9);
            }

            iface.optionType = 1;
            return;
        }

        if (type == 203) {
            int32_t count = friendCount;

            if (friendlistStatus != 2) {
                count = 0;
            }

            iface.scrollableHeight = (count * 15) + 20;

            if (iface.scrollableHeight <= iface.height) {
                iface.scrollableHeight = iface.height + 1;
            }
            return;
        }

        if ((type >= 401) && (type <= 500)) {
            if ((((type -= 401)) == 0) && (friendlistStatus == 0)) {
                iface.text = "Loading ignore list";
                iface.optionType = 0;
                return;
            }

            if ((type == 1) && (friendlistStatus == 0)) {
                iface.text = "Please wait...";
                iface.optionType = 0;
                return;
            }

            int32_t count = ignoreCount;

            if (friendlistStatus == 0) {
                count = 0;
            }

            if (type >= count) {
                iface.text = "";
                iface.optionType = 0;
            } else {
                iface.text = StringUtil::FormatName(StringUtil::FromBase37(ignoreName37[type]));
                iface.optionType = IfType::OPTION_TYPE_STANDARD;
            }
            return;
        }

        if (type == 503) {
            iface.scrollableHeight = (ignoreCount * 15) + 20;

            if (iface.scrollableHeight <= iface.height) {
                iface.scrollableHeight = iface.height + 1;
            }
            return;
        }

        if (type == 327) {
            iface.modelPitch = 150;
            iface.modelYaw = static_cast<int32_t>(std::sin(static_cast<double>(loopCycle) / 40.0) * 256.0) & 0x7ff;

            if (updateDesignModel) {
                for (int32_t part = 0; part < 7; part++) {
                    int32_t kit = designIdentikits[part];
                    if ((kit >= 0) && !IdkType::instances[kit].ValidateModel()) {
                        return;
                    }
                }

                updateDesignModel = false;

                std::vector<std::shared_ptr<Model>> models(7);
                int32_t modelCount = 0;
                for (int32_t part = 0; part < 7; part++) {
                    int32_t kit = designIdentikits[part];
                    if (kit >= 0) {
                        models[modelCount++] = IdkType::instances[kit].GetModel();
                    }
                }

                auto model = std::make_shared<Model>(modelCount, models);
                for (int32_t part = 0; part < 5; part++) {
                    if (designColors[part] != 0) {
                        model->Recolor(designPartColor[part][0], designPartColor[part][designColors[part]]);
                        if (part == 1) {
                            model->Recolor(designHairColor[0], designHairColor[designColors[part]]);
                        }
                    }
                }

                model->CreateLabelReferences();
                model->ApplyTransform(SeqType::instances[localPlayer->seqStandID].transformIDs[0]);
                model->CalculateNormals(64, 850, -30, -50, -30, true);

                iface.modelType = IfType::MODEL_TYPE_PLAYER_DESIGN;
                iface.modelID = 0;

                IfType::CacheModel(0, IfType::MODEL_TYPE_PLAYER_DESIGN, model);
            }
            return;
        }

        if (type == 324) {
            if (genderButtonImage0 == nullptr) {
                genderButtonImage0 = iface.image;
                genderButtonImage1 = iface.activeImage;
            }
            if (designGenderMale) {
                iface.image = genderButtonImage1;
            } else {
                iface.image = genderButtonImage0;
            }
            return;
        }

        if (type == 325) {
            if (genderButtonImage0 == nullptr) {
                genderButtonImage0 = iface.image;
                genderButtonImage1 = iface.activeImage;
            }
            if (designGenderMale) {
                iface.image = genderButtonImage0;
            } else {
                iface.image = genderButtonImage1;
            }
            return;
        }

        if (type == 600) {
            iface.text = reportAbuseInput;

            if ((loopCycle % 20) < 10) {
                iface.text += "|";
            } else {
                iface.text += " ";
            }
            return;
        }

        if (type == 613) {
            if (rights >= 1) {
                if (reportAbuseMuteOption) {
                    iface.color = 0xff0000;
                    iface.text = "Moderator option: Mute player for 48 hours: <ON>";
                } else {
                    iface.color = 0xffffff;
                    iface.text = "Moderator option: Mute player for 48 hours: <OFF>";
                }
            } else {
                iface.text = "";
            }
        }

        if ((type == 650) || (type == 655)) {
            if (lastAddress != 0) {
                std::string text;
                if (daysSinceLastLogin == 0) {
                    text = "earlier today";
                } else if (daysSinceLastLogin == 1) {
                    text = "yesterday";
                } else {
                    text = std::to_string(daysSinceLastLogin) + " days ago";
                }
                iface.text = "You last logged in " + text + " from: " + Signlink::dns;
            } else {
                iface.text = "";
            }
        }

        if (type == 651) {
            if (unreadMessages == 0) {
                iface.text = "0 unread messages";
                iface.color = 0xffff00;
            }

            if (unreadMessages == 1) {
                iface.text = "1 unread message";
                iface.color = 65280;
            }

            if (unreadMessages > 1) {
                iface.text = std::to_string(unreadMessages) + " unread messages";
                iface.color = 65280;
            }
        }

        if (type == 652) {
            if (daysSinceRecoveriesChanged == 201) {
                if (warnMembersInNonMembers == 1) {
                    iface.text = "@yel@This is a non-members world: @whi@Since you are a member we";
                } else {
                    iface.text = "";
                }
            } else if (daysSinceRecoveriesChanged == 200) {
                iface.text = "You have not yet set any password recovery questions.";
            } else {
                std::string text;

                if (daysSinceRecoveriesChanged == 0) {
                    text = "Earlier today";
                } else if (daysSinceRecoveriesChanged == 1) {
                    text = "Yesterday";
                } else {
                    text = std::to_string(daysSinceRecoveriesChanged) + " days ago";
                }
                iface.text = text + " you changed your recovery questions";
            }
        }

        if (type == 653) {
            if (daysSinceRecoveriesChanged == 201) {
                if (warnMembersInNonMembers == 1) {
                    iface.text = "@whi@recommend you use a members world instead. You may use";
                } else {
                    iface.text = "";
                }
            } else if (daysSinceRecoveriesChanged == 200) {
                iface.text = "We strongly recommend you do so now to secure your account.";
            } else {
                iface.text = "If you do not remember making this change then cancel it immediately";
            }
        }

        if (type == 654) {
            if (daysSinceRecoveriesChanged == 201) {
                if (warnMembersInNonMembers == 1) {
                    iface.text = "@whi@this world but member benefits are unavailable whilst here.";
                } else {
                    iface.text = "";
                }
                return;
            }
            if (daysSinceRecoveriesChanged == 200) {
                iface.text = "Do this from the 'account management' area on our front webpage";
                return;
            }
            iface.text = "Do this from the 'account management' area on our front webpage";
        }
    }

    void Game::DrawInterfaceInventory(IfType& parent, int32_t x, int32_t y, IfType& iface)
    {
        int32_t slot = 0;
        for (int32_t row = 0; row < iface.height; row++) {
            for (int32_t column = 0; column < iface.width; column++) {
                int32_t slotX = x + (column * (32 + iface.inventoryMarginX));
                int32_t slotY = y + (row * (32 + iface.inventoryMarginY));

                if (slot < 20) {
                    slotX += iface.inventorySlotOffsetX[slot];
                    slotY += iface.inventorySlotOffsetY[slot];
                }

                if (iface.inventorySlotObjID[slot] > 0) {
                    int32_t dx = 0;
                    int32_t dy = 0;
                    int32_t objID = iface.inventorySlotObjID[slot] - 1;

                    if (((slotX > (Draw2D::left - 32)) && (slotX < Draw2D::right) && (slotY > (Draw2D::top - 32)) && (slotY < Draw2D::bottom)) || ((objDragArea != 0) && (objDragSlot == slot))) {
                        int32_t outlineColor = 0;

                        if ((objSelected == 1) && (selectedObjSlot == slot) && (selectedObjInterfaceID == iface.id)) {
                            outlineColor = 0xffffff;
                        }

                        auto icon = ObjType::GetIcon(objID, iface.inventorySlotObjCount[slot], outlineColor);

                        if (icon != nullptr) {
                            if ((objDragArea != 0) && (objDragSlot == slot) && (objDragInterfaceID == iface.id)) {
                                dx = mouseX - objGrabX;
                                dy = mouseY - objGrabY;

                                if ((dx < 5) && (dx > -5)) {
                                    dx = 0;
                                }

                                if ((dy < 5) && (dy > -5)) {
                                    dy = 0;
                                }

                                if (objDragCycles < 5) {
                                    dx = 0;
                                    dy = 0;
                                }

                                icon->Draw(slotX + dx, slotY + dy, 128);

                                // scroll component up if dragging obj near the top
                                if (((slotY + dy) < Draw2D::top) && (parent.scrollPosition > 0)) {
                                    int32_t scroll = (delta * (Draw2D::top - slotY - dy)) / 3;

                                    if (scroll > (delta * 10)) {
                                        scroll = delta * 10;
                                    }

                                    if (scroll > parent.scrollPosition) {
                                        scroll = parent.scrollPosition;
                                    }
                                    parent.scrollPosition -= scroll;
                                    objGrabY += scroll;
                                }

                                // scroll component down if dragging obj near the bottom
                                if (((slotY + dy + 32) > Draw2D::bottom) && (parent.scrollPosition < (parent.scrollableHeight - parent.height))) {
                                    int32_t scroll = (delta * ((slotY + dy + 32) - Draw2D::bottom)) / 3;

                                    if (scroll > (delta * 10)) {
                                        scroll = delta * 10;
                                    }

                                    if (scroll > (parent.scrollableHeight - parent.height - parent.scrollPosition)) {
                                        scroll = parent.scrollableHeight - parent.height - parent.scrollPosition;
                                    }
                                    parent.scrollPosition += scroll;
                                    objGrabY -= scroll;
                                }
                            } else if ((actionArea != 0) && (actionSlot == slot) && (actionInterfaceID == iface.id)) {
                                icon->Draw(slotX, slotY, 128);
                            } else {
                                icon->Draw(slotX, slotY);
                            }

                            if ((icon->cropW == 33) || (iface.inventorySlotObjCount[slot] != 1)) {
                                int32_t count = iface.inventorySlotObjCount[slot];
                                fontPlain11->DrawString(FormatObjCount(count), slotX + 1 + dx, slotY + 10 + dy, 0);
                                fontPlain11->DrawString(FormatObjCount(count), slotX + dx, slotY + 9 + dy, 0xffff00);
                            }
                        }
                    }
                } else if (!iface.inventorySlotImage.empty() && (slot < 20)) {
                    auto image = iface.inventorySlotImage[slot];

                    if (image != nullptr) {
                        image->Draw(slotX, slotY);
                    }
                }
                slot++;
            }
        }
    }

    void Game::DrawInterfaceRect(int32_t x, int32_t y, IfType& child)
    {
        bool hovered = (chatHoveredInterfaceID == child.id) || (sidebarHoveredInterfaceID == child.id) || (viewportHoveredInterfaceID == child.id);
        int32_t rgb;

        if (ExecuteInterfaceScript(child)) {
            rgb = child.activeColor;
            if (hovered && (child.activeHoverColor != 0)) {
                rgb = child.activeHoverColor;
            }
        } else {
            rgb = child.color;
            if (hovered && (child.hoverColor != 0)) {
                rgb = child.hoverColor;
            }
        }

        if (child.transparency == 0) {
            if (child.fill) {
                Draw2D::FillRect(x, y, child.width, child.height, rgb);
            } else {
                Draw2D::DrawRect(x, y, child.width, child.height, rgb);
            }
        } else if (child.fill) {
            Draw2D::FillRect(x, y, child.width, child.height, rgb, 256 - (child.transparency & 0xff));
        } else {
            Draw2D::DrawRect(x, y, child.width, child.height, rgb, 256 - (child.transparency & 0xff));
        }
    }

    void Game::DrawInterfaceText(int32_t x, int32_t y, IfType& iface)
    {
        auto font = iface.font;
        std::string text = iface.text;
        bool hovered = (chatHoveredInterfaceID == iface.id) || (sidebarHoveredInterfaceID == iface.id) || (viewportHoveredInterfaceID == iface.id);

        int32_t rgb;
        if (ExecuteInterfaceScript(iface)) {
            rgb = iface.activeColor;

            if (hovered && (iface.activeHoverColor != 0)) {
                rgb = iface.activeHoverColor;
            }

            if (!iface.activeText.empty()) {
                text = iface.activeText;
            }
        } else {
            rgb = iface.color;

            if (hovered && (iface.hoverColor != 0)) {
                rgb = iface.hoverColor;
            }
        }

        if ((iface.optionType == IfType::OPTION_TYPE_CONTINUE) && pressedContinueOption) {
            text = "Please wait...";
            rgb = iface.color;
        }

        if (Draw2D::width == 479) {
            if (rgb == 0xffff00) {
                rgb = 0xFF;
            }
            if (rgb == 0xC000) {
                rgb = 0xffffff;
            }
        }

        for (int32_t lineY = y + font->height; !text.empty(); lineY += font->height) {
            if (text.find('%') != std::string::npos) {
                while (true) {
                    size_t j = text.find("%1");
                    if (j == std::string::npos) {
                        break;
                    }
                    text = text.substr(0, j) + GetIntString(ExecuteClientscript1(iface, 0)) + text.substr(j + 2);
                }
                while (true) {
                    size_t j = text.find("%2");
                    if (j == std::string::npos) {
                        break;
                    }
                    text = text.substr(0, j) + GetIntString(ExecuteClientscript1(iface, 1)) + text.substr(j + 2);
                }
                while (true) {
                    size_t j = text.find("%3");
                    if (j == std::string::npos) {
                        break;
                    }
                    text = text.substr(0, j) + GetIntString(ExecuteClientscript1(iface, 2)) + text.substr(j + 2);
                }
                while (true) {
                    size_t j = text.find("%4");
                    if (j == std::string::npos) {
                        break;
                    }
                    text = text.substr(0, j) + GetIntString(ExecuteClientscript1(iface, 3)) + text.substr(j + 2);
                }
                while (true) {
                    size_t j = text.find("%5");
                    if (j == std::string::npos) {
                        break;
                    }
                    text = text.substr(0, j) + GetIntString(ExecuteClientscript1(iface, 4)) + text.substr(j + 2);
                }
            }

            size_t newline = text.find("\\n");
            std::string split;
            if (newline != std::string::npos) {
                split = text.substr(0, newline);
                text = text.substr(newline + 2);
            } else {
                split = text;
                text = "";
            }

            if (iface.center) {
                font->DrawStringTaggableCenter(split, x + (iface.width / 2), lineY, rgb, iface.shadow);
            } else {
                font->DrawStringTaggable(split, x, lineY, rgb, iface.shadow);
            }
        }
    }

    void Game::DrawInterfaceImage(int32_t x, int32_t y, IfType& iface)
    {
        std::shared_ptr<Image24> image;
        if (ExecuteInterfaceScript(iface)) {
            image = iface.activeImage;
        } else {
            image = iface.image;
        }
        if (image != nullptr) {
            image->Draw(x, y);
        }
    }

    void Game::DrawInterfaceModel(int32_t x, int32_t y, IfType& iface)
    {
        int32_t tmpX = Draw3D::centerX;
        int32_t tmpY = Draw3D::centerY;

        Draw3D::centerX = x + (iface.width / 2);
        Draw3D::centerY = y + (iface.height / 2);

        int32_t eyeY = (Draw3D::sin[iface.modelPitch] * iface.modelZoom) >> 16;
        int32_t eyeZ = (Draw3D::cos[iface.modelPitch] * iface.modelZoom) >> 16;

        bool active = ExecuteInterfaceScript(iface);
        int32_t seqID;

        if (active) {
            seqID = iface.activeSeqID;
        } else {
            seqID = iface.seqID;
        }

        std::shared_ptr<Model> model;

        if (seqID == -1) {
            model = iface.GetModel(-1, -1, active);
        } else {
            SeqType& type = SeqType::instances[seqID];
            model = iface.GetModel(type.transformIDs[iface.seqFrame], type.auxiliaryTransformIDs[iface.seqFrame], active);
        }

        if (model != nullptr) {
            model->DrawSimple(0, iface.modelYaw, 0, iface.modelPitch, 0, eyeY, eyeZ);
        }

        Draw3D::centerX = tmpX;
        Draw3D::centerY = tmpY;
    }

    void Game::DrawInterfaceInventoryText(int32_t x, int32_t y, IfType& iface)
    {
        auto font = iface.font;
        int32_t slot = 0;
        for (int32_t row = 0; row < iface.height; row++) {
            for (int32_t column = 0; column < iface.width; column++) {
                if (iface.inventorySlotObjID[slot] > 0) {
                    const auto& type = ObjType::Get(iface.inventorySlotObjID[slot] - 1);
                    std::string text = type->name;

                    if (type->stackable || (iface.inventorySlotObjCount[slot] != 1)) {
                        text = text + " x" + FormatObjCountTagged(iface.inventorySlotObjCount[slot]);
                    }

                    int32_t textX = x + (column * (115 + iface.inventoryMarginX));
                    int32_t textY = y + (row * (12 + iface.inventoryMarginY));

                    if (iface.center) {
                        font->DrawStringTaggableCenter(text, textX + (iface.width / 2), textY, iface.color, iface.shadow);
                    } else {
                        font->DrawStringTaggable(text, textX, textY, iface.color, iface.shadow);
                    }
                }
                slot++;
            }
        }
    }

    std::string Game::FormatObjCount(int32_t amount)
    {
        if (amount < 100000) {
            return std::to_string(amount);
        }
        if (amount < 10000000) {
            return std::to_string(amount / 1000) + "K";
        } else {
            return std::to_string(amount / 1000000) + "M";
        }
    }

    std::string Game::FormatObjCountTagged(int32_t amount)
    {
        std::string s = std::to_string(amount);
        for (int32_t k = static_cast<int>(s.length()) - 3; k > 0; k -= 3) {
            s = s.substr(0, k) + "," + s.substr(k);
        }
        if (s.length() > 8) {
            s = "@gre@" + s.substr(0, s.length() - 8) + " million @whi@(" + s + ")";
        } else if (s.length() > 4) {
            s = "@cya@" + s.substr(0, s.length() - 4) + "K @whi@(" + s + ")";
        }
        return " " + s;
    }

    std::string Game::GetIntString(int32_t i)
    {
        if (i < 999999999) {
            return std::to_string(i);
        }
        return "*";
    }

    /**
     * Executes ClientScript 1 and returns a state value.
     *
     * @param iface the interface.
     * @return the state.
     */
    bool Game::ExecuteInterfaceScript(const IfType& iface)
    {
        if (iface.scriptComparator.empty()) {
            return false;
        }
        for (size_t i = 0; i < iface.scriptComparator.size(); i++) {
            int32_t value = ExecuteClientscript1(iface, static_cast<int32_t>(i));
            int32_t operand = iface.scriptOperand[i];

            if (iface.scriptComparator[i] == 2) {
                if (value >= operand) {
                    return false;
                }
            } else if (iface.scriptComparator[i] == 3) {
                if (value <= operand) {
                    return false;
                }
            } else if (iface.scriptComparator[i] == 4) {
                if (value == operand) {
                    return false;
                }
            } else if (value != operand) {
                return false;
            }
        }
        return true;
    }

    int32_t Game::ExecuteClientscript1(const IfType& iface, int32_t scriptIndex)
    {
        if (iface.scripts.empty() || scriptIndex >= static_cast<int32_t>(iface.scripts.size())) {
            return -2;
        }

        const auto& script = iface.scripts[scriptIndex];
        if (script.empty()) {
            return -2;
        }

        int32_t acc = 0;
        size_t pos = 0;
        int32_t arith = 0;

        while (pos < script.size()) {
            int32_t code = script[pos++];
            int32_t reg = 0;
            int8_t nextArithmetic = 0;

            if (code == 0) {
                return acc;
            } else if (code == 1) { // load_skill_level {skill}
                reg = skillLevel[script[pos++]];
            } else if (code == 2) { // load_skill_base_level {skill}
                reg = skillBaseLevel[script[pos++]];
            } else if (code == 3) { // load_skill_exp {skill}
                reg = skillExperience[script[pos++]];
            } else if (code == 4) { // load_inv_count {interface id} {obj id}
                auto& inventory = *IfType::instances[script[pos++]];
                int32_t objID = script[pos++];
                if ((objID >= 0) && (objID < ObjType::count) && (!ObjType::Get(objID)->members || members)) {
                    for (size_t slot = 0; slot < inventory.inventorySlotObjID.size(); slot++) {
                        if (inventory.inventorySlotObjID[slot] == (objID + 1)) {
                            reg += inventory.inventorySlotObjCount[slot];
                        }
                    }
                }
            } else if (code == 5) { // load_var {id}
                reg = varps[script[pos++]];
            } else if (code == 6) { // load_next_level_xp {skill}
                reg = levelExperience[skillBaseLevel[script[pos++]] - 1];
            } else if (code == 7) {
                reg = (varps[script[pos++]] * 100) / 46875;
            } else if (code == 8) { // load_combat_level
                reg = localPlayer->combatLevel;
            } else if (code == 9) { // load_total_level
                for (int32_t skill = 0; skill < SKILL_COUNT; skill++) {
                    if (SKILL_ENABLED[skill]) {
                        reg += skillBaseLevel[skill];
                    }
                }
            } else if (code == 10) { // load_inv_contains {interface id} {obj id}
                auto& c = *IfType::instances[script[pos++]];
                int32_t objID = script[pos++] + 1;
                if ((objID >= 0) && (objID < ObjType::count) && (!ObjType::Get(objID)->members || members)) {
                    for (size_t slot = 0; slot < c.inventorySlotObjID.size(); slot++) {
                        if (c.inventorySlotObjID[slot] != objID) {
                            continue;
                        }
                        reg = 999999999;
                        break;
                    }
                }
            } else if (code == 11) { // load_energy
                reg = energy;
            } else if (code == 12) { // load_weight
                reg = weightCarried;
            } else if (code == 13) { // load_bool {varp} {bit: 0..31}
                int32_t varp = varps[script[pos++]];
                int32_t bit = script[pos++];
                reg = ((varp & (1 << bit)) == 0) ? 0 : 1;
            } else if (code == 14) { // load_varbit {varbit}
                const auto& varbit = *VarbitType::instances[script[pos++]];
                int32_t lsb = varbit.lsb;
                reg = (varps[varbit.varp] >> lsb) & BITMASK[varbit.msb - lsb];
            } else if (code == 15) { // sub
                nextArithmetic = 1;
            } else if (code == 16) { // div
                nextArithmetic = 2;
            } else if (code == 17) { // mul
                nextArithmetic = 3;
            } else if (code == 18) { // load_world_x
                reg = (localPlayer->x >> 7) + sceneBaseTileX;
            } else if (code == 19) { // load_world_z
                reg = (localPlayer->z >> 7) + sceneBaseTileZ;
            } else if (code == 20) { // load {value}
                reg = script[pos++];
            }

            if (nextArithmetic == 0) {
                if (arith == 0) {
                    acc += reg;
                }
                if (arith == 1) {
                    acc -= reg;
                }
                if ((arith == 2) && (reg != 0)) {
                    acc /= reg;
                }
                if (arith == 3) {
                    acc *= reg;
                }
                arith = 0;
            } else {
                arith = nextArithmetic;
            }
        }
        return acc;
    }

    void Game::HandleInterfaceInput(const IfType& parent, int32_t x, int32_t y, int32_t scrollPosition)
    {
        int32_t mx = mouseX;
        int32_t my = mouseY;

        if ((parent.type != IfType::TYPE_PARENT) || (parent.childID.empty()) || parent.hide) {
            return;
        }

        if ((mx < x) || (my < y) || (mx > (x + parent.width)) || (my > (y + parent.height))) {
            return;
        }

        int32_t childCount = parent.childID.size();

        for (int32_t i = 0; i < childCount; i++) {
            int32_t childX = parent.childX[i] + x;
            int32_t childZ = (parent.childY[i] + y) - scrollPosition;
            const auto& child = IfType::instances[parent.childID[i]];

            childX += child->x;
            childZ += child->y;

            if (((child->delegateHover >= 0) || (child->hoverColor != 0)) && (mx >= childX) && (my >= childZ) &&
                (mx < (childX + child->width)) && (my < (childZ + child->height))) {
                if (child->delegateHover >= 0) {
                    lastHoveredInterfaceID = child->delegateHover;
                } else {
                    lastHoveredInterfaceID = child->id;
                }
            }

            if (child->type == IfType::TYPE_PARENT) {
                HandleInterfaceInput(*child, childX, childZ, child->scrollPosition);

                if (child->scrollableHeight > child->height) {
                    HandleScrollInput(childX + child->width, child->height, mx, my, *child, childZ, true, child->scrollableHeight);
                }
            } else if (child->type == IfType::TYPE_INVENTORY) {
                HandleInterfaceInventoryInput(childX, childZ, *child);
            } else if ((mx >= childX) && (my >= childZ) && (mx < (childX + child->width)) && (my < (childZ + child->height))) {
                HandleInterfaceOptionInput(*child);
            }
        }
    }

    void Game::HandleScrollInput(int32_t left, int32_t height, int32_t mouseX, int32_t mouseY, IfType& iface,
        int32_t top, bool redraw, int32_t scrollableHeight)
    {
        if (scrollGrabbed) {
            scrollInputPadding = 32;
        } else {
            scrollInputPadding = 0;
        }

        scrollGrabbed = false;

        if ((mouseX >= left) && (mouseX < (left + 16)) && (mouseY >= top) && (mouseY < (top + 16))) {
            iface.scrollPosition -= dragCycles * 4;
            if (redraw) {
                redrawSidebar = true;
            }
        } else if ((mouseX >= left) && (mouseX < (left + 16)) && (mouseY >= ((top + height) - 16)) && (mouseY < (top + height))) {
            iface.scrollPosition += dragCycles * 4;
            if (redraw) {
                redrawSidebar = true;
            }
        } else if ((mouseX >= (left - scrollInputPadding)) && (mouseX < (left + 16 + scrollInputPadding)) && (mouseY >= (top + 16)) && (mouseY < ((top + height) - 16)) && (dragCycles > 0)) {
            int32_t gripSize = ((height - 32) * height) / scrollableHeight;
            if (gripSize < 8) {
                gripSize = 8;
            }
            int32_t gripY = mouseY - top - 16 - (gripSize / 2);
            int32_t maxY = height - 32 - gripSize;
            iface.scrollPosition = ((scrollableHeight - height) * gripY) / maxY;
            if (redraw) {
                redrawSidebar = true;
            }
            scrollGrabbed = true;
        }
    }

    void Game::HandleInterfaceInventoryInput(int32_t x, int32_t y, const IfType& iface)
    {
        int32_t slot = 0;

        for (int32_t row = 0; row < iface.height; row++) {
            for (int32_t col = 0; col < iface.width; col++) {
                int32_t slotX = x + (col * (32 + iface.inventoryMarginX));
                int32_t slotY = y + (row * (32 + iface.inventoryMarginY));

                if (slot < 20) {
                    slotX += iface.inventorySlotOffsetX[slot];
                    slotY += iface.inventorySlotOffsetY[slot];
                }

                if ((mouseX < slotX) || (mouseY < slotY) || (mouseX >= (slotX + 32)) || (mouseY >= (slotY + 32))) {
                    slot++;
                    continue;
                }

                hoveredSlot = slot;
                hoveredSlotParentID = iface.id;

                if (iface.inventorySlotObjID[slot] <= 0) {
                    slot++;
                    continue;
                }

                const auto& obj = ObjType::Get(iface.inventorySlotObjID[slot] - 1);

                if ((objSelected == 1) && iface.inventoryInteractable) {
                    if ((iface.id != selectedObjInterfaceID) || (slot != selectedObjSlot)) {
                        AddMenuOption("Use " + selectedObjName + " with @lre@" + obj->name, 870, slot, iface.id, obj->id);
                    }
                } else if ((spellSelected == 1) && iface.inventoryInteractable) {
                    if ((activeSpellFlags & 0x10) == 0x10) {
                        AddMenuOption(spellCaption + " @lre@" + obj->name, 543, slot, iface.id, obj->id);
                    }
                } else {
                    if (iface.inventoryInteractable) {
                        for (int32_t op = 4; op >= 3; op--) {
                            if ((!obj->inventoryOptions.empty()) && (!obj->inventoryOptions[op].empty())) {
                                AddMenuOption(obj->inventoryOptions[op] + " @lre@" + obj->name, OBJ_IOP_ACTION[op], slot, iface.id, obj->id);
                            } else if (op == 4) {
                                AddMenuOption("Drop @lre@" + obj->name, 847, slot, iface.id, obj->id);
                            }
                        }
                    }

                    if (iface.inventoryUsable) {
                        AddMenuOption("Use @lre@" + obj->name, 447, slot, iface.id, obj->id);
                    }

                    if (iface.inventoryInteractable && (!obj->inventoryOptions.empty())) {
                        for (int32_t op = 2; op >= 0; op--) {
                            if (!obj->inventoryOptions[op].empty()) {
                                AddMenuOption(obj->inventoryOptions[op] + " @lre@" + obj->name, OBJ_IOP_ACTION[op], slot, iface.id, obj->id);
                            }
                        }
                    }

                    if (!iface.inventoryOptions.empty()) {
                        for (int32_t op = 4; op >= 0; op--) {
                            if (!iface.inventoryOptions[op].empty()) {
                                AddMenuOption(iface.inventoryOptions[op] + " @lre@" + obj->name, INV_OP_ACTION[op], slot, iface.id, obj->id);
                            }
                        }
                    }

                    AddMenuOption("Examine @lre@" + obj->name, 1125, slot, iface.id, obj->id);
                }
                slot++;
            }
        }
    }

    void Game::HandleInterfaceOptionInput(IfType& child)
    {
        if (child.optionType == IfType::OPTION_TYPE_STANDARD) {
            bool override = false;

            if (child.contentType != 0) {
                override = HandleSocialMenuOption(child);
            }

            if (!override) {
                AddMenuOption(child.option, 315, 0, child.id, 0);
            }
        } else if (child.optionType == IfType::OPTION_TYPE_SPELL) {
            if (spellSelected == 0) {
                auto prefix = child.spellAction;
                if (prefix.contains(" ")) {
                    prefix = prefix.substr(0, prefix.find(" "));
                }
                AddMenuOption(prefix + " @gre@" + child.spellName, 626, 0, child.id, 0);
            }
        } else if (child.optionType == IfType::OPTION_TYPE_CLOSE) {
            AddMenuOption("Close", 200, 0, child.id, 0);
        } else if (child.optionType == IfType::OPTION_TYPE_TOGGLE) {
            AddMenuOption(child.option, 169, 0, child.id, 0);
        } else if (child.optionType == IfType::OPTION_TYPE_SELECT) {
            AddMenuOption(child.option, 646, 0, child.id, 0);
        } else if (child.optionType == IfType::OPTION_TYPE_CONTINUE) {
            if (!pressedContinueOption) {
                AddMenuOption(child.option, 679, 0, child.id, 0);
            }
        }
    }

    bool Game::HandleSocialMenuOption(const IfType& iface)
    {
        int32_t type = iface.contentType;
        if (((type >= 1) && (type <= 200)) || ((type >= 701) && (type <= 900))) {
            if (type >= 801) {
                type -= 701;
            } else if (type >= 701) {
                type -= 601;
            } else if (type >= 101) {
                type -= 101;
            } else {
                type--;
            }
            AddMenuOption("Remove @whi@" + friendName[type], 792);
            AddMenuOption("Message @whi@" + friendName[type], 639);
            return true;
        }
        if ((type >= 401) && (type <= 500)) {
            AddMenuOption("Remove @whi@" + iface.text, 322);
            return true;
        } else {
            return false;
        }
    }

    void Game::HandleObjDragging()
    {
        if (objDragArea == 0) {
            return;
        }

        objDragCycles++;

        // mouse is greater than 5px from grab point in any direction, trigger treshold
        if ((mouseX > (objGrabX + 5)) || (mouseX < (objGrabX - 5)) || (mouseY > (objGrabY + 5)) || (mouseY < (objGrabY - 5))) {
            objGrabThreshold = true;
        }

        if (mouseButton != 0) {
            return;
        }

        if (objDragArea == 2) {
            redrawSidebar = true;
        }

        if (objDragArea == 3) {
            redrawChatback = true;
        }

        objDragArea = 0;

        // mouse moved at least 5px and have been holding obj for 100ms or longer
        if (objGrabThreshold && (objDragCycles >= 5)) {
            hoveredSlotParentID = -1;
            HandleInput();

            if ((hoveredSlotParentID == objDragInterfaceID) && (hoveredSlot != objDragSlot)) {
                const auto& iface = IfType::instances[objDragInterfaceID];

                // mode 0 = swap
                // mode 1 = insert
                int32_t mode = 0;

                if ((bankArrangeMode == 1) && (iface->contentType == 206)) {
                    mode = 1;
                }

                if (iface->inventorySlotObjID[hoveredSlot] <= 0) {
                    mode = 0;
                }

                if (iface->inventoryMoveReplaces) {
                    int32_t src = objDragSlot;
                    int32_t dst = hoveredSlot;
                    iface->inventorySlotObjID[dst] = iface->inventorySlotObjID[src];
                    iface->inventorySlotObjCount[dst] = iface->inventorySlotObjCount[src];
                    iface->inventorySlotObjID[src] = -1;
                    iface->inventorySlotObjCount[src] = 0;
                } else if (mode == 1) {
                    int32_t src = objDragSlot;
                    for (int32_t dst = hoveredSlot; src != dst; ) {
                        if (src > dst) {
                            iface->InventorySwap(src, src - 1);
                            src--;
                        } else {
                            iface->InventorySwap(src, src + 1);
                            src++;
                        }
                    }
                } else {
                    iface->InventorySwap(objDragSlot, hoveredSlot);
                }
                out.WriteOp(214);
                out.Write16LEA(objDragInterfaceID);
                out.Write8C(mode);
                out.Write16LEA(objDragSlot);
                out.Write16LE(hoveredSlot);
            }
        } else if (((mouseButtonsOption == 1) || IsAddFriendOption(menuSize - 1)) && (menuSize > 2)) {
            ShowContextMenu();
        } else if (menuSize > 0) {
            UseMenuOption(menuSize - 1);
        }
        actionCycles = 10;
        mouseClickButton = 0;
    }

    std::string Game::GetCombatLevelColorTag(int32_t viewerLevel, int32_t otherLevel)
    {
        int32_t diff = viewerLevel - otherLevel;
        if (diff < -9) {
            return "@red@";
        } else if (diff < -6) {
            return "@or3@";
        } else if (diff < -3) {
            return "@or2@";
        } else if (diff < 0) {
            return "@or1@";
        } else if (diff > 9) {
            return "@gre@";
        } else if (diff > 6) {
            return "@gr3@";
        } else if (diff > 3) {
            return "@gr2@";
        } else if (diff > 0) {
            return "@gr1@";
        } else {
            return "@yel@";
        }
    }

    void Game::Logout()
    {
        if (connection) {
            connection->Close();
        }
        ingame = false;
        titleScreenState = 0;
        username = "";
        password = "";
        hideLoginButtons = false;
        ClearCaches();
        scene->Reset();
        for (int32_t i = 0; i < 4; i++) {
            levelCollisionMap[i]->Reset();
        }
        StopMidi();
        nextSong = -1;
        song = -1;
        nextSongDelay = 0;
    }

    void Game::Unload() {
        // Stop threads first before any resource cleanup
        StopFlames();

        // Close network connection
        if (connection) {
            connection->Close();
            connection.reset();
            socket = nullptr;  // Connection destroyed the socket
        } else if (socket) {
            NET_DestroyStreamSocket(socket);
            socket = nullptr;
        }
        if (addr) {
            NET_UnrefAddress(addr);
            addr = nullptr;
        }

        Signlink::Unload();
        // Todo something better?
        for (auto & store : filestores)
        {
            store->Unload();
        }
        StopMidi();
        /*if (mouseRecorder != null) {
            mouseRecorder.active = false;
            mouseRecorder = null;
        }*/
        if (ondemand != nullptr) {
            ondemand->Stop();
            ondemand = nullptr;
        }
        if (audioManager != nullptr) {
            audioManager->Shutdown();
            audioManager = nullptr;
        }
        chatBuffer.Clear();
        out.Clear();
        login.Clear();
        in.Clear();
        sceneMapIndex.clear();
        sceneMapLandData.clear();
        sceneMapLocData.clear();
        sceneMapLandFile.clear();
        sceneMapLocFile.clear();
        levelHeightmap.fill();
        levelTileFlags.fill();
        scene.reset();
        levelCollisionMap.clear();
        bfsDirection.fill();
        bfsCost.fill();
        bfsStepX.clear();
        bfsStepZ.clear();
        textureBuffer.clear();
        areaSidebar.reset();
        areaMapback.reset();
        areaViewport.reset();
        areaChatback.reset();
        areaBackbase1.reset();
        areaBackbase2.reset();
        areaBackhmid1.reset();
        areaBackleft1.reset();
        areaBackleft2.reset();
        areaBackright1.reset();
        areaBackright2.reset();
        areaBacktop1.reset();
        areaBackvmid1.reset();
        areaBackvmid2.reset();
        areaBackvmid3.reset();
        areaBackhmid2.reset();
        imageInvback.reset();
        imageMapback.reset();
        imageChatback.reset();
        imageBackbase1.reset();
        imageBackbase2.reset();
        imageBackhmid1.reset();
        imageSideicons.clear();
        imageRedstone1.reset();
        imageRedstone2.reset();
        imageRedstone3.reset();
        imageRedstone1h.reset();
        imageRedstone2h.reset();
        imageRedstone1v.reset();
        imageRedstone2v.reset();
        imageRedstone3v.reset();
        imageRedstone1hv.reset();
        imageRedstone2hv.reset();
        imageCompass.reset();
        imageHitmarks.clear();
        imageHeadicons.clear();
        imageCrosses.clear();
        imageMapdot0.reset();
        imageMapdot1.reset();
        imageMapdot2.reset();
        imageMapdot3.reset();
        imageMapdot4.reset();
        imageMapscene.clear();
        imageMapfunction.clear();
        tileLastOccupiedCycle.fill();
        players.clear();
        playerIDs.clear();
        entityUpdateIDs.clear();
        playerAppearanceBuffer.clear();
        entityRemovalIDs.clear();
        npcs.clear();
        npcIDs.clear();
        levelObjStacks.fill();
        while (!temporaryLocs.isEmpty()) {
            delete temporaryLocs.pollFront();
        }
        projectiles.clear();
        while (!spotanims.isEmpty()) {
            delete spotanims.pollFront();
        }
        menuParamA.clear();
        menuParamB.clear();
        menuAction.clear();
        menuParamC.clear();
        menuOption.clear();
        varps.clear();
        activeMapFunctionX.clear();
        activeMapFunctionZ.clear();
        activeMapFunctions.clear();
        imageMinimap.reset();
        friendName.clear();
        friendName37.clear();
        friendWorld.clear();
        imageTitle0.reset();
        imageTitle1.reset();
        imageTitle2.reset();
        imageTitle3.reset();
        imageTitle4.reset();
        imageTitle5.reset();
        imageTitle6.reset();
        imageTitle7.reset();
        imageTitle8.reset();
        UnloadTitle();
        LocType::Unload();
        NPCType::Unload();
        ObjType::Unload();
        FloType::instances.clear();
        IdkType::instances.clear();
        IfType::instances.clear();
        SeqType::instances.clear();
        SpotAnimType::instances.clear();
        SpotAnimType::modelCache.clear();
        VarpType::instances.clear();
        PlayerEntity::modelCache.clear();
        Draw3D::Unload();
        Scene::Unload();
        Model::Unload();
        SeqTransform::Unload();

        // Clear static surface pointer to prevent access after SDL cleanup
        Draw2D::surface = nullptr;
    }

    // TODO: call if necessary
    void Game::Refresh()
    {
        redrawTitleBackground = true;
    }

    void Game::DrawProgress(const int32_t percent, const std::string &message) {
        // Store progress locally for game-specific use
        lastProgressPercent = percent;
        lastProgressMessage = message;

        // Update shared progress state (thread-safe) via base class
        GameShell::DrawProgress(percent, message);
    }

    void Game::DrawLoadingProgress() {
        // Wait for title resources (fonts) to be ready before using them
        if (!SDL_GetAtomicInt(&titleResourcesReady)) {
            GameShell::DrawLoadingProgress();
            return;
        }

        // Initialize title screen buffers if not done yet (must happen on main thread)
        LoadTitle();

        if (SDL_GetAtomicInt(&flameActive)) {
            uint64_t now = SDL_GetTicks();
            if (now - lastFlameUpdate >= 20) {
                UpdateFlames();
                lastFlameUpdate = now;
            }
            DrawFlames();
        }

        // Read current progress
        int32_t percent = lastProgressPercent;
        std::string message = lastProgressMessage;

        imageTitle4->Bind();

        constexpr int32_t x = 360;
        constexpr int32_t y = 200;

        constexpr int8_t offsetY = 20;
        fontBold12->DrawStringCenter("RuneScape is loading - please wait...", x / 2, (y / 2) - 26 - offsetY, 0xFFFFFF);
        constexpr int32_t midY = (y / 2) - 18 - offsetY;

        Draw2D::DrawRect((x / 2) - 152, midY, 304, 34, 0x8c1111);
        Draw2D::DrawRect((x / 2) - 151, midY + 1, 302, 32, 0x000000);
        Draw2D::FillRect((x / 2) - 150, midY + 2, percent * 3, 30, 0x8c1111);
        Draw2D::FillRect(((x / 2) - 150) + (percent * 3), midY + 2, 300 - (percent * 3), 30, 0x000000);

        fontBold12->DrawStringCenter(message, x / 2, ((y / 2) + 5) - offsetY, 0xFFFFFF);
        imageTitle4->Draw(drawSurface, 202, 171);

        if (redrawTitleBackground) {
            redrawTitleBackground = false;

            if (!SDL_GetAtomicInt(&flameActive)) {
                imageTitle0->Draw(drawSurface, 0, 0);
                imageTitle1->Draw(drawSurface, 637, 0);
            }

            imageTitle2->Draw(drawSurface, 128, 0);
            imageTitle3->Draw(drawSurface, 202, 371);
            imageTitle5->Draw(drawSurface, 0, 265);
            imageTitle6->Draw(drawSurface, 562, 265);
            imageTitle7->Draw(drawSurface, 128, 171);
            imageTitle8->Draw(drawSurface, 562, 171);
        }

        PresentFrame();
    }

    const std::array<int32_t, 32> Game::BITMASK = [] {
        std::array<int32_t, 32> arr{};
        int32_t acc = 2;
        for (int32_t k = 0; k < 32; ++k) {
            arr[k] = acc - 1;
            acc += acc;
        }
        return arr;
    }();

    const std::array<int32_t, 99> Game::levelExperience = []() {
        std::array<int32_t, 99> arr{};
        int32_t acc = 0;
        for (int32_t i = 0; i < 99; i++) {
            int32_t level = i + 1;
            int32_t delta = static_cast<int32_t>(static_cast<double>(level) + 300.0 * std::pow(2.0, static_cast<double>(level) / 7.0));
            acc += delta;
            arr[i] = acc / 4;
        }
        return arr;
    }();
}