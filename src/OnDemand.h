#pragma once
#include "PCH.h"
#include "FileArchive.h"
#include "DoublyLinkedList.h"
#include "Connection.h"

namespace SDL_Client {
    class Game;

    struct OnDemandRequest : public DoublyLinkedList::Node {
        int32_t store = 0;
        int32_t file = 0;
        bool important = false;
        int32_t cycle = 0;
        std::vector<int8_t> data;
    };

    class OnDemand {
    public:
        OnDemand();
        ~OnDemand();

        // Initialization
        void Load(FileArchive& versionlist, Game* g);
        void Stop();

        // File info
        int32_t GetFileCount(int32_t store);
        int32_t GetModelFlags(int32_t id);
        int32_t GetMapFile(int32_t type, int32_t x, int32_t z);
        int32_t GetSeqFrameCount() const;
        bool HasMapLocFile(int32_t fileId);
        bool HasMidi(int32_t id);

        // Requests
        void Request(int32_t store, int32_t file);
        void RequestModel(int32_t id);
        int32_t Remaining();
        OnDemandRequest* Poll();

        // Prefetching
        void Prefetch(uint8_t priority, int32_t archive, int32_t file);
        void Prefetch(int32_t file, int32_t store);
        void PrefetchMaps(bool members);
        void ClearPrefetches();

        // Validation
        bool Validate(int32_t expectedVersion, int32_t crc, const std::vector<int8_t>& src);

        // Background thread
        void Run();
    public:
        // Public members (accessed by Game)
        std::vector<int32_t> animIndex;
        int32_t totalPrefetchFiles = 0;
        int32_t loadedPrefetchFiles = 0;

        int32_t failCount = 0;
        int32_t cycle = 0;
        std::list<OnDemandRequest*> requests;

        // Thread-safe message access
        std::string GetMessage() {
            SDL_LockMutex(messageMutex);
            std::string copy = message;
            SDL_UnlockMutex(messageMutex);
            return copy;
        }

        void SetMessage(const std::string& msg) {
            SDL_LockMutex(messageMutex);
            message = msg;
            SDL_UnlockMutex(messageMutex);
        }

    private:
        // Network operations
        void Send(OnDemandRequest* request);
        void ReadData();
        void HandleQueue();
        void HandlePending();
        void HandleExtras();

        // Decompression
        std::vector<int8_t> DecompressGzip(const std::vector<int8_t>& compressed);

        // File version/checksum data
        std::vector<std::vector<int32_t>> storeFileVersions;
        std::vector<std::vector<int32_t>> storeFileChecksums;
        std::vector<std::vector<uint8_t>> storeFilePriorities;
        std::vector<uint8_t> modelIndex;

        // Map data
        std::vector<int32_t> mapIndex;
        std::vector<int32_t> mapLandFile;
        std::vector<int32_t> mapLocFile;
        std::vector<int32_t> mapPrefetched;
        std::vector<int32_t> midiIndex;

        // Request queues - these own the OnDemandRequest objects
        DoublyLinkedList pending;
        DoublyLinkedList prefetches;
        DoublyLinkedList completed;
        DoublyLinkedList missing;
        DoublyLinkedList queue;
        std::vector<std::unique_ptr<OnDemandRequest>> allRequests; // Owns all request objects

        // Networking
        NET_StreamSocket* socket = nullptr;
        std::unique_ptr<Connection> connection;
        std::vector<int8_t> buffer;

        // State
        Game* game = nullptr;
        OnDemandRequest* current = nullptr;
        int32_t partOffset = 0;
        int32_t partAvailable = 0;
        int32_t waitCycles = 0;
        int32_t heartbeatCycle = 0;
        int32_t importantCount = 0;
        int32_t requestCount = 0;
        int32_t topPriority = 0;
        uint64_t socketOpenTime = 0;
        bool running = false;
        bool active = false;

        // Threading
        SDL_Mutex* lock = nullptr;
        SDL_Mutex* queueMutex = nullptr;
        SDL_Mutex* completedMutex = nullptr;
        SDL_Mutex* prefetchesMutex = nullptr;
        SDL_Mutex* messageMutex = nullptr;
        SDL_Thread* workerThread = nullptr;

        // Message (protected by messageMutex)
        std::string message;

        static int32_t RunThread(void* data);
    };
}
