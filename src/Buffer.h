#pragma once
#include "PCH.h"
#include "ISAACRandom.h"

namespace SDL_Client {

    class Buffer {
    public:
        Buffer() = default;
        explicit Buffer(int32_t size);
        explicit Buffer(const std::vector<int8_t>& src);

        void Clear();
        void WriteOp(int32_t op);
        void WriteSize(int32_t value);
        void Write8(int32_t value);
        void Write8C(int32_t value);
        void Write8S(int32_t value);
        void Write16(int32_t value);
        void Write16A(int32_t value);
        void Write16LE(int32_t value);
        void Write16LEA(int32_t value);
        /*void Write24(int32_t value);*/
        void Write32(int32_t value);
        void Write32LE(int32_t value);
        void Write64(int64_t value);
        void WriteString(const std::string& value);
        void Write(const std::vector<int8_t>& src,int32_t off, int32_t len);
        //void Write(const std::vector<std::int8_t>& src);
        void WriteA(const std::vector<int8_t>& src, int32_t off, int32_t len);

        int8_t Read8();
        int8_t Read8C();
        int8_t Read8S();
        int32_t ReadU8();
        int32_t ReadU8A();
        int32_t ReadU8C();
        int32_t ReadU8S();
        int32_t ReadSmart();
        int32_t ReadUSmart();
        uint16_t ReadU16();
        int32_t ReadU16A();
        int32_t ReadU16LE();
        int32_t ReadU16LEA();
        int32_t Read16();
        int16_t Read16LE();
        int16_t Read16LEA();
        int32_t Read24();
        int32_t Read32();
        int32_t Read32ME();
        int32_t Read32RME();
        int64_t Read64();
        std::string ReadString();
        void Read(std::vector<int8_t>& dst, int32_t off, int32_t len);
        void Read(std::vector<int8_t>& dst);
        void ReadReversed(std::vector<int8_t>& dst, int32_t off, int32_t len);

        void AccessBits();
        int32_t ReadN(int32_t n);
        void AccessBytes();

        // RSA encryption - exponent and modulus as decimal strings (like Java BigInteger)
        void Encrypt(const char* exponent, const char* modulus);

    public:
        int32_t position = 0;
        std::vector<int8_t> data;
        ISAACRandom random;
        int32_t bitPosition = 0;

    private:
        static constexpr int32_t BITMASK[32] = {
            0, 1, 3, 7, 15, 31, 63, 127,
            255, 511, 1023, 2047, 4095, 8191, 16383, 32767,
            65535, 0x1ffff, 0x3ffff, 0x7ffff, 0xfffff, 0x1fffff,
            0x3fffff, 0x7fffff, 0xffffff, 0x1ffffff, 0x3ffffff, 0x7ffffff,
            0xfffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff
        };
    };
};