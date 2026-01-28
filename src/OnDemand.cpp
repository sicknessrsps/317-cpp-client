#include "OnDemand.h"
#include "Buffer.h"
#include "Game.h"

namespace SDL_Client {

    OnDemand::OnDemand() : storeFileVersions(4), storeFilePriorities(4), storeFileChecksums(4), buffer(500) {
        lock = SDL_CreateMutex();
        queueMutex = SDL_CreateMutex();
        completedMutex = SDL_CreateMutex();
        prefetchesMutex = SDL_CreateMutex();
        messageMutex = SDL_CreateMutex();
    }

    OnDemand::~OnDemand() {
        Stop();
        if (lock) SDL_DestroyMutex(lock);
        if (queueMutex) SDL_DestroyMutex(queueMutex);
        if (completedMutex) SDL_DestroyMutex(completedMutex);
        if (prefetchesMutex) SDL_DestroyMutex(prefetchesMutex);
        if (messageMutex) SDL_DestroyMutex(messageMutex);
    }

    void OnDemand::Load(FileArchive& versionlist, Game* g) {
        const std::string versionFilenames[] = {"model_version", "anim_version", "midi_version", "map_version"};

        for (int32_t i = 0; i < 4; i++) {
            std::vector<std::int8_t> data = versionlist.Read(versionFilenames[i]);
            const int32_t count = data.size() / 2;

            Buffer buf(data);

            storeFileVersions[i] = std::vector<int32_t>(count);
            storeFilePriorities[i] = std::vector<uint8_t>(count);

            for (int32_t l = 0; l < count; l++) {
                storeFileVersions[i][l] = buf.ReadU16();
            }
        }

        const std::string crcFilenames[] = {"model_crc", "anim_crc", "midi_crc", "map_crc"};

        for (int32_t i = 0; i < 4; i++) {
            std::vector<std::int8_t> data = versionlist.Read(crcFilenames[i]);
            const int32_t count = data.size() / 4;
            Buffer buf(data);
            storeFileChecksums[i] = std::vector<int32_t>(count);
            for (int32_t l1 = 0; l1 < count; l1++) {
                storeFileChecksums[i][l1] = buf.Read32();
            }
        }

        std::vector<std::int8_t> data = versionlist.Read("model_index");
        int32_t count = storeFileVersions[0].size();

        modelIndex.resize(count);

        for (int32_t i = 0; i < count; i++) {
            if (i < static_cast<int32_t>(data.size())) {
                modelIndex[i] = data[i];
            } else {
                modelIndex[i] = 0;
            }
        }

        data = versionlist.Read("map_index");
        Buffer buf(data);
        count = data.size() / 7;

        mapIndex.resize(count);
        mapLandFile.resize(count);
        mapLocFile.resize(count);
        mapPrefetched.resize(count);

        for (int32_t i2 = 0; i2 < count; i2++) {
            mapIndex[i2] = buf.ReadU16();
            mapLandFile[i2] = buf.ReadU16();
            mapLocFile[i2] = buf.ReadU16();
            mapPrefetched[i2] = buf.ReadU8();
        }

        data = versionlist.Read("anim_index");
        buf = Buffer(data);
        count = data.size() / 2;
        animIndex.resize(count);

        for (int32_t j2 = 0; j2 < count; j2++) {
            animIndex[j2] = buf.ReadU16();
        }

        data = versionlist.Read("midi_index");
        buf = Buffer(data);
        count = data.size();
        midiIndex.resize(count);

        for (int32_t k2 = 0; k2 < count; k2++) {
            midiIndex[k2] = buf.ReadU8();
        }

        game = g;
        running = true;
        workerThread = SDL_CreateThread(RunThread, "OnDemand", this);
        if (workerThread == nullptr) {
            LOG_ERROR("OnDemand: Failed to create worker thread: %s", SDL_GetError());
        }
    }

    int OnDemand::RunThread(void* data) {
        static_cast<OnDemand*>(data)->Run();
        return 0;
    }

    void OnDemand::Stop() {
        running = false;
        if (workerThread != nullptr) {
            SDL_WaitThread(workerThread, nullptr);
            workerThread = nullptr;
        }
        if (connection) {
            connection->Close();
            connection.reset();
            socket = nullptr;  // Connection destroyed the socket
        } else if (socket != nullptr) {
            NET_DestroyStreamSocket(socket);
            socket = nullptr;
        }
    }

    int32_t OnDemand::GetFileCount(const int32_t store) {
        return storeFileVersions[store].size();
    }

    int32_t OnDemand::GetModelFlags(const int32_t id) {
        return modelIndex[id] & 0xff;
    }

    int32_t OnDemand::GetMapFile(int32_t type, int32_t x, int32_t z) {
        int32_t index = (x << 8) + z;
        for (size_t i = 0; i < mapIndex.size(); i++) {
            if (mapIndex[i] == index) {
                if (type == 0) {
                    return mapLandFile[i];
                } else {
                    return mapLocFile[i];
                }
            }
        }
        return -1;
    }

    int32_t OnDemand::GetSeqFrameCount() const {
        return animIndex.size();
    }

    bool OnDemand::HasMapLocFile(int32_t fileId) {
        for (size_t i = 0; i < mapIndex.size(); i++) {
            if (mapLocFile[i] == fileId) {
                return true;
            }
        }
        return false;
    }

    bool OnDemand::HasMidi(int32_t id) {
        if (id < 0 || id >= static_cast<int32_t>(midiIndex.size())) {
            return false;
        }
        return midiIndex[id] == 1;
    }

    bool OnDemand::Validate(int32_t expectedVersion, int32_t crc, const std::vector<int8_t>& src) {
        if (src.empty()) {
            return false;
        }

        if (src.size() < 2) {
            return false;
        }

        int32_t fileEndPos = src.size() - 2;
        int32_t fileVersion = ((src[fileEndPos] & 0xff) << 8) + (src[fileEndPos + 1] & 0xff);

        // Calculate CRC32
        uint32_t crc32 = 0xFFFFFFFF;
        for (int32_t i = 0; i < fileEndPos; i++) {
            uint8_t byte = src[i];
            crc32 ^= byte;
            for (int j = 0; j < 8; j++) {
                if (crc32 & 1) {
                    crc32 = (crc32 >> 1) ^ 0xEDB88320;
                } else {
                    crc32 >>= 1;
                }
            }
        }
        crc32 ^= 0xFFFFFFFF;

        return fileVersion == expectedVersion && static_cast<int32_t>(crc32) == crc;
    }

    void OnDemand::Request(const int32_t store, const int32_t file) {
        if ((store < 0) || (store >= static_cast<int32_t>(storeFileVersions.size())) ||
            (file < 0) || (file >= static_cast<int32_t>(storeFileVersions[store].size()))) {
            return;
        }
        if (storeFileVersions[store][file] == 0) {
            return;
        }

        SDL_LockMutex(lock);
        for (const auto& request : requests) {
            if ((request->store == store) && (request->file == file)) {
                SDL_UnlockMutex(lock);
                return;
            }
        }

        auto request = std::make_unique<OnDemandRequest>();
        request->store = store;
        request->file = file;
        request->important = true;

        OnDemandRequest* reqPtr = request.get();
        allRequests.push_back(std::move(request));

        SDL_LockMutex(queueMutex);
        queue.pushBack(reqPtr);
        SDL_UnlockMutex(queueMutex);

        requests.push_front(reqPtr);
        SDL_UnlockMutex(lock);
    }

    void OnDemand::RequestModel(const int32_t id) {
        Request(0, id);
    }

    int32_t OnDemand::Remaining() {
        SDL_LockMutex(lock);
        int32_t size = requests.size();
        SDL_UnlockMutex(lock);
        return size;
    }

    OnDemandRequest* OnDemand::Poll() {
        SDL_LockMutex(completedMutex);
        OnDemandRequest* request = static_cast<OnDemandRequest*>(completed.pollFront());
        SDL_UnlockMutex(completedMutex);
        if (request == nullptr) {
            return nullptr;
        }
        SDL_LockMutex(lock);
        requests.remove(request);
        SDL_UnlockMutex(lock);
        if (request->data.empty()) {
            return request;
        }
        // Decompress gzip data
        request->data = DecompressGzip(request->data);
        return request;
    }

    void OnDemand::Prefetch(uint8_t priority, int32_t archive, int32_t file) {
        if (game->filestores[0] == nullptr) {
            return;
        }
        if (file < 0 || file >= static_cast<int32_t>(storeFileVersions[archive].size())) {
            return;
        }
        if (storeFileVersions[archive][file] == 0) {
            return;
        }

        std::vector<int8_t> data = game->filestores[archive + 1]->Read(file);

        if (Validate(storeFileVersions[archive][file], storeFileChecksums[archive][file], data)) {
            return;
        }

        storeFilePriorities[archive][file] = priority;

        if (priority > topPriority) {
            topPriority = priority;
        }

        totalPrefetchFiles++;
    }

    void OnDemand::Prefetch(int32_t file, int32_t store) {
        if (game->filestores[0] == nullptr) {
            return;
        }
        if (file < 0 || file >= static_cast<int32_t>(storeFileVersions[store].size())) {
            return;
        }
        if (storeFileVersions[store][file] == 0) {
            return;
        }
        if (storeFilePriorities[store][file] == 0) {
            return;
        }
        if (topPriority == 0) {
            return;
        }

        auto request = std::make_unique<OnDemandRequest>();
        request->store = store;
        request->file = file;
        request->important = false;

        OnDemandRequest* reqPtr = request.get();
        allRequests.push_back(std::move(request));

        SDL_LockMutex(prefetchesMutex);
        prefetches.pushBack(reqPtr);
        SDL_UnlockMutex(prefetchesMutex);
    }

    void OnDemand::PrefetchMaps(bool members) {
        int32_t count = mapIndex.size();
        for (int32_t i = 0; i < count; i++) {
            if (members || (mapPrefetched[i] != 0)) {
                Prefetch(2, 3, mapLocFile[i]);
                Prefetch(2, 3, mapLandFile[i]);
            }
        }
    }

    void OnDemand::ClearPrefetches() {
        SDL_LockMutex(prefetchesMutex);
        prefetches.clear();
        SDL_UnlockMutex(prefetchesMutex);
    }

    void OnDemand::Send(OnDemandRequest* request) {
        if (socket == nullptr) {
            uint64_t now = SDL_GetTicks();
            if ((now - socketOpenTime) < 4000) {
                return;
            }
            socketOpenTime = now;

            LOG_INFO("OnDemand: Resolving %s...", Game::server.c_str());
            NET_Address* addr = NET_ResolveHostname(Game::server.c_str());
            if (addr == nullptr) {
                LOG_ERROR("OnDemand: DNS resolution failed for %s", Game::server.c_str());
                failCount++;
                return;
            }

            LOG_INFO("OnDemand: Connecting to port %d...", 43594 + game->portOffset);
            socket = NET_CreateClient(addr, 43594 + game->portOffset);
            NET_UnrefAddress(addr);

            if (socket == nullptr) {
                LOG_ERROR("OnDemand: Failed to create socket");
                failCount++;
                return;
            }

            // Wait for connection - use polling for Emscripten compatibility
            int32_t waitAttempts = 0;
            constexpr int32_t maxWaitAttempts = 200; // 200 * 50ms = 10 seconds max
            while (waitAttempts < maxWaitAttempts) {
                auto status = NET_GetConnectionStatus(socket);
                if (status != NET_SUCCESS && status != NET_FAILURE) {
                    // Still connecting
                    SDL_Delay(50);
                    waitAttempts++;
                    continue;
                }
                if (status == NET_SUCCESS) {
                    break;
                }
                // Connection failed
                LOG_ERROR("OnDemand: Connection failed");
                NET_DestroyStreamSocket(socket);
                socket = nullptr;
                failCount++;
                return;
            }

            if (waitAttempts >= maxWaitAttempts) {
                LOG_ERROR("OnDemand: Connection timeout");
                NET_DestroyStreamSocket(socket);
                socket = nullptr;
                failCount++;
                return;
            }

            LOG_INFO("OnDemand: Connected, sending handshake...");
            connection = std::make_unique<Connection>(socket);

            // Send handshake
            std::vector<int8_t> handshake = {15};
            connection->Write(handshake, 0, 1);
            connection->Flush();
            if (connection->Closed()) {
                LOG_ERROR("OnDemand: Connection closed after handshake write");
                socket = nullptr;
                connection.reset();
                partAvailable = 0;
                failCount++;
                return;
            }

            // Read 8 bytes response
            for (int j = 0; j < 8; j++) {
                if (connection->Read() == -1) {
                    LOG_ERROR("OnDemand: Failed to read handshake response byte %d", j);
                    socket = nullptr;
                    connection.reset();
                    partAvailable = 0;
                    failCount++;
                    return;
                }
            }

            LOG_INFO("OnDemand: Handshake complete");
            waitCycles = 0;
        }

        buffer[0] = static_cast<int8_t>(request->store);
        buffer[1] = static_cast<int8_t>(request->file >> 8);
        buffer[2] = static_cast<int8_t>(request->file);

        if (request->important) {
            buffer[3] = 2;
        } else if (!game->ingame) {
            buffer[3] = 1;
        } else {
            buffer[3] = 0;
        }

        connection->Write(buffer, 0, 4);
        connection->Flush();
        if (connection->Closed()) {
            socket = nullptr;
            connection.reset();
            partAvailable = 0;
            failCount++;
            return;
        }

        heartbeatCycle = 0;
        failCount = -10000;
    }

    void OnDemand::ReadData() {
        if (connection == nullptr) {
            return;
        }

        int32_t available = connection->Available();

        if ((partAvailable == 0) && (available >= 6)) {
            active = true;

            connection->Read(buffer, 0, 6);
            if (connection->Closed()) {
                socket = nullptr;
                connection.reset();
                partAvailable = 0;
                return;
            }

            int32_t store = buffer[0] & 0xff;
            int32_t file = ((buffer[1] & 0xff) << 8) + (buffer[2] & 0xff);
            int32_t size = ((buffer[3] & 0xff) << 8) + (buffer[4] & 0xff);
            int32_t part = buffer[5] & 0xff;

            current = nullptr;

            for (auto* request = static_cast<OnDemandRequest*>(pending.peekFront());
                 request != nullptr;
                 request = static_cast<OnDemandRequest*>(pending.prev())) {
                if ((request->store == store) && (request->file == file)) {
                    current = request;
                }
                if (current != nullptr) {
                    request->cycle = 0;
                }
            }

            if (current != nullptr) {
                waitCycles = 0;

                if (size == 0) {
                    LOG_ERROR("Rej: %d,%d", store, file);

                    current->data.clear();

                    if (current->important) {
                        SDL_LockMutex(completedMutex);
                        completed.pushBack(current);
                        SDL_UnlockMutex(completedMutex);
                    } else {
                        current->unlink();
                    }

                    current = nullptr;
                } else {
                    if ((current->data.empty()) && (part == 0)) {
                        current->data.resize(size);
                    }

                    if (current->data.empty()) {
                        LOG_ERROR("missing start of file");
                        socket = nullptr;
                        connection.reset();
                        partAvailable = 0;
                        return;
                    }
                }
            }

            partOffset = part * 500;
            partAvailable = 500;

            if (partAvailable > (size - (part * 500))) {
                partAvailable = size - (part * 500);
            }

            // Re-check available bytes after consuming the header
            available = connection->Available();
        }

        if ((partAvailable > 0) && (available >= partAvailable)) {
            active = true;
            std::vector<int8_t>* dst = &buffer;
            int32_t offset = 0;

            if (current != nullptr) {
                dst = &current->data;
                offset = partOffset;
            }

            connection->Read(*dst, offset, partAvailable);
            if (connection->Closed()) {
                socket = nullptr;
                connection.reset();
                partAvailable = 0;
                return;
            }

            if (((partAvailable + partOffset) >= static_cast<int32_t>(dst->size())) && (current != nullptr)) {
                if (game->filestores[0] != nullptr) {
                    game->filestores[current->store + 1]->Write(*dst, current->file, dst->size());
                }

                if (!current->important && (current->store == 3)) {
                    current->important = true;
                    current->store = 93;
                }

                if (current->important) {
                    SDL_LockMutex(completedMutex);
                    completed.pushBack(current);
                    SDL_UnlockMutex(completedMutex);
                } else {
                    current->unlink();
                }
            }
            partAvailable = 0;
        }
    }

    void OnDemand::HandleQueue() {
        SDL_LockMutex(queueMutex);
        auto* request = static_cast<OnDemandRequest*>(queue.pollFront());
        SDL_UnlockMutex(queueMutex);

        int32_t processedCount = 0;
        int32_t missingCount = 0;
        int32_t completedCount = 0;

        while (request != nullptr) {
            active = true;
            std::vector<int8_t> data;

            if (game->filestores[0] != nullptr) {
                data = game->filestores[request->store + 1]->Read(request->file);
            }
            if (!Validate(storeFileVersions[request->store][request->file],
                         storeFileChecksums[request->store][request->file], data)) {
                data.clear();
            }

            SDL_LockMutex(queueMutex);
            if (data.empty()) {
                missing.pushBack(request);
                missingCount++;
            } else {
                request->data = std::move(data);
                SDL_LockMutex(completedMutex);
                completed.pushBack(request);
                SDL_UnlockMutex(completedMutex);
                completedCount++;
            }
            processedCount++;
            request = static_cast<OnDemandRequest*>(queue.pollFront());
            SDL_UnlockMutex(queueMutex);
        }

        if (processedCount > 0) {
            LOG_INFO("OnDemand: HandleQueue processed %d requests (%d from cache, %d need download)",
                     processedCount, completedCount, missingCount);
        }
    }

    void OnDemand::HandlePending() {
        importantCount = 0;
        requestCount = 0;

        for (OnDemandRequest* request = static_cast<OnDemandRequest*>(pending.peekFront());
             request != nullptr;
             request = static_cast<OnDemandRequest*>(pending.prev())) {
            if (request->important) {
                importantCount++;
            } else {
                requestCount++;
            }
        }

        while (importantCount < 10) {
            OnDemandRequest* request = static_cast<OnDemandRequest*>(missing.pollFront());

            if (request == nullptr) {
                break;
            }

            if (storeFilePriorities[request->store][request->file] != 0) {
                loadedPrefetchFiles++;
            }

            storeFilePriorities[request->store][request->file] = 0;
            pending.pushBack(request);
            importantCount++;
            Send(request);
            active = true;
        }
    }

    void OnDemand::HandleExtras() {
        while ((importantCount == 0) && (requestCount < 10)) {

            if (topPriority == 0) {
                break;
            }

            SDL_LockMutex(prefetchesMutex);
            OnDemandRequest* extra = static_cast<OnDemandRequest*>(prefetches.pollFront());
            SDL_UnlockMutex(prefetchesMutex);

            while (extra != nullptr) {
                if (storeFilePriorities[extra->store][extra->file] != 0) {
                    storeFilePriorities[extra->store][extra->file] = 0;
                    pending.pushBack(extra);
                    Send(extra);
                    active = true;

                    if (loadedPrefetchFiles < totalPrefetchFiles) {
                        loadedPrefetchFiles++;
                    }

                    SetMessage("Loading extra files - " + std::to_string(loadedPrefetchFiles * 100 / totalPrefetchFiles) + "%");
                    requestCount++;

                    if (requestCount == 10) {
                        return;
                    }
                }
                SDL_LockMutex(prefetchesMutex);
                extra = static_cast<OnDemandRequest*>(prefetches.pollFront());
                SDL_UnlockMutex(prefetchesMutex);
            }

            for (int32_t store = 0; store < 4; store++) {
                std::vector<uint8_t>& priorities = storeFilePriorities[store];
                for (size_t file = 0; file < priorities.size(); file++) {
                    if (priorities[file] != topPriority) {
                        continue;
                    }

                    priorities[file] = 0;

                    auto req = std::make_unique<OnDemandRequest>();
                    req->store = store;
                    req->file = file;
                    req->important = false;

                    OnDemandRequest* reqPtr = req.get();
                    allRequests.push_back(std::move(req));

                    pending.pushBack(reqPtr);
                    Send(reqPtr);
                    active = true;

                    if (loadedPrefetchFiles < totalPrefetchFiles) {
                        loadedPrefetchFiles++;
                    }

                    SetMessage("Loading extra files - " + std::to_string(loadedPrefetchFiles * 100 / totalPrefetchFiles) + "%");
                    requestCount++;

                    if (requestCount == 10) {
                        return;
                    }
                }
            }
            topPriority--;
        }
    }

    void OnDemand::Run() {
        while (running) {
            cycle++;
            int32_t del = 20;

            if ((topPriority == 0) && (game->filestores[0] != nullptr)) {
                del = 50;
            }

            SDL_Delay(del);

            active = true;

            for (int32_t j = 0; j < 100; j++) {
                if (!active) {
                    break;
                }
                active = false;
                HandleQueue();
                HandlePending();
                if ((importantCount == 0) && (j >= 5)) {
                    break;
                }

                HandleExtras();
                if (connection != nullptr) {
                    ReadData();
                }
            }

            bool loading = false;

            for (OnDemandRequest* request = static_cast<OnDemandRequest*>(pending.peekFront());
                 request != nullptr;
                 request = static_cast<OnDemandRequest*>(pending.prev())) {
                if (request->important) {
                    loading = true;
                    request->cycle++;

                    if (request->cycle > 50) {
                        request->cycle = 0;
                        Send(request);
                    }
                }
            }

            if (!loading) {
                for (OnDemandRequest* request = static_cast<OnDemandRequest*>(pending.peekFront());
                     request != nullptr;
                     request = static_cast<OnDemandRequest*>(pending.prev())) {
                    loading = true;
                    request->cycle++;

                    if (request->cycle > 50) {
                        request->cycle = 0;
                        Send(request);
                    }
                }
            }

            if (loading) {
                waitCycles++;

                if (waitCycles > 750) {
                    if (socket != nullptr) {
                        NET_DestroyStreamSocket(socket);
                        socket = nullptr;
                    }
                    connection.reset();
                    partAvailable = 0;
                }
            } else {
                waitCycles = 0;
                SetMessage("");
            }

            if (game->ingame && (socket != nullptr) && (connection != nullptr) &&
                ((topPriority > 0) || (game->filestores[0] == nullptr))) {
                heartbeatCycle++;

                if (heartbeatCycle > 500) {
                    heartbeatCycle = 0;
                    buffer[0] = 0;
                    buffer[1] = 0;
                    buffer[2] = 0;
                    buffer[3] = 10;

                    connection->Write(buffer, 0, 4);
                    if (connection->Closed()) {
                        waitCycles = 5000;
                    }
                }
            }
        }
    }

    std::vector<int8_t> OnDemand::DecompressGzip(const std::vector<int8_t>& compressed) {
        if (compressed.empty()) {
            return {};
        }

        z_stream stream = {};
        stream.next_in = reinterpret_cast<Bytef*>(const_cast<int8_t*>(compressed.data()));
        stream.avail_in = compressed.size();

        // Use inflateInit2 with window bits = 15 + 16 for gzip format
        if (inflateInit2(&stream, 15 + 16) != Z_OK) {
            return {};
        }

        std::vector<int8_t> decompressed;
        const size_t chunkSize = 32768; // 32KB chunks
        std::vector<int8_t> buf(chunkSize);

        int32_t ret;
        do {
            stream.next_out = reinterpret_cast<Bytef*>(buf.data());
            stream.avail_out = chunkSize;

            ret = inflate(&stream, Z_NO_FLUSH);

            if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                inflateEnd(&stream);
                return {};
            }

            size_t have = chunkSize - stream.avail_out;
            decompressed.insert(decompressed.end(), buf.begin(), buf.begin() + have);

        } while (ret != Z_STREAM_END);

        inflateEnd(&stream);
        return decompressed;
    }
}
