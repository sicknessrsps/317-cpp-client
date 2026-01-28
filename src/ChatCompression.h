#pragma once
#include "PCH.h"
#include "Buffer.h"

namespace SDL_Client {

    class ChatCompression {
    public:
        static std::string Unpack(int32_t length, Buffer& in);
        static void Pack(const std::string& in, Buffer& out);
        static std::string Format(const std::string& string);
        ChatCompression() = delete;

    private:
        static constexpr char TABLE[] = {
            // only this first row is actually combined with anything else
            ' ', 'e', 't', 'a', 'o', 'i', 'h', 'n', 's', 'r', 'd', 'l', 'u',
            // the rest are just 'accepted' values.
            'm', 'w', 'c', 'y', 'f', 'g', 'p', 'b', 'v', 'k', 'x', 'j', 'q', 'z',
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
            ' ', '!', '?', '.', ',', ':', ';', '(', ')', '-', '&', '*', '\\', '\'', '@', '#', '+', '=', '\xA3', '$', '%', '"', '[', ']'
        };

        static constexpr int32_t RANGE = 0x20 + 0xA3;  // 195, matches Java

        static inline std::array<char, 100> charBuffer{};
        static inline Buffer buffer{std::vector<int8_t>(100)};
    };

}