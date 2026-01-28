#include "ChatCompression.h"

namespace SDL_Client {

    std::string ChatCompression::Unpack(int32_t length, Buffer& in) {
        int32_t pos = 0;
        int32_t carry = -1;

        for (int32_t i = 0; i < length; i++) {
            int32_t value = in. ReadU8();
            int32_t nibble = (value >> 4) & 0b1111;

            if (carry == -1) {
                if (nibble < 13) {
                    charBuffer[pos++] = TABLE[nibble];
                } else {
                    carry = nibble;
                }
            } else {
                charBuffer[pos++] = TABLE[((carry << 4) + nibble) - RANGE];
                carry = -1;
            }

            nibble = value & 0xF;

            if (carry == -1) {
                if (nibble < 13) {
                    charBuffer[pos++] = TABLE[nibble];
                } else {
                    carry = nibble;
                }
            } else {
                charBuffer[pos++] = TABLE[((carry << 4) + nibble) - RANGE];
                carry = -1;
            }
        }

        // basic sentence casing
        // "hi. i'm a line." -> "Hi.  I'm a line."
        bool uppercase = true;
        for (int32_t i = 0; i < pos; i++) {
            char c = charBuffer[i];
            if (uppercase && (c >= 'a') && (c <= 'z')) {
                charBuffer[i] -= 'a' - 'A';
                uppercase = false;
            }

            if ((c == '.') || (c == '!') || (c == '?')) {
                uppercase = true;
            }
        }

        return std::string(charBuffer.data(), pos);
    }

    void ChatCompression::Pack(const std::string& in, Buffer& out) {
        std::string input = in;

        if (input.length() > 80) {
            input = input. substr(0, 80);
        }

        // toLowerCase
        std::ranges::transform(input, input. begin(),
                               [](unsigned char c) { return std::tolower(c); });

        int32_t carry = -1;
        for (size_t i = 0; i < input. length(); i++) {
            char ch = input[i];

            int32_t index = 0;
            // FIX: Use std::size(TABLE) or define TABLE_SIZE constant
            // sizeof(TABLE) might include padding or have issues with constexpr arrays
            for (int32_t j = 0; j < static_cast<int32_t>(std::size(TABLE)); j++) {
                if (ch == TABLE[j]) {
                    index = j;
                    break;
                }
            }

            if (index > 12) {
                index += RANGE;
            }

            if (carry == -1) {
                if (index < 13) {
                    carry = index;
                } else {
                    out.Write8(index);
                }
            } else if (index < 13) {
                out.Write8((carry << 4) + index);
                carry = -1;
            } else {
                out.Write8((carry << 4) + (index >> 4));
                carry = index & 0xF;
            }
        }

        if (carry != -1) {
            out.Write8(carry << 4);
        }
    }

    std::string ChatCompression::Format(const std::string& string) {
        buffer.position = 0;
        Pack(string, buffer);
        int32_t length = buffer.position;
        buffer.position = 0;
        return Unpack(length, buffer);
    }

}