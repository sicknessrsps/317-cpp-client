#pragma once
#include "PCH.h"

namespace SDL_Client {

    // Simple BigInteger implementation for RSA operations
    // This is a minimal implementation - for production use, consider a crypto library
    class BigInteger {
    public:
        BigInteger(const char* decimalString);
        BigInteger(const std::vector<int8_t>& bytes);
        BigInteger(const BigInteger& other) = default;
        BigInteger& operator=(const BigInteger& other) = default;
        BigInteger(BigInteger&& other) noexcept = default;
        BigInteger& operator=(BigInteger&& other) noexcept = default;
        ~BigInteger() = default;

        // Perform modular exponentiation: this^exponent mod modulus
        BigInteger ModPow(const BigInteger& exponent, const BigInteger& modulus) const;

        // Convert to byte array (big-endian)
        std::vector<int8_t> ToByteArray() const;

    private:
        std::vector<uint32_t> data; // Little-endian limbs (least significant first)
        bool negative = false;

        // Helper constructors
        BigInteger() = default;
        explicit BigInteger(uint32_t value);

        // Arithmetic operations
        BigInteger Add(const BigInteger& other) const;
        BigInteger Subtract(const BigInteger& other) const;
        BigInteger Multiply(const BigInteger& other) const;
        BigInteger Divide(const BigInteger& divisor, BigInteger* remainder = nullptr) const;
        BigInteger Mod(const BigInteger& modulus) const;

        // Comparison
        int Compare(const BigInteger& other) const;
        bool IsZero() const;
        bool IsOne() const;

        // Bit operations
        BigInteger ShiftLeft(size_t bits) const;
        BigInteger ShiftRight(size_t bits) const;
        bool GetBit(size_t index) const;

        // Helper methods
        void Normalize();
        static BigInteger FromBytes(const std::vector<uint8_t>& bytes);
    };

} // namespace SDL_Client