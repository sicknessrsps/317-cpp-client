#include "BigInteger.h"
#include <algorithm>
#include <stdexcept>

namespace SDL_Client {

    BigInteger::BigInteger(uint32_t value) : negative(false) {
        if (value == 0) {
            data.push_back(0);
        } else {
            data.push_back(value);
        }
    }

    BigInteger::BigInteger(const char* decimalString) : negative(false) {
        if (!decimalString || decimalString[0] == '\0') {
            data.push_back(0);
            return;
        }

        // Parse decimal string
        std::string str(decimalString);
        if (str[0] == '-') {
            negative = true;
            str = str.substr(1);
        }

        // Start with zero
        data.push_back(0);

        // Process each digit
        for (char c : str) {
            if (c < '0' || c > '9') {
                throw std::invalid_argument("Invalid decimal string");
            }

            // Multiply by 10 and add digit
            uint64_t carry = c - '0';
            for (size_t i = 0; i < data.size(); ++i) {
                uint64_t temp = static_cast<uint64_t>(data[i]) * 10 + carry;
                data[i] = static_cast<uint32_t>(temp);
                carry = temp >> 32;
            }
            if (carry > 0) {
                data.push_back(static_cast<uint32_t>(carry));
            }
        }

        Normalize();
    }

    BigInteger::BigInteger(const std::vector<int8_t>& bytes) : negative(false) {
        if (bytes.empty()) {
            data.push_back(0);
            return;
        }

        // Convert signed bytes to unsigned
        std::vector<uint8_t> unsignedBytes(bytes.size());
        for (size_t i = 0; i < bytes.size(); ++i) {
            unsignedBytes[i] = static_cast<uint8_t>(bytes[i]);
        }

        // Big-endian byte array to little-endian limbs
        data.clear();
        for (size_t i = 0; i < unsignedBytes.size(); i += 4) {
            uint32_t limb = 0;
            for (size_t j = 0; j < 4 && i + j < unsignedBytes.size(); ++j) {
                size_t byteIndex = unsignedBytes.size() - 1 - i - j;
                limb |= static_cast<uint32_t>(unsignedBytes[byteIndex]) << (j * 8);
            }
            data.push_back(limb);
        }

        Normalize();
    }

    std::vector<int8_t> BigInteger::ToByteArray() const {
        if (IsZero()) {
            return {0};
        }

        // Calculate number of bytes needed
        size_t numBytes = 0;
        for (size_t i = data.size(); i > 0; --i) {
            if (data[i - 1] != 0) {
                uint32_t limb = data[i - 1];
                numBytes = (i - 1) * 4;
                while (limb > 0) {
                    numBytes++;
                    limb >>= 8;
                }
                break;
            }
        }

        if (numBytes == 0) {
            return {0};
        }

        // Convert limbs to big-endian bytes
        std::vector<uint8_t> unsignedBytes(numBytes);
        for (size_t i = 0; i < numBytes; ++i) {
            size_t limbIndex = i / 4;
            size_t byteInLimb = i % 4;
            if (limbIndex < data.size()) {
                unsignedBytes[numBytes - 1 - i] = static_cast<uint8_t>(data[limbIndex] >> (byteInLimb * 8));
            }
        }

        // Java BigInteger.toByteArray() adds a leading 0x00 byte if the high bit is set
        // to ensure the number is interpreted as positive (two's complement)
        bool needsSignByte = (unsignedBytes[0] & 0x80) != 0;

        // Convert to signed bytes
        std::vector<int8_t> result;
        if (needsSignByte) {
            result.resize(numBytes + 1);
            result[0] = 0; // Sign byte
            for (size_t i = 0; i < numBytes; ++i) {
                result[i + 1] = static_cast<int8_t>(unsignedBytes[i]);
            }
        } else {
            result.resize(numBytes);
            for (size_t i = 0; i < numBytes; ++i) {
                result[i] = static_cast<int8_t>(unsignedBytes[i]);
            }
        }

        return result;
    }

    void BigInteger::Normalize() {
        while (data.size() > 1 && data.back() == 0) {
            data.pop_back();
        }
        if (data.empty()) {
            data.push_back(0);
        }
        if (IsZero()) {
            negative = false;
        }
    }

    bool BigInteger::IsZero() const {
        return data.size() == 1 && data[0] == 0;
    }

    bool BigInteger::IsOne() const {
        return !negative && data.size() == 1 && data[0] == 1;
    }

    int BigInteger::Compare(const BigInteger& other) const {
        if (negative != other.negative) {
            return negative ? -1 : 1;
        }

        if (data.size() != other.data.size()) {
            int result = data.size() > other.data.size() ? 1 : -1;
            return negative ? -result : result;
        }

        for (size_t i = data.size(); i > 0; --i) {
            if (data[i - 1] != other.data[i - 1]) {
                int result = data[i - 1] > other.data[i - 1] ? 1 : -1;
                return negative ? -result : result;
            }
        }

        return 0;
    }

    BigInteger BigInteger::Add(const BigInteger& other) const {
        if (negative == other.negative) {
            BigInteger result;
            result.negative = negative;
            result.data.clear();

            size_t maxSize = std::max(data.size(), other.data.size());
            uint64_t carry = 0;

            for (size_t i = 0; i < maxSize || carry; ++i) {
                uint64_t sum = carry;
                if (i < data.size()) sum += data[i];
                if (i < other.data.size()) sum += other.data[i];

                result.data.push_back(static_cast<uint32_t>(sum));
                carry = sum >> 32;
            }

            result.Normalize();
            return result;
        } else {
            return negative ? other.Subtract(*this) : Subtract(other);
        }
    }

    BigInteger BigInteger::Subtract(const BigInteger& other) const {
        if (negative != other.negative) {
            BigInteger temp = other;
            temp.negative = !temp.negative;
            return Add(temp);
        }

        if (Compare(other) < 0) {
            BigInteger result = other.Subtract(*this);
            result.negative = !result.negative;
            return result;
        }

        BigInteger result;
        result.negative = negative;
        result.data.clear();

        int64_t borrow = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            int64_t diff = static_cast<int64_t>(data[i]) - borrow;
            if (i < other.data.size()) {
                diff -= other.data[i];
            }

            if (diff < 0) {
                diff += 0x100000000LL;
                borrow = 1;
            } else {
                borrow = 0;
            }

            result.data.push_back(static_cast<uint32_t>(diff));
        }

        result.Normalize();
        return result;
    }

    BigInteger BigInteger::Multiply(const BigInteger& other) const {
        BigInteger result;
        result.data.assign(data.size() + other.data.size(), 0);
        result.negative = negative != other.negative;

        for (size_t i = 0; i < data.size(); ++i) {
            uint64_t carry = 0;
            for (size_t j = 0; j < other.data.size() || carry; ++j) {
                uint64_t product = result.data[i + j] + carry;
                if (j < other.data.size()) {
                    product += static_cast<uint64_t>(data[i]) * other.data[j];
                }
                result.data[i + j] = static_cast<uint32_t>(product);
                carry = product >> 32;
            }
        }

        result.Normalize();
        return result;
    }

    BigInteger BigInteger::Divide(const BigInteger& divisor, BigInteger* remainder) const {
        if (divisor.IsZero()) {
            throw std::domain_error("Division by zero");
        }

        if (Compare(divisor) < 0) {
            if (remainder) *remainder = *this;
            return BigInteger(0u);
        }

        BigInteger quotient(0u);
        BigInteger rem(0u);

        // Long division algorithm
        for (int i = static_cast<int>(data.size()) * 32 - 1; i >= 0; --i) {
            rem = rem.ShiftLeft(1);
            if (GetBit(i)) {
                rem.data[0] |= 1;
            }

            if (rem.Compare(divisor) >= 0) {
                rem = rem.Subtract(divisor);
                if (static_cast<size_t>(i) / 32 >= quotient.data.size()) {
                    quotient.data.resize(i / 32 + 1, 0);
                }
                quotient.data[i / 32] |= 1U << (i % 32);
            }
        }

        quotient.Normalize();
        if (remainder) *remainder = rem;
        return quotient;
    }

    BigInteger BigInteger::Mod(const BigInteger& modulus) const {
        BigInteger remainder;
        Divide(modulus, &remainder);
        return remainder;
    }

    bool BigInteger::GetBit(size_t index) const {
        size_t limbIndex = index / 32;
        size_t bitIndex = index % 32;
        if (limbIndex >= data.size()) return false;
        return (data[limbIndex] & (1U << bitIndex)) != 0;
    }

    BigInteger BigInteger::ShiftLeft(size_t bits) const {
        if (bits == 0 || IsZero()) return *this;

        BigInteger result;
        result.negative = negative;
        result.data.clear();

        size_t limbShift = bits / 32;
        size_t bitShift = bits % 32;

        result.data.resize(data.size() + limbShift + (bitShift ? 1 : 0), 0);

        if (bitShift == 0) {
            for (size_t i = 0; i < data.size(); ++i) {
                result.data[i + limbShift] = data[i];
            }
        } else {
            uint32_t carry = 0;
            for (size_t i = 0; i < data.size(); ++i) {
                uint64_t temp = (static_cast<uint64_t>(data[i]) << bitShift) | carry;
                result.data[i + limbShift] = static_cast<uint32_t>(temp);
                carry = static_cast<uint32_t>(temp >> 32);
            }
            if (carry) {
                result.data[data.size() + limbShift] = carry;
            }
        }

        result.Normalize();
        return result;
    }

    BigInteger BigInteger::ShiftRight(size_t bits) const {
        if (bits == 0 || IsZero()) return *this;

        size_t limbShift = bits / 32;
        if (limbShift >= data.size()) return BigInteger(0u);

        size_t bitShift = bits % 32;

        BigInteger result;
        result.negative = negative;
        result.data.clear();
        result.data.resize(data.size() - limbShift);

        if (bitShift == 0) {
            for (size_t i = 0; i < result.data.size(); ++i) {
                result.data[i] = data[i + limbShift];
            }
        } else {
            for (size_t i = 0; i < result.data.size(); ++i) {
                result.data[i] = data[i + limbShift] >> bitShift;
                if (i + limbShift + 1 < data.size()) {
                    result.data[i] |= data[i + limbShift + 1] << (32 - bitShift);
                }
            }
        }

        result.Normalize();
        return result;
    }

    BigInteger BigInteger::ModPow(const BigInteger& exponent, const BigInteger& modulus) const {
        if (modulus.IsZero()) {
            throw std::domain_error("Modulus cannot be zero");
        }

        // Right-to-left binary exponentiation
        BigInteger result(1u);
        BigInteger base = Mod(modulus);
        BigInteger exp = exponent;

        while (!exp.IsZero()) {
            if (exp.data[0] & 1) {
                result = result.Multiply(base).Mod(modulus);
            }
            base = base.Multiply(base).Mod(modulus);
            exp = exp.ShiftRight(1);
        }

        return result;
    }

} // namespace SDL_Client