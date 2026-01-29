#pragma once
#include "FileArchive.h"
#include "GameShell.h"
#include "DrawArea.h"
#include "FileStore.h"
#include "BitmapFont.h"
#include "Image24.h"
#include "Image8.h"
#include "OnDemand.h"
#include "Scene.h"
#include "SceneBuilder.h"
#include "Connection.h"
#include "ObjType.h"
#include "ObjEntity.h"
#include "ISAACRandom.h"
#include "PlayerEntity.h"
#include "CollisionMap.h"
#include "SpotAnimEntity.h"
#include "NPCEntity.h"
#include "IfType.h"
#include "AudioManager.h"

namespace SDL_Client {

    enum class ConnectionState
    {
        DISCONNECTED,
        RESOLVING_DNS,
        RESOLVING_SOCKET,
        CONNECTED
    };

    class Game : public GameShell {
        friend class OnDemand;

    public:
        explicit Game(const GameSpecification& specification);
        void Update() override;
        void Draw() override;
        void Load() override;
        void Unload() override;
        void Refresh() override;
    protected:
        void DrawProgress(int32_t percent, const std::string& message) override;
        void DrawLoadingProgress() override;
    private:
        void LoadTitle();
        void LoadTitleImages();
        std::unique_ptr<FileArchive> LoadArchive(int32_t fileId, const std::string& caption,
            const std::string& fileName, int32_t expectedChecksum, int32_t progress);
        void LoadArchiveChecksums();
        void LoadTitleBackground() const;
        void StopFlames();
        void UpdateFlames();
        void DrawFlames();
        void DrawGame();
        void DrawScene();
        void DrawDebug();
        void DrawSidebar();
        void DrawChatback();
        void DrawSideicons();
        void DrawPrivacySettings();
        void DrawMinimap();
        void DrawMinimapFlag() const;
        void DrawOnMinimap(Image24& image, int32_t dx, int32_t dy) const;
        void CreateMinimap(int32_t level);
        void DrawMinimapLoc(int32_t tileZ, int32_t wallRGB, int32_t tileX, int32_t doorRGB, int32_t level) const;
        void DrawMinimapFunctions();
        void DrawMinimapObjs();
        void DrawMinimapNPCs();
        void DrawMinimapPlayers();
        void DrawChat();
        void DrawChats();
        void DrawScrollbar(int32_t x, int32_t y, int32_t height, int32_t scrollHeight, int32_t scrollY) const;
        void Draw2DEntityElements();
        void UpdateSceneState();
        int32_t CheckScene();
        int32_t GetHeightmapY(int32_t level, int32_t sceneX, int32_t sceneZ);

        void ReadRebuildRegion();

        void UpdateGame();
        void UpdateCamera();
        void UpdateNetwork();
        void UpdateOrbitCamera();
        void OrbitCamera(int32_t distance, int32_t pitch, int32_t targetX, int32_t targetY, int32_t yaw, int32_t targetZ);
        void UpdateTitle();
        void PrepareGameScreen();
        void Login(const std::string& usernameInput, const std::string& passwordInput, bool reconnect);
        bool CheckLoginStatus();
        //void Login();
        void UnloadTitle();
        static void ClearCaches();
        void BuildScene();
        void FetchMaps();
        void FetchMapsInstanced();
        void UpdateFlameBuffer(const std::shared_ptr<Image8>& image);
        void DrawTitleScreen(bool hideButtons);
        static int32_t Mix(int32_t src, int32_t dst, int32_t alpha);
        void HandleOnDemandRequests();
        void UseWalkHereOption(int32_t mouseX, int32_t mouseY);
        void HandleMouseInput();
        void HandleMinimapInput();
        void HandleTabInput();
        void HandleChatMouseInput(int32_t mouseY);
        void UseMenuOption(int32_t optionID);
        void SendUseObjOnNPC(int32_t npcID);
        void UseGroundObjOption0(int32_t tileX, int32_t tileZ, int32_t objID);
        void UseGroundObjOption1(int32_t tileX, int32_t tileZ, int32_t objID);
        void UseGroundObjOption2(int32_t tileX, int32_t tileZ, int32_t objID);
        void UseGroundObjOption3(int32_t tileX, int32_t tileZ, int32_t objID);
        void UseGroundObjOption4(int32_t tileX, int32_t tileZ, int32_t objID);
        void UseObjOnPlayer(int32_t playerID);
        void CastSpellOnObj(int32_t slot, int32_t interfaceID, int32_t objID);
        void CastSpellOnGroundObj(int32_t tileX, int32_t tileZ, int32_t objID);
        void CastSpellOnLoc(int32_t tileX, int32_t tileZ, int32_t locBitset);
        void UseObjOnGroundObj(int32_t tileX, int32_t tileZ, int32_t groundObjID);
        void UseObjOnLoc(int32_t tileX, int32_t tileZ, int32_t locBitset);
        void UseButton(int32_t b);
        void SelectSpell(int32_t interfaceID);
        void SelectObj(int32_t slot, int32_t interfaceID, int32_t objID);
        void UsePlayerOption0(int32_t playerID);
        void UsePlayerOption1(int32_t playerID);
        void UsePlayerOption2(int32_t c);
        void UsePlayerOption3(int32_t playerID);
        void UsePlayerOption4(int32_t playerID);
        void UseSelectOption(int32_t interfaceID);
        void UseNPCOption0(int32_t npcID);
        void UseNPCOption1(int32_t npcID);
        void UseNPCOption2(int32_t npcID);
        void UseNPCOption3(int32_t npcID);
        void UseNPCOption4(int32_t npcID);
        void UseObjOption0(int32_t slot, int32_t interfaceID, int32_t objID);
        void UseObjOption1(int32_t slot, int32_t interfaceID, int32_t objID);
        void UseObjOption2(int32_t slot, int32_t interfaceID, int32_t objID);
        void UseObjOption3(int32_t slot, int32_t interfaceID, int32_t objID);
        void UseObjOption4(int32_t slot, int32_t interfaceID, int32_t objID);
        void UseObjOnObj(int32_t slot, int32_t interfaceID, int32_t objID);
        void CastSpellOnNPC(int32_t npcID);
        void CastSpellOnPlayer(int32_t playerID);
        void ExamineNPC(int32_t npcID);
        void ExamineObj(int32_t objID);
        void ExamineInventoryObj(int32_t slot, int32_t interfaceID, int32_t objID);
        void UseLocOption0(int32_t a, int32_t b, int32_t c);
        void UseLocOption1(int32_t a, int32_t b, int32_t c);
        void UseLocOption2(int32_t a, int32_t b, int32_t c);
        void UseLocOption3(int32_t a, int32_t b, int32_t c);
        void UseLocOption4(int32_t tileX, int32_t tileZ, int32_t locBitset);
        void UseInventoryOption0(int32_t slot, int32_t interfaceID, int32_t objID);
        void UseInventoryOption1(int32_t slot, int32_t interfaceID, int32_t objID);
        void UseInventoryOption2(int32_t slot, int32_t interfaceID, int32_t objID);
        void UseInventoryOption3(int32_t slot, int32_t interfaceID, int32_t objID);
        void UseInventoryOption4(int32_t a, int32_t interfaceID, int32_t c);
        void UseToggleOption(int32_t interfaceID);
        bool InteractWithLoc(int32_t bitset, int32_t x, int32_t z);
        void BuildSceneInstanced(SceneBuilder& builder);
        void BuildSceneStandard(SceneBuilder& builder);
        void SortObjStacks(int32_t x, int32_t z);
        void ClearTemporaryLocs();
        void StoreLoc(SceneLocTemporary& loc) const;
        void UpdateTemporaryLocs();
        void UpdateVarp(int32_t varpID);
        void AddLoc(int32_t z, int32_t level, int32_t angle, int32_t kind, int32_t x, int32_t classID, int32_t id);
        void UpdateTextures(int32_t cycle);
        void UpdateTexture(int32_t textureID, int32_t cycle);
        bool IsSceneLocsLoaded();
        void AcceptPlayerRequest(int32_t action, const std::string& playerName);
        void DrawMinimapHint();
        void DrawMinimapHint(Image24& image, int32_t x, int32_t y) const;
        int32_t GetTopLevel();
        void DrawPrivateMessages();
        void DrawMouseCrosses() const;
        void DrawViewportInterfaces();
        void DrawMultizone() const;
        void DrawSystemUpdateTimer() const;
        void AddMenuOption(const std::string& option, int32_t action);
        void AddMenuOption(const std::string& option, int32_t action, int32_t a, int32_t b, int32_t c);
        void AddMessage(int32_t type, const std::string& prefix, const std::string& message);
        void UpdateChatOverride();
        void DrawMenu() const;
        void ShowContextMenu();
        void HandleMenuInput(int32_t button);
        void HandleInput();
        void HandleInputKey();
        void HandleInputReportAbuseKey(int32_t key);
        void HandleInputSocialKey(int32_t key);
        void HandleInputAmountKey(int32_t key);
        void HandleInputNameKey(int32_t key);
        void HandleInputChatKey(int32_t key);
        void HandleChatSettingsInput();
        void SortMenuOptions();
        void DrawTooltip();
        void HandlePrivateChatInput();
        void HandleViewportInput();
        void HandleSidebarInput();
        void HandleChatInput();
        void HandleViewportOptions();
        bool HandleInterfaceAction(IfType& iface);
        void HandleLocOptions(int32_t bitset, int32_t x, int32_t z, int32_t id);
        void HandleNPCOptions(int32_t x, int32_t z, int32_t d);
        void HandlePlayerOptions(int32_t x, int32_t z, int32_t id);
        void HandleObjStackOptions(int32_t x, int32_t z);
        void AddNPCOptions(std::shared_ptr<NPCType> type, int32_t npcID, int32_t tileZ, int32_t tileX);
        void AddPlayerOptions(int32_t tileX, int32_t playerID, PlayerEntity& player, int32_t tileZ);
        void ExamineLoc(int32_t bitset);
        bool TryMove(int32_t type, int32_t srcX, int32_t srcZ, int32_t dx, int32_t dz, int32_t locType, int32_t locWidth, int32_t locLength, int32_t locAngle, int32_t locInteractionFlags, bool tryNearest);
        std::unique_ptr<Connection> OpenURL(const std::string& s);
        void TryReconnect();
        bool Read();
        void ReadSyncPlayers();
        void ReadLastLoginInfo();
        void ReadLocalPlayer();
        void ReadIfSetPlayerHead();
        void ReadPlayers();
        void ReadNewPlayers();
        void ReadPlayerUpdates();
        void ReadPlayerForceMovement(PlayerEntity& player);
        void ReadPlayerGraphic(PlayerEntity& player);
        void ReadPlayerAnimation(PlayerEntity& player);
        void ReadPlayerChatForced(PlayerEntity& player);
        void ReadPlayerChat(PlayerEntity& player);
        void ReadPlayerAppearance(int32_t playerID, PlayerEntity& player);
        void ReadPlayerTargetTile(PlayerEntity& player);
        void ReadPlayerTargetEntity(PlayerEntity& player);
        void ReadPlayerDamage0(PlayerEntity& player);
        void ReadPlayerDamage1(PlayerEntity& player);
        void ReadChatFilterSettings();
        void ReadUpdateRunWeight();
        void ReadIfSetModel();
        void ReadIfSetColor();
        void ReadUpdateInvFull();
        void ReadIfSetAngle();
        void ReadMessageGame();
        void ReadSetPlayerOp();
        void ReadZoneClear();
        void ReadIgnoreList();
        void ReadFriendStatus();
        void ReadUpdateRunEnergy();
        void ReadIfTab();
        void ReadIfSetPosition();
        void ReadIfViewportOverlay();
        void ReadIfSetNPCHead();
        void ReadMessagePublic();
        void ReadTabHint();
        void ReadIfSetObject();
        void ReadIfSetHide();
        void ReadIfStopAnim();
        void ReadIfSetText();
        void ReadUpdateInvPartial();
        void ReadTabSelected();
        void ReadZonePacket(int32_t code);
        void ReadObjAdd();
        void ReadObjReveal();
        void ReadObjCount();
        void ReadObjDelete();
        void ReadLocAdd();
        void ReadLocChange();
        void ReadLocDelete();
        void ReadLocPlayer();
        void ReadZoneUpdate();
        void ReadMapAnim();
        void ReadMapProjectile();
        void ReadCameraReset();
        void ReadInventoryClear();
        void ReadCameraSetPos();
        void ReadCameraLookAt();
        void ReadUpdateStat();
        void ReadCameraShake();
        void ReadSyncNPCs();
        void OpenChatInput(int32_t type);
        void ReadNPCs();
        void ReadNewNPCs();
        void ReadNPCUpdates();
        void ReadNPCAnimation(NPCEntity& npc);
        void ReadNPCDamage0(NPCEntity& npc);
        void ReadNPCGraphic(NPCEntity& npc);
        void ReadNPCTargetEntity(NPCEntity& npc);
        void ReadNPCChat(NPCEntity& npc);
        void ReadNPCDamage1(NPCEntity& npc);
        void ReadNPCTransform(NPCEntity& npc);
        void ReadNPCTargetTile(NPCEntity& npc);
        void ReadViewportInterface();
        void ReadVarpLarge();
        void ReadVarpSmall();
        void ReadIfSetScrollPos();
        void RestoreVarCache();
        void DrawTileHint();
        void DrawHealth(const std::shared_ptr<PathingEntity>& entity);
        void DrawHitmarks(const std::shared_ptr<PathingEntity>& entity);
        void ReadHintArrow();
        void ReadIfViewportAndSidebar();
        void PushPlayers(bool local);
        void UpdatePlayers();
        void UpdateNPCs();
        void PushNPCs(bool important);
        void UpdateIdleCycles();
        void UpdateEntityChats();
        void UpdateEntity(const std::shared_ptr<PathingEntity>& entity);
        void UpdateSequences(PathingEntity& e) const;
        void UpdateMovement(PathingEntity& entity);
        bool UpdateInterfaceAnimation(int32_t delta, int32_t id);
        void UpdateFacingDirection(PathingEntity& e) const;
        void UpdateForceMovement(PathingEntity& entity) const;
        void StartForceMovement(PathingEntity& entity) const;
        void ResetAnimations();
        void AppendLoc(int32_t duration, int32_t id, int32_t rotation,
            int32_t classID, int32_t z, int32_t kind, int32_t level, int32_t x, int32_t delay);
        void ShiftScene();
        void ApplyCameraAdjustments();
        void ProjectFromGround(const std::shared_ptr<PathingEntity>& entity, int32_t height);
        void ProjectFromGround(int32_t x, int32_t height, int32_t z);
        void Project(int32_t x, int32_t y, int32_t z);
        bool IsFriend(const std::string& name) const;
        void ClearTileFlags();
        bool IsAddFriendOption(int32_t option);
        void AddFriend(int64_t name37);
        void AddIgnore(int64_t name37);
        void PushProjectiles();
        void PushSpotanims();
        void ApplyCutscene();
        int32_t GetTopLevelCutscene();
        void RemoveIgnore(int64_t name37);
        void RemoveFriend(int64_t name37);
        void DrawError();
        void ValidateCharacterDesign();
        void DrawParentInterface(IfType& parent, int32_t px, int32_t py, int32_t scrollY);
        void HandleInterfaceInput(const IfType& parent, int32_t x, int32_t y, int32_t scrollPosition);
        void HandleScrollInput(int32_t left, int32_t height, int32_t mouseX, int32_t mouseY,
            IfType& iface, int32_t top, bool redraw, int32_t scrollableHeight);
        void HandleInterfaceInventoryInput(int32_t x, int32_t y, const IfType& iface);
        void HandleInterfaceOptionInput(IfType& child);
        bool HandleSocialMenuOption(const IfType& iface);
        void HandleObjDragging();
        void UpdateInterfaceContent(IfType& iface);
        void DrawInterfaceInventory(IfType& parent, int32_t x, int32_t y, IfType& iface);
        void DrawInterfaceRect(int32_t x, int32_t y, IfType& child);
        void DrawInterfaceText(int32_t x, int32_t y, IfType& iface);
        void DrawInterfaceImage(int32_t x, int32_t y, IfType& iface);
        void DrawInterfaceModel(int32_t x, int32_t y, IfType& iface);
        void DrawInterfaceInventoryText(int32_t x, int32_t y, IfType& iface);
        void UseReportAbuseOption(const std::string& input);
        void CloseInterfaces();
        void ReadIfSetAnim();
        void ReadIfChat();
        void OpenViewportInterface(int32_t interfaceID);
        void ResetInterfaceAnimation(int32_t interfaceID);
        void PromptMessageFriend(int64_t name37);

        void ReadSynthSound();
        void SetWaveVolume(int32_t volume);
        void MidiVol(bool enabled, int32_t volumeMillibels);
        void ReadMidiSong();
        void ReadMidiJingle();
        void ReadMapSound();
        void PlayMidi(bool fade, std::vector<int8_t>& data);
        void UpdateAudio();
        void StopMidi();

        void Debug();

        std::string FormatObjCount(int32_t amount);
        std::string FormatObjCountTagged(int32_t amount);
        std::string GetIntString(int32_t i);
        bool ExecuteInterfaceScript(const IfType& iface);
        int32_t ExecuteClientscript1(const IfType& iface, int32_t scriptIndex);
        static std::string GetCombatLevelColorTag(int32_t viewerLevel, int32_t otherLevel);
        void Logout();
    public:
        inline static std::string server;
        inline static int32_t port;
        inline static bool enableRSA;
        std::shared_ptr<FileStore> filestores[5];
        static const std::array<int32_t, 32> BITMASK;
        std::vector<int32_t> varps =  std::vector<int32_t>(2000);
        inline static std::shared_ptr<PlayerEntity> localPlayer = nullptr;
        int32_t rights = 0;
        bool flagged = false;
        inline static bool members = true;
        inline static NET_StreamSocket* socket = nullptr;
        inline static int32_t loopCycle = 0;
        inline static const std::vector<std::vector<int32_t>> designPartColor = {
            {6798, 107, 10283, 16, 4797, 7744, 5799, 4634, 33697, 22433, 2983, 54193},
            {8741, 12, 64030, 43162, 7735, 8404, 1701, 38430, 24094, 10153, 56621, 4783, 1341, 16578, 35003, 25239},
            {25238, 8742, 12, 64030, 43162, 7735, 8404, 1701, 38430, 24094, 10153, 56621, 4783, 1341, 16578, 35003},
            {4626, 11146, 6439, 12, 4758, 10270},
            {4550, 4537, 5681, 5673, 5790, 6806, 8076, 4574}
        };
        inline static const std::array<int32_t, 16> designHairColor = {9104, 10275, 7595, 3610, 7975, 8526, 918, 38802, 24466, 10145, 58654, 5027, 1457, 16565, 34991, 25486};
        inline static const std::array<int32_t, 23> LOC_KIND_TO_CLASS_ID = {0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3};
    private:
        /**
         * Set to <code>true</code> when the change region packet is received, and set to <code>false</code> when the player
         * sync packet is received. Used by {@link #checkScene()}.
         */
        bool awaitingSync = false;
            /**
         * No effect when set to <code>0</code>, otherwise block chats.
         */
        int32_t nodeID = 0;

        bool jaggrabEnabled = false;
        NET_StreamSocket* jaggrabSocket = nullptr;

        /**
        * This message is set when {@link #stickyChatInterfaceID} is not <code>-1</code> and a type <code>0</code> (system) message is received.
        *
        * @see #addMessage(int, String, String)
        */
        std::string modalMessage;
        int32_t chatInterfaceID = -1;
        bool showSocialInput = false;
        std::string socialMessage;
        std::string socialInput;
        int32_t chatbackInputType = 0;
        std::string chatbackInput;
        /**
         * The sticky chatback interface, which can only be opened/closed by the server via packet 218.
         */
        int32_t stickyChatInterfaceID = -1;

        /**
         * Not an actual component, just a hacky fix (by Andy Grower) to use the same method to handle scrolling.
         *
         * @see #handleScrollInput(int, int, int, int, IfType, int, boolean, int)
         */
        IfType chatInterface = IfType();

        int32_t overrideChat = 0;
        int32_t entityRemovalCount = 0;
        int32_t entityUpdateCount = 0;
        int32_t playerCount = 0;
        int32_t localPID = -1;
        int32_t scenePrevBaseTileX = 0;
        int32_t scenePrevBaseTileZ = 0;
        int32_t isMember = false;
        int32_t npcCount = 0;
        std::vector<std::shared_ptr<PlayerEntity>> players = std::vector<std::shared_ptr<PlayerEntity>>(MAX_PLAYER_COUNT);
        std::vector<int32_t> entityUpdateIDs = std::vector<int32_t>(MAX_PLAYER_COUNT);
        std::vector<int32_t> playerIDs = std::vector<int32_t>(MAX_PLAYER_COUNT);
        std::vector<Buffer> playerAppearanceBuffer = std::vector<Buffer>(MAX_PLAYER_COUNT);
        std::vector<int32_t> entityRemovalIDs = std::vector<int32_t>(1000);

        std::vector<std::shared_ptr<NPCEntity>> npcs = std::vector<std::shared_ptr<NPCEntity>>(16384);
        std::vector<int32_t> npcIDs = std::vector<int32_t>(16384);

        std::vector<std::shared_ptr<CollisionMap>> levelCollisionMap = { nullptr, nullptr, nullptr, nullptr };

        Array2DIndexed<int32_t> bfsDirection = Array2DIndexed<int32_t>(104, 104);
        Array2DIndexed<int32_t> bfsCost = Array2DIndexed<int32_t>(104, 104);
        std::vector<int32_t> bfsStepX = std::vector<int32_t>(4000);
        std::vector<int32_t> bfsStepZ = std::vector<int32_t>(4000);
        int32_t tryMoveNearest = 0;
        int32_t flagSceneTileX = 0;
        int32_t flagSceneTileZ = 0;

        int32_t loginAttempts = 0;

        int32_t bankArrangeMode = 0;
        bool objGrabThreshold = false;

#ifdef __EMSCRIPTEN__
        int32_t portOffset = 2;
#else
        int32_t portOffset = 0;
#endif
        int64_t serverSeed = 0;

        bool loadArchiveChecksums = false;

        Buffer out = Buffer(5000);
        Buffer in = Buffer(5000);
        Buffer login = Buffer(1024);
        Buffer chatBuffer{std::vector<int8_t>(5000)};

        std::vector<std::string> menuOption = std::vector<std::string>(500);
        std::vector<int32_t> menuParamA = std::vector<int32_t>(500);
        std::vector<int32_t> menuParamB = std::vector<int32_t>(500);
        std::vector<int32_t> menuParamC = std::vector<int32_t>(500);
        std::vector<int32_t> menuAction = std::vector<int32_t>(500);

        int32_t menuX = 0;
        int32_t menuY = 0;
        int32_t menuWidth = 0;
        int32_t menuHeight = 0;
        int32_t menuSize = 0;

        /**
         * Used for adding/removing friends/ignores and sending private messages.
         */
        int32_t socialAction = 0;

        int64_t inputFriendName37 = 0;

        bool scrollGrabbed = false;
        /**
         * The max allowable distance away from the sides of a currently grabbed scrollbar grip before stopping input.
         */
        int32_t scrollInputPadding = 0;
        int32_t dragCycles = 0;

        int32_t lastHoveredInterfaceID = 0;

        int32_t hintNPC = 0;

        int32_t viewportOverlayInterfaceID = 0;

        /**
         * The current container slot id the mouse is hovered over that belongs to {@link #hoveredSlotParentID}.
         */
        int32_t hoveredSlot = 0;
        /**
         * The current component id the mouse is hovered over that the {@link #hoveredSlot} belongs to.
         */
        int32_t hoveredSlotParentID = 0;

        int32_t actionCycles = 0;

        int32_t minimapZoom = 0;
        int32_t minimapAnticheatAngle = 0;
        int32_t minimapLevel = -1;
        int32_t activeMapFunctionCount = 0;
        std::unique_ptr<Image24> imageMinimap;
        std::vector<int32_t> minimapMaskLineLengths = std::vector<int32_t>(151);
        std::vector<int32_t> minimapMaskLineOffsets = std::vector<int32_t>(151);
        std::vector<std::unique_ptr<Image24>> activeMapFunctions = std::vector<std::unique_ptr<Image24>>(1000);

        /**
         * Tells the client to disconnect instead of attempting to reestablish connection during a {@link #tryReconnect()}.
         * This is typically set to 250 (5 seconds) after {@link #idleCycles} has reached 4500 (90 seconds).
         *
         * @see #updateIdleCycles()
         * @see #tryReconnect()
         */
        int32_t idleTimeout = 0;
        int32_t systemUpdateTimer = 0;
        uint64_t sceneLoadStartTime = 0;
        int32_t heartbeatTimer = 0;
        int32_t bytesOut = 0;

        bool errorHost = false;
        bool errorLoading = false;
        bool errorStarted = false;

        bool withinTutorialIsland = false;
        bool sceneInstanced = false;

        bool menuVisible = false;
        int32_t menuArea = 0;

        int32_t spellSelected = 0;
        std::string selectedObjName;
        int32_t activeSpellID = 0;
        int32_t activeSpellFlags = 0;
        std::string spellCaption;

        int32_t crossX = 0;
        int32_t crossY = 0;
        int32_t crossMode = 0;
        int32_t crossCycle = 0;
        int32_t delta = 0;
        int32_t sceneCycle = 0;
        bool lowmem = false;
        bool ingame = false;

        // for server sided packets
        int32_t baseX = 0;
        int32_t baseZ = 0;

        int32_t publicChatSetting = 0;
        int32_t privateChatSetting = 0;
        int32_t tradeChatSetting = 0;
        int32_t chatCount = 0;
        int32_t friendCount = 0;
        int32_t friendlistStatus = 0;
        int32_t ignoreCount = 0;
        int32_t messageCounter = 0;
        std::vector<std::string> friendName = std::vector<std::string>(200);
        std::vector<int64_t> friendName37 = std::vector<int64_t>(200);
        std::vector<int32_t> friendWorld = std::vector<int32_t>(200);
        std::vector<int64_t> ignoreName37 = std::vector<int64_t>(100);
        std::vector<int32_t>messageIDs = std::vector<int32_t>(100);

        Array2DIndexed<int32_t> tileLastOccupiedCycle =  Array2DIndexed<int32_t>(104, 104);

        bool redrawSidebar = false;
        bool redrawChatback = false;
        bool redrawTitleBackground = false;
        bool redrawSideicons = false;
        bool redrawPrivacySettings = false;

        bool updateDesignModel = false;
        bool designGenderMale = true;
        std::array<int32_t, 7> designIdentikits{};
        std::array<int32_t, 5> designColors{};

        std::shared_ptr<Image24> genderButtonImage0;
        std::shared_ptr<Image24> genderButtonImage1;

        std::string reportAbuseInput;
        bool reportAbuseMuteOption = false;

        int32_t lastAddress = 0;
        int32_t daysSinceLastLogin = 0;
        int32_t unreadMessages = 0;
        int32_t daysSinceRecoveriesChanged = 0;
        int32_t warnMembersInNonMembers = 0;

        int32_t viewportHoveredInterfaceID = 0;
        int32_t sidebarHoveredInterfaceID = 0;
        int32_t chatHoveredInterfaceID = 0;

        int32_t objDragArea = 0;
        int32_t objDragSlot = 0;
        int32_t objDragInterfaceID = 0;
        int32_t objGrabX = 0;
        int32_t objGrabY = 0;
        int32_t objDragCycles = 0;

        int32_t objSelected = 0;
        int32_t selectedObjSlot = 0;
        int32_t selectedObjInterfaceID = 0;

        int32_t actionArea = 0;
        int32_t actionSlot = 0;
        int32_t actionInterfaceID = 0;

        bool pressedContinueOption = false;

        static constexpr int32_t MAX_CHATS = 50;
        std::vector<int32_t> CHAT_COLORS = {0xffff00, 0xff0000, 65280, 65535, 0xff00ff, 0xffffff};

        std::array<int32_t, 25> skillLevel{};
        std::array<int32_t, 25> skillBaseLevel{};
        std::array<int32_t, 25> skillExperience{};
        static const std::array<int32_t, 99> levelExperience;

        int32_t energy = 0;
        int32_t weightCarried = 0;
        static constexpr int32_t SKILL_COUNT = 25;
        static constexpr bool SKILL_ENABLED[25] = {true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false, true, false, false, false, false};


        std::vector<int32_t> chatX = std::vector<int32_t>(MAX_CHATS);
        std::vector<int32_t> chatY = std::vector<int32_t>(MAX_CHATS);
        std::vector<int32_t> chatHeight = std::vector<int32_t>(MAX_CHATS);
        std::vector<int32_t> chatWidth = std::vector<int32_t>(MAX_CHATS);
        std::vector<int32_t> chatColors = std::vector<int32_t>(MAX_CHATS);
        std::vector<int32_t> chatStyles = std::vector<int32_t>(MAX_CHATS);
        std::vector<int32_t> chatTimers = std::vector<int32_t>(MAX_CHATS);
        std::vector<std::string> chats = std::vector<std::string>(MAX_CHATS);

        int32_t projectX = -1;
        int32_t projectY = -1;
        int32_t chatEffects = 0;
        int32_t splitPrivateChat = 0;

        const std::string VALID_CHAT_CHARACTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!\"\243$%^&*()-_=+[{]};:'@#~,<.>/?\\| ";

        int32_t lastProgressPercent = 0;
        std::string lastProgressMessage;

        int32_t currentLevel = 0;

        int32_t viewportWidth;
        int32_t viewportHeight;

        std::unique_ptr<FileArchive> archiveTitle;
        std::unique_ptr<FileArchive> archiveConfig;
        std::unique_ptr<FileArchive> archiveInterface;
        std::unique_ptr<FileArchive> archiveMedia;
        std::shared_ptr<FileArchive> archiveTextures;
        std::unique_ptr<FileArchive> archiveWordenc;
        std::unique_ptr<FileArchive> archiveSounds;
        std::unique_ptr<FileArchive> archiveVersionlist;

        std::unique_ptr<DrawArea> areaSidebar;
        std::unique_ptr<DrawArea> areaMapback;
        std::unique_ptr<DrawArea> areaViewport;
        std::unique_ptr<DrawArea> areaChatback;
        std::unique_ptr<DrawArea> areaBackbase1;
        std::unique_ptr<DrawArea> areaBackbase2;
        std::unique_ptr<DrawArea> areaBackhmid1;

        std::unique_ptr<DrawArea> areaBackleft1;
        std::unique_ptr<DrawArea> areaBackleft2;
        std::unique_ptr<DrawArea> areaBackright1;
        std::unique_ptr<DrawArea> areaBackright2;
        std::unique_ptr<DrawArea> areaBacktop1;
        std::unique_ptr<DrawArea> areaBackvmid1;
        std::unique_ptr<DrawArea> areaBackvmid2;
        std::unique_ptr<DrawArea> areaBackvmid3;
        std::unique_ptr<DrawArea> areaBackhmid2;

        std::unique_ptr<DrawArea> imageTitle0;
        std::unique_ptr<DrawArea> imageTitle1;
        std::unique_ptr<DrawArea> imageTitle2;
        std::unique_ptr<DrawArea> imageTitle3;
        std::unique_ptr<DrawArea> imageTitle4;
        std::unique_ptr<DrawArea> imageTitle5;
        std::unique_ptr<DrawArea> imageTitle6;
        std::unique_ptr<DrawArea> imageTitle7;
        std::unique_ptr<DrawArea> imageTitle8;

        std::shared_ptr<BitmapFont> fontPlain11;
        std::shared_ptr<BitmapFont> fontPlain12;
        std::shared_ptr<BitmapFont> fontBold12;
        std::shared_ptr<BitmapFont> fontQuill8;

        bool started = false;
        SDL_AtomicInt titleResourcesReady{0};  // Set to 1 when archiveTitle and fonts are loaded
        SDL_AtomicInt flameActive{0};
        uint64_t lastFlameUpdate = 0;

        bool connecting = false;

        /**
         * When <code>true</code> will cause the camera to be overridden by the following cutscene fields.
         *
         * @see #applyCutscene()
         * @see #cutsceneSrcLocalTileX
         * @see #cutsceneSrcLocalTileZ
         * @see #cutsceneSrcHeight
         * @see #cutsceneMoveSpeed
         * @see #cutsceneMoveAcceleration
         * @see #cutsceneDstLocalTileX
         * @see #cutsceneDstLocalTileZ
         * @see #cutsceneDstHeight
         * @see #cutsceneRotateSpeed
         * @see #cutsceneRotateAcceleration
         */
        bool cutscene = false;
        /**
         * @see #cutscene
         */
        int32_t cutsceneSrcLocalTileX;
        /**
         * @see #cutscene
         */
        int32_t cutsceneSrcLocalTileZ;
        /**
         * @see #cutscene
         */
        int32_t cutsceneSrcHeight;
        /**
         * @see #cutscene
         */
        int32_t cutsceneMoveSpeed;
        /**
         * @see #cutscene
         */
        int32_t cutsceneMoveAcceleration;
        /**
         * @see #cutscene
         */
        int32_t cutsceneDstLocalTileX;
        /**
         * @see #cutscene
         */
        int32_t cutsceneDstLocalTileZ;
        /**
         * @see #cutscene
         */
        int32_t cutsceneDstHeight;
        /**
         * @see #cutscene
         */
        int32_t cutsceneRotateSpeed;
        /**
         * @see #cutscene
         */
        int32_t cutsceneRotateAcceleration;

        /**
         * The active viewport interface id which is affected by input.
         */
        int32_t viewportInterfaceID = -1;

        std::unique_ptr<Image8> imageTitlebox;
        std::unique_ptr<Image8> imageTitlebutton;

        std::shared_ptr<Image8> imageRunes[12];

        std::unique_ptr<Image8> imageInvback;
        std::unique_ptr<Image8> imageMapback;
        std::unique_ptr<Image8> imageChatback;
        std::unique_ptr<Image8> imageBackhmid1;
        std::unique_ptr<Image8> imageBackbase1;
        std::unique_ptr<Image8> imageBackbase2;
        std::unique_ptr<Image8> imageScrollbar0;
        std::unique_ptr<Image8> imageScrollbar1;

        std::unique_ptr<Image8> imageRedstone1;
        std::unique_ptr<Image8> imageRedstone2;
        std::unique_ptr<Image8> imageRedstone3;
        std::unique_ptr<Image8> imageRedstone1h;
        std::unique_ptr<Image8> imageRedstone2h;

        std::unique_ptr<Image8> imageRedstone1v;
        std::unique_ptr<Image8> imageRedstone2v;
        std::unique_ptr<Image8> imageRedstone3v;
        std::unique_ptr<Image8> imageRedstone1hv;
        std::unique_ptr<Image8> imageRedstone2hv;
        std::vector<std::unique_ptr<Image8>> imageSideicons = std::vector<std::unique_ptr<Image8>>(13);
        std::vector<std::unique_ptr<Image8>> imageMapscene = std::vector<std::unique_ptr<Image8>>(100);
        std::vector<std::unique_ptr<Image24>> imageMapfunction = std::vector<std::unique_ptr<Image24>>(100);
        std::vector<std::unique_ptr<Image24>> imageHitmarks = std::vector<std::unique_ptr<Image24>>(20);
        std::vector<std::unique_ptr<Image24>> imageHeadicons = std::vector<std::unique_ptr<Image24>>(20);

        std::vector<int32_t> activeMapFunctionX = std::vector<int32_t>(1000);
        std::vector<int32_t> activeMapFunctionZ = std::vector<int32_t>(1000);

        std::unique_ptr<Image24> imageMapmarker0;
        std::unique_ptr<Image24> imageMapmarker1;

        std::unique_ptr<Image24> imageMapdot0;
        std::unique_ptr<Image24> imageMapdot1;
        std::unique_ptr<Image24> imageMapdot2;
        std::unique_ptr<Image24> imageMapdot3;
        std::unique_ptr<Image24> imageMapdot4;

        std::vector<std::unique_ptr<Image8>> imageModIcons = std::vector<std::unique_ptr<Image8>>(2);

        std::unique_ptr<Image24> imageCompass;
        std::unique_ptr<Image24> imageMapedge;

        std::unique_ptr<Image24> imageFlamesLeft;
        std::unique_ptr<Image24> imageFlamesRight;

        std::vector<std::unique_ptr<Image24>> imageCrosses = std::vector<std::unique_ptr<Image24>>(8);

        std::vector<std::string> messageSender = std::vector<std::string>(100);
        std::vector<std::string> messageText = std::vector<std::string>(100);
        std::vector<int32_t> messageType = std::vector<int32_t>(100);
        int32_t chatScrollOffset = 0;
        int32_t chatScrollHeight = 78;
        std::string chatTyped;

        std::unique_ptr<OnDemand> ondemand;

        std::unique_ptr<Scene> scene;

        std::unique_ptr<AudioManager> audioManager;

        // Sound effect management
        static constexpr int32_t MAX_WAVES = 50;
        int32_t waveCount = 0;
        std::vector<int32_t> waveIDs = std::vector<int32_t>(MAX_WAVES);
        std::vector<int32_t> waveLoops = std::vector<int32_t>(MAX_WAVES);
        std::vector<int32_t> waveDelay = std::vector<int32_t>(MAX_WAVES);
        bool waveEnabled = true;

        // MIDI/Music management
        int32_t song = -1;
        int32_t nextSong = -1;
        int32_t nextSongDelay = 0;
        bool songFading = false;
        bool midiEnabled = true;

        bool showOccluders = false;
        bool showPerformance = false;
        bool showTraffic = false;

        std::array<int32_t, 256> flameGradient0{};
        std::array<int32_t, 256> flameGradient1{};
        std::array<int32_t, 256> flameGradient2{};

        std::array<int32_t, 256> flameGradient{};
        std::array<int32_t, 32768> flameBuffer0{};
        std::array<int32_t, 32768> flameBuffer1{};
        std::array<int32_t, 32768> flameBuffer2{};
        std::array<int32_t, 32768> flameBuffer3{};

        std::vector<int32_t> areaSidebarOffsets;
        std::vector<int32_t> areaViewportOffsets;

        std::vector<int32_t> areaChatbackOffsets;

        std::vector<int32_t> compassMaskLineLengths = std::vector<int32_t>(33);
        std::vector<int32_t> compassMaskLineOffsets = std::vector<int32_t>(33);

        std::vector<int8_t> textureBuffer =  std::vector<int8_t>(16384);

        std::string loginMessage0;
        std::string loginMessage1;
        std::string username;
        std::string password;

        int32_t titleLoginField = 0;
        int32_t hintType = 0;
        int32_t hintPlayer = 0;

        int32_t titleScreenState = 0;
        bool hideLoginButtons = false;
        int32_t sceneState = 0;
        int32_t minimapState = 0;

        int32_t flameCycle0 = 0;
        int32_t flameGradientCycle0 = 0;
        int32_t flameGradientCycle1 = 0;
        std::vector<int32_t> flameLineOffset;

        int32_t cameraX = 0;
        int32_t cameraY = 0;
        int32_t cameraZ = 0;
        int32_t cameraPitch = 0;
        int32_t cameraYaw = 0;
        int32_t orbitCameraX = 0;
        int32_t orbitCameraZ = 0;
        int32_t orbitCameraYawVelocity = 0;
        int32_t orbitCameraPitchVelocity = 0;
        int32_t orbitCameraPitch = 128;
        int32_t orbitCameraYaw = 0;
        /**
         * Used to keep the camera from falling below the terrain.
         */
        int32_t cameraPitchClamp = 0;

        int32_t sceneCenterZoneX = 0;
        int32_t sceneCenterZoneZ = 0;
        int32_t sceneBaseTileX = 0;
        int32_t sceneBaseTileZ = 0;

        int32_t hintTileX = 0;
        int32_t hintTileZ = 0;
        int32_t hintHeight = 0;
        int32_t hintOffsetX = 0;
        int32_t hintOffsetZ = 0;

        int32_t selectedObjID = 0;

        int32_t multizone = 0;

        int32_t flashingTab = -1;
        int32_t selectedTab = 3;
        int32_t sidebarInterfaceID = -1;
        std::vector<int32_t> tabInterfaceID = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

        Array3DIndexed<int8_t> levelTileFlags;
        Array3DIndexed<int32_t> levelHeightmap;

        Array3DIndexed<DoublyLinkedList> levelObjStacks = Array3DIndexed<DoublyLinkedList>(4, 104, 104);
        Array3DIndexed<int32_t> levelChunkBitset = Array3DIndexed<int32_t>(4, 13, 13);

        DoublyLinkedList spotanims;
        DoublyLinkedList projectiles = DoublyLinkedList();

        std::array<int32_t, 5> CameraModifierCycle{};
        DoublyLinkedList temporaryLocs;

        std::vector<std::vector<int8_t>> sceneMapLandData;
        std::vector<std::vector<int8_t>> sceneMapLocData;
        std::vector<int32_t> sceneMapIndex;
        std::vector<int32_t> sceneMapLandFile;
        std::vector<int32_t> sceneMapLocFile;

        std::array<int8_t, 5> cameraModifierEnabled{};
        std::array<int32_t, 5> cameraModifierWobbleScale{};
        std::array<int32_t, 5> cameraModifierJitter{};
        std::array<int32_t, 5> cameraModifierWobbleSpeed{};
        std::array<int32_t, 5> cameraModifierCycle{};

        NET_Address* addr = nullptr;
        ConnectionState connectionState = ConnectionState::DISCONNECTED;
        std::unique_ptr<Connection> connection;

        ISAACRandom randomIn;
        int32_t packetType = 0;
        int32_t packetSize = 0;
        int32_t lastPacketType0 = 0;
        int32_t lastPacketType1 = 0;
        int32_t lastPacketType2 = 0;
        int32_t bytesIn = 0;
        int32_t idleNetCycles = 0;

        /**
         * Game mouse buttons option.
         * 0 = TWO
         * 1 = ONE
         */
        int32_t mouseButtonsOption = 0;

        int32_t reportAbuseInterfaceID = -1;

        std::function<void()> pendingLoginTask;

        std::array<std::string, 5> playerOptions{};
        std::array<bool, 5> playerOptionPushDown{};
        std::array<int32_t, 9> archiveChecksum{};

        std::array<int32_t, 2000> varCache{};

        static constexpr std::array<int32_t, 5> LOC_OP_ACTION = {502, 900, 113, 872, 1062};
        static constexpr std::array<int32_t, 5> NPC_OP_ACTION = {20, 412, 225, 965, 478};
        static constexpr std::array<int32_t, 5> OBJ_IOP_ACTION = {74, 454, 539, 493, 847};
        static constexpr std::array<int32_t, 5> INV_OP_ACTION = {632, 78, 867, 431, 53};
        static constexpr std::array<int32_t, 5> OBJ_OP_ACTION = {652, 567, 234, 244, 214};

        static constexpr int32_t MAX_PLAYER_COUNT = 2048;
        static constexpr int32_t LOCAL_PLAYER_INDEX = 2047;

        static constexpr const char* RSA_MODULUS = "154716576955727211785325901324600371570743870951265699878791420662149847304806292871306627954592016176777271836594269271402325018074771615888141014620464135045992181111299136486686619037661880469066008356626133074380092172756020539926867427649496961875661371341305414542539735538097737233565433332038277041279";
        static constexpr const char* RSA_EXPONENT = "65537";

    };

}
