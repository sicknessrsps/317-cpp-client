#include "Buffer.h"
#include "BigInteger.h"
#include "Game.h"


namespace SDL_Client {

    Buffer::Buffer(const int32_t size) : data(size), position(0), bitPosition(0) {}

    Buffer::Buffer(const std::vector<std::int8_t>& src) : data(src), position(0), bitPosition(0) {}

    void Buffer::Clear()
    {
        data.clear();
        position = 0;
        bitPosition = 0;
    }

    void Buffer::WriteOp(int32_t op) {
        data[position++] = static_cast<int8_t>(op + random.GetNextKey());
    }

    void Buffer::WriteSize(int32_t value) {
        data[position - value - 1] = static_cast<int8_t>(value);
    }

    void Buffer::Write8C(int32_t value) {
        data[position++] = static_cast<int8_t>(-value);
    }

    void Buffer::Write8S(int32_t value) {
        data[position++] = static_cast<int8_t>(128 - value);
    }

    void Buffer::Write16(int32_t value) {
        data[position++] = static_cast<int8_t>(value >> 8);
        data[position++] = static_cast<int8_t>(value);
    }

    void Buffer::Write16A(int32_t value) {
        data[position++] = static_cast<int8_t>(value >> 8);
        data[position++] = static_cast<int8_t>(value + 128);
    }

    void Buffer::Write16LE(int32_t value) {
        data[position++] = static_cast<int8_t>(value);
        data[position++] = static_cast<int8_t>(value >> 8);
    }

    void Buffer::Write16LEA(int32_t value) {
        data[position++] = static_cast<int8_t>(value + 128);
        data[position++] = static_cast<int8_t>(value >> 8);
    }

    /*void Buffer::Write24(int32_t value) {
    }*/

    void Buffer::Write32(int32_t value) {
        data[position++] = static_cast<int8_t>(value >> 24);
        data[position++] = static_cast<int8_t>(value >> 16);
        data[position++] = static_cast<int8_t>(value >> 8);
        data[position++] = static_cast<int8_t>(value);
    }

    void Buffer::Write32LE(int32_t value) {
        data[position++] = static_cast<int8_t>(value);
        data[position++] = static_cast<int8_t>(value >> 8);
        data[position++] = static_cast<int8_t>(value >> 16);
        data[position++] = static_cast<int8_t>(value >> 24);
    }

    void Buffer::Write64(int64_t value) {
        data[position++] = static_cast<int8_t>(static_cast<int32_t>(value >> 56));
        data[position++] = static_cast<int8_t>(static_cast<int32_t>(value >> 48));
        data[position++] = static_cast<int8_t>(static_cast<int32_t>(value >> 40));
        data[position++] = static_cast<int8_t>(static_cast<int32_t>(value >> 32));
        data[position++] = static_cast<int8_t>(static_cast<int32_t>(value >> 24));
        data[position++] = static_cast<int8_t>(static_cast<int32_t>(value >> 16));
        data[position++] = static_cast<int8_t>(static_cast<int32_t>(value >> 8));
        data[position++] = static_cast<int8_t>(static_cast<int32_t>(value));
    }

    void Buffer::WriteString(const std::string &s) {
        for (uint8_t value : s) {
            Write8(value);
        }
        Write8('\n');
    }

    void Buffer::Write(const std::vector<std::int8_t> &src, int32_t off, int32_t len) {
        std::copy_n(src.begin() + off, len, data.begin() + position);
        position += len;
    }

    /*void Buffer::Write(const std::vector<std::uint8_t> &src) {
    }*/

    void Buffer::WriteA(const std::vector<int8_t>& src, int32_t off, int32_t len) {
        for (int32_t i = (off + len) - 1; i >= off; i--) {
            data[position++] = static_cast<int8_t>(src[i] + 128);
        }
    }

    std::int8_t Buffer::Read8() {
        return data[position++];
    }

    std::int8_t Buffer::Read8C() {
        return static_cast<int8_t>(-data[position++]);
    }

    int8_t Buffer::Read8S() {
        return static_cast<int8_t>(128 - data[position++]);
    }

    int32_t Buffer::ReadU8() {
        return data[position++] & 0xff;
    }

    int32_t Buffer::ReadU8A() {
        return (data[position++] - 128) & 0xff;
    }

    int32_t Buffer::ReadU8C() {
        return -data[position++] & 0xff;
    }

    int32_t Buffer::ReadU8S() {
        return (128 - data[position++]) & 0xff;
    }

    int32_t Buffer::ReadSmart() {
        if ((data[position] & 0xff) < 128) {
            return ReadU8() - 64;
        } else {
            return ReadU16() - 49152;
        }
    }

    /**
     * Gets a 1 or 2 byte varint which has the range [0...32768].
     *
     * @return the value.
     */
    int32_t Buffer::ReadUSmart() {
        if ((data[position] & 0xff) < 128) {
            return ReadU8();
        } else {
            return ReadU16() - 32768;
        }
    }

    std::uint16_t Buffer::ReadU16()
    {
        position += 2;
        return ((data[position - 2] & 0xFF) << 8)  |
               (data[position - 1] & 0xFF);
    }

    int32_t Buffer::ReadU16A() {
        position += 2;
        return ((data[position - 2] & 0xff) << 8) + ((data[position - 1] - 128) & 0xff);
    }

    int32_t Buffer::ReadU16LE() {
        position += 2;
        return ((data[position - 1] & 0xff) << 8) + (data[position - 2] & 0xff);
    }

    int32_t Buffer::ReadU16LEA() {
        position += 2;
        return ((data[position - 1] & 0xff) << 8) + ((data[position - 2] - 128) & 0xff);
    }

    int32_t Buffer::Read16() {
        position += 2;
        int32_t value = ((data[position - 2] & 0xff) << 8) + (data[position - 1] & 0xff);
        if (value > 32767) {
            value -= 65536;
        }
        return value;
    }

    int16_t Buffer::Read16LE() {
        position += 2;
        int32_t value = ((data[position - 1] & 0xff) << 8) + (data[position - 2] & 0xff);
        if (value > 32767) {
            value -= 65536;
        }
        return value;
    }

    int16_t Buffer::Read16LEA() {
        position += 2;
        int32_t value = ((data[position - 1] & 0xff) << 8) + ((data[position - 2] - 128) & 0xff);
        if (value > 32767) {
            value -= 65536;
        }
        return value;
    }

    int32_t Buffer::Read24() {
        position += 3;
        return ((data[position - 3] & 0xFF) << 16) |
               ((data[position - 2] & 0xFF) << 8)  |
               (data[position - 1] & 0xFF);
    }

    int32_t Buffer::Read32()
    {
        position += 4;
        return ((data[position - 4] & 0xFF) << 24) |
               ((data[position - 3] & 0xFF) << 16) |
               ((data[position - 2] & 0xFF) << 8)  |
               (data[position - 1] & 0xFF);
    }

    int32_t Buffer::Read32ME() {
        position += 4;
        return ((data[position - 3] & 0xff) << 24) + ((data[position - 4] & 0xff) << 16) + ((data[position - 1] & 0xff) << 8) + (data[position - 2] & 0xff);
    }

    int32_t Buffer::Read32RME() {
        position += 4;
        return ((data[position - 2] & 0xff) << 24) + ((data[position - 1] & 0xff) << 16) + ((data[position - 4] & 0xff) << 8) + (data[position - 3] & 0xff);
    }

    std::int64_t Buffer::Read64() {
        const int64_t msi = static_cast<int64_t>(Read32()) & 0xffffffffL;
        const int64_t lsi = static_cast<int64_t>(Read32()) & 0xffffffffL;
        return (msi << 32) + lsi;
    }

    std::string Buffer::ReadString() {
        const int32_t start = position;
        while (true) {
            if (data[position++] == '\n') {
                break;
            }
        }
        return std::string(data.begin() + start, data.begin() + position - 1);
    }

    void Buffer::Read(std::vector<int8_t>& dst, int32_t off, int32_t len) {
        std::copy_n(data.begin() + position, len, dst.begin() + off);
        position += len;
    }

    void Buffer::Read(std::vector<int8_t>& dst) {
        Read(dst, 0, dst.size());
    }

    void Buffer::ReadReversed(std::vector<int8_t>& dst, int32_t off, int32_t len) {
        for (int32_t i = (off + len) - 1; i >= off; i--) {
            dst[i] = data[position++];
        }
    }

    void Buffer::AccessBits() {
        bitPosition = position * 8;
    }

    int32_t Buffer::ReadN(int32_t n) {
        int32_t bytePos = bitPosition >> 3;
        int32_t remaining = 8 - (bitPosition & 7);
        int32_t value = 0;
        bitPosition += n;
        for (; n > remaining; remaining = 8) {
            value += (data[bytePos++] & BITMASK[remaining]) << (n - remaining);
            n -= remaining;
        }
        if (n == remaining) {
            value += data[bytePos] & BITMASK[remaining];
        } else {
            value += (data[bytePos] >> (remaining - n)) & BITMASK[n];
        }
        return value;
    }

    void Buffer::AccessBytes() {
        position = (bitPosition + 7) / 8;
    }

    void Buffer::Encrypt(const char* exponent, const char* modulus) {
        int32_t length = position;
        position = 0;
        std::vector<int8_t> raw(length);
        Read(raw, 0, length);
        std::vector<int8_t> encrypted = Game::enableRSA ? BigInteger(raw).ModPow(BigInteger(exponent), BigInteger(modulus)).ToByteArray() : raw;
        position = 0;
        Write8(static_cast<int32_t>(encrypted.size()));
        Write(encrypted, 0, static_cast<int32_t>(encrypted.size()));
    }

    void Buffer::Write8(int32_t value)
    {
        data[position++] = static_cast<int8_t>(value);
    }
}
