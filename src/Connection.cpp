#include "Connection.h"

#include "FileStore.h"

namespace SDL_Client
{
    Connection::Connection(NET_StreamSocket* socket) : socket(socket)
    {
        NET_SetStreamSocketNoDelay(socket, true);
        readBuffer.resize(READ_BUFFER_SIZE);

        readMutex = SDL_CreateMutex();
        readCv = SDL_CreateCondition();
        writeMutex = SDL_CreateMutex();
        writeCv = SDL_CreateCondition();

        readThread = SDL_CreateThread(ReadThreadEntry, "ReadThread", this);
        writeThread = SDL_CreateThread(WriteThreadEntry, "WriteThread", this);
    }

    Connection::~Connection()
    {
        Close();
    }

    int Connection::ReadThreadEntry(void* data)
    {
        static_cast<Connection*>(data)->ReadThreadFunc();
        return 0;
    }

    int Connection::WriteThreadEntry(void* data)
    {
        static_cast<Connection*>(data)->WriteThreadFunc();
        return 0;
    }

    void Connection::ReadThreadFunc()
    {
        uint8_t tempBuffer[512];

        while (!stopping.load()) {
            if (closed.load()) {
                break;
            }

            // Wait for socket to be readable with timeout to allow checking stop condition
            void* sockets[1] = { socket };
            int32_t ready = NET_WaitUntilInputAvailable(sockets, 1, 50);
            if (ready < 0) {
                closed.store(true);
                SDL_LockMutex(readMutex);
                SDL_BroadcastCondition(readCv);
                SDL_UnlockMutex(readMutex);
                break;
            }
            if (ready == 0) {
                continue;
            }

            int32_t n = NET_ReadFromStreamSocket(socket, tempBuffer, sizeof(tempBuffer));

            if (n > 0) {
                SDL_LockMutex(readMutex);
                for (int32_t i = 0; i < n; i++) {
                    size_t nextHead = (readHead + 1) % READ_BUFFER_SIZE;
                    if (nextHead == readTail) {
                        break;  // Buffer full
                    }
                    readBuffer[readHead] = tempBuffer[i];
                    readHead = nextHead;
                }
                SDL_BroadcastCondition(readCv);
                SDL_UnlockMutex(readMutex);
            } else {
                // n <= 0: connection closed or error
                closed.store(true);
                SDL_LockMutex(readMutex);
                SDL_BroadcastCondition(readCv);
                SDL_UnlockMutex(readMutex);
                break;
            }
        }
    }

    void Connection::WriteThreadFunc()
    {
        while (!stopping.load()) {
            std::vector<uint8_t> toWrite;
            {
                SDL_LockMutex(writeMutex);
                while (writeQueue.empty() && !stopping.load() && !closed.load()) {
                    SDL_WaitConditionTimeout(writeCv, writeMutex, 50);
                }
                if (stopping.load() || closed.load()) {
                    if (!writeQueue.empty() && !closed.load()) {
                        toWrite = std::move(writeQueue);
                        writeQueue.clear();
                    } else {
                        SDL_UnlockMutex(writeMutex);
                        break;
                    }
                } else {
                    toWrite = std::move(writeQueue);
                    writeQueue.clear();
                }
                SDL_UnlockMutex(writeMutex);
            }
            if (!toWrite.empty() && socket && !closed.load()) {
                if (!NET_WriteToStreamSocket(socket, toWrite.data(), static_cast<int32_t>(toWrite.size()))) {
                    closed.store(true);
                    writePending.store(false);
                    SDL_LockMutex(writeMutex);
                    SDL_BroadcastCondition(writeCv);
                    SDL_UnlockMutex(writeMutex);
                    break;
                }
            }
            // Signal that write completed
            writePending.store(false);
            SDL_LockMutex(writeMutex);
            SDL_BroadcastCondition(writeCv);
            SDL_UnlockMutex(writeMutex);
        }
    }

    void Connection::Write(std::vector<int8_t>& src, int32_t off, int32_t len)
    {
        if (closed.load()) {
            return;
        }
        SDL_LockMutex(writeMutex);
        const uint8_t* data = reinterpret_cast<const uint8_t*>(src.data()) + off;
        writeQueue.insert(writeQueue.end(), data, data + len);
        writePending.store(true);
        SDL_BroadcastCondition(writeCv);
        SDL_UnlockMutex(writeMutex);
    }

    void Connection::Flush()
    {
        // Wait until write is complete (queue empty AND no write in progress)
        SDL_LockMutex(writeMutex);
        while (writePending.load() && !closed.load()) {
            SDL_WaitConditionTimeout(writeCv, writeMutex, 50);
        }
        SDL_UnlockMutex(writeMutex);
    }

    int32_t Connection::Read()
    {
        while (true) {
            SDL_LockMutex(readMutex);
            if (readTail != readHead) {
                uint8_t byte = readBuffer[readTail];
                readTail = (readTail + 1) % READ_BUFFER_SIZE;
                SDL_UnlockMutex(readMutex);
                return byte;
            }
            if (closed.load()) {
                SDL_UnlockMutex(readMutex);
                return -1;
            }
            SDL_UnlockMutex(readMutex);
            SDL_Delay(1);
        }
    }

    int32_t Connection::Read(std::vector<int8_t>& dst, int32_t off, int32_t len)
    {
        uint8_t* writePtr = reinterpret_cast<uint8_t*>(dst.data()) + off;
        int32_t remaining = len;

        SDL_LockMutex(readMutex);
        while (remaining > 0) {
            // Wait for data if buffer is empty
            while (readTail == readHead && !closed.load()) {
                SDL_WaitConditionTimeout(readCv, readMutex, 50);
            }

            if (closed.load() && readTail == readHead) {
                break;
            }

            // Read available data
            while (remaining > 0 && readTail != readHead) {
                *writePtr = readBuffer[readTail];
                readTail = (readTail + 1) % READ_BUFFER_SIZE;
                writePtr++;
                remaining--;
            }
        }
        SDL_UnlockMutex(readMutex);
        return len - remaining;
    }

    void Connection::SetLagSimulation(int32_t percent)
    {
        NET_SimulateStreamPacketLoss(socket, percent);
    }

    int32_t Connection::Available()
    {
        if (closed.load()) {
            return 0;
        }
        SDL_LockMutex(readMutex);
        int32_t available;
        if (readHead >= readTail) {
            available = static_cast<int32_t>(readHead - readTail);
        } else {
            available = static_cast<int32_t>(READ_BUFFER_SIZE - readTail + readHead);
        }
        SDL_UnlockMutex(readMutex);
        return available;
    }

    bool Connection::Closed() const
    {
        return closed.load();
    }

    void Connection::Close()
    {
        if (stopping.exchange(true)) {
            return;
        }

        closed.store(true);

        if (readCv) {
            SDL_LockMutex(readMutex);
            SDL_BroadcastCondition(readCv);
            SDL_UnlockMutex(readMutex);
        }
        if (writeCv) {
            SDL_LockMutex(writeMutex);
            SDL_BroadcastCondition(writeCv);
            SDL_UnlockMutex(writeMutex);
        }

        if (readThread) {
            SDL_WaitThread(readThread, nullptr);
            readThread = nullptr;
        }
        if (writeThread) {
            SDL_WaitThread(writeThread, nullptr);
            writeThread = nullptr;
        }

        if (readMutex) {
            SDL_DestroyMutex(readMutex);
            readMutex = nullptr;
        }
        if (readCv) {
            SDL_DestroyCondition(readCv);
            readCv = nullptr;
        }
        if (writeMutex) {
            SDL_DestroyMutex(writeMutex);
            writeMutex = nullptr;
        }
        if (writeCv) {
            SDL_DestroyCondition(writeCv);
            writeCv = nullptr;
        }

        if (socket) {
            NET_DestroyStreamSocket(socket);
            socket = nullptr;
        }
    }

    void Connection::Debug()
    {
        LOG_INFO("Connection Debug:");
        LOG_INFO("  Socket: %p", socket);
        LOG_INFO("  Closed: %s", closed.load() ? "true" : "false");

        if (!closed.load() && socket) {
            LOG_INFO("  Available bytes: %d", Available());
            LOG_INFO("  Write queue size: %zu", writeQueue.size());
        }
    }
}
