#pragma once
#include "PCH.h"

namespace SDL_Client {

    class StringUtil {
    public:
        static std::string ToAsterisks(std::string_view s);
        static int64_t ToBase37(std::string_view s);
        static std::string FromBase37(int64_t value);
        static std::string FormatName(int64_t name37);
        static std::string FormatName(std::string_view s);
        static bool EqualsIgnoreCase(std::string_view a, std::string_view b);
        static std::string ToLower(std::string_view s);
        static std::string Trim(std::string_view s);
        static int64_t HashCode(std::string_view s);
        static std::string FormatIPv4(int32_t ipv4);
    private:
        static constexpr char BASE37_TABLE[37] = {
            '_',
            'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
            '0','1','2','3','4','5','6','7','8','9'
        };
    };
}