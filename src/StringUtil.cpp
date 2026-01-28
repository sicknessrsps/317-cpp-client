#include "StringUtil.h"

namespace SDL_Client {

    std::string StringUtil::ToAsterisks(std::string_view s) {
        return std::string(s.size(), '*');
    }

    int64_t StringUtil::ToBase37(std::string_view s)
    {
        int64_t l = 0;
        const size_t len = std::min(s.length(), size_t{12});
        for (size_t i = 0; i < len; i++) {
            const char c = s[i];
            l *= 37LL;

            if (c >= 'A' && c <= 'Z') {
                l += (1 + c - 'A');
            } else if (c >= 'a' && c <= 'z') {
                l += (1 + c - 'a');
            } else if (c >= '0' && c <= '9') {
                l += (27 + c - '0');
            }
        }

        while ((l % 37LL) == 0 && l != 0) {
            l /= 37LL;
        }

        return l;
    }

    std::string StringUtil::FromBase37(int64_t value) {
        const int64_t limit = 0x5b5b57f8a98a5dd1LL; // 37^12

        if (value <= 0 || value >= limit)
            return "invalid_name";

        if (value % 37LL == 0)
            return "invalid_name";

        char tmp[12];
        int len = 0;

        while (value != 0) {
            int64_t last = value;
            value /= 37LL;
            int idx = static_cast<int>(last - value * 37LL); // remainder
            tmp[11 - len] = BASE37_TABLE[idx];
            len++;
        }

        return std::string(&tmp[12 - len], len);
    }

    std::string StringUtil::FormatName(int64_t name37)
    {
        return FormatName(FromBase37(name37));
    }

    std::string StringUtil::FormatName(std::string_view s)
    {
        if (s.empty())
            return std::string{};

        std::string out{s};  // make a mutable copy

        // Replace underscores and capitalize the following letter
        for (size_t j = 0; j < out.size(); j++) {
            if (out[j] == '_') {
                out[j] = ' ';

                if (j + 1 < out.size() &&
                    out[j + 1] >= 'a' && out[j + 1] <= 'z')
                {
                    out[j + 1] = static_cast<char>(out[j + 1] - 'a' + 'A');
                }
            }
        }

        // Capitalize first letter if lowercase
        if (out[0] >= 'a' && out[0] <= 'z') {
            out[0] = static_cast<char>(out[0] - 'a' + 'A');
        }

        return out;
    }

    bool StringUtil::EqualsIgnoreCase(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size()) return false;

        for (size_t i = 0; i < a.size(); i++) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) return false;
        }
        return true;
    }

    std::string StringUtil::ToLower(std::string_view s)
    {
        std::string result;
        result.reserve(s.size());
        for (char c : s) {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return result;
    }

    std::string StringUtil::Trim(std::string_view s)
    {
        if (s.empty())
            return std::string{};

        // Find first non-whitespace character
        auto start = s.find_first_not_of(" \t\n\r\f\v");
        if (start == std::string_view::npos)
            return std::string{}; // String is all whitespace

        // Find last non-whitespace character
        auto end = s.find_last_not_of(" \t\n\r\f\v");

        return std::string{s.substr(start, end - start + 1)};
    }

    int64_t StringUtil::HashCode(std::string_view s)
    {
        int64_t hash = 0;
        for (char ch : s) {
            char c = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            hash = ((hash * 61) + static_cast<int64_t>(c)) - 32;
            hash = (hash + (hash >> 56)) & 0xffffffffffffffLL;
        }
        return hash;
    }

    std::string StringUtil::FormatIPv4(int32_t ipv4)
    {
        return std::format("{}.{}.{}.{}",
            (ipv4 >> 24) & 0xFF,
            (ipv4 >> 16) & 0xFF,
            (ipv4 >> 8) & 0xFF,
            ipv4 & 0xFF);
    }
}
