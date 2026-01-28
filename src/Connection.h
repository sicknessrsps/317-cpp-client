#pragma once
#include "PCH.h"

namespace SDL_Client
{
    class Connection
    {
    public:
        explicit Connection(NET_StreamSocket* socket);
        Connection() = default;
        ~Connection();
        void Write(std::vector<int8_t>& src, int32_t off, int32_t len);
        void Flush();
        int32_t Read();
        int32_t Read(std::vector<int8_t>& dst, int32_t off, int32_t len);
        void SetLagSimulation(int32_t percent);
        int32_t Available();
        bool Closed() const;
        void Close();
        void Debug();
    private:
        static constexpr size_t READ_BUFFER_SIZE = 65536;

        void ReadThreadFunc();
        void WriteThreadFunc();

        static int ReadThreadEntry(void* data);
        static int WriteThreadEntry(void* data);

        NET_StreamSocket* socket = nullptr;
        std::atomic<bool> closed = false;
        std::atomic<bool> stopping = false;

        SDL_Thread* readThread = nullptr;
        SDL_Thread* writeThread = nullptr;

        SDL_Mutex* readMutex = nullptr;
        SDL_Condition* readCv = nullptr;
        std::vector<uint8_t> readBuffer;
        size_t readHead = 0;
        size_t readTail = 0;

        SDL_Mutex* writeMutex = nullptr;
        SDL_Condition* writeCv = nullptr;
        std::vector<uint8_t> writeQueue;
        std::atomic<bool> writePending = false;
    };
}