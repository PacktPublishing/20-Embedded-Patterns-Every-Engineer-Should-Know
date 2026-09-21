// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include <ascon/aead/ascon_aead128.hpp>

#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"
#include "BoundedAsciiString.h"
#include "ImmutableByteView.h"
#include "MutableByteView.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>

namespace {

using Byte = std::uint8_t;
using Key = std::array<Byte, ascon_aead128::KEY_BYTE_LEN>;
using Nonce = std::array<Byte, ascon_aead128::NONCE_BYTE_LEN>;
using Tag = std::array<Byte, ascon_aead128::TAG_BYTE_LEN>;

using WeatherServicePassword = ch17::BoundedAsciiString<64>;

constexpr std::size_t EncodedPasswordSize =
    1 + WeatherServicePassword::maxSize;

using EncodedPassword = std::array<Byte, EncodedPasswordSize>;

// This readable metadata is authenticated along with the ciphertext.
// Bytes: format version, parameter ID (big endian), encoded payload size.
constexpr std::array<Byte, 4> PasswordMetadata{
    1,
    0x01, 0x04,
    static_cast<Byte>(EncodedPasswordSize)
};

struct EncryptedParameterRecord {
    Nonce nonce{};
    std::array<Byte, PasswordMetadata.size()> associatedData{};
    EncodedPassword ciphertext{};
    Tag tag{};
};

[[noreturn]] void fail(std::string_view message)
{
    std::cerr << "ERROR: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

WeatherServicePassword makePassword(std::string_view text)
{
    const auto password = WeatherServicePassword::make(text);
    if (!password) {
        fail("demonstration password is invalid");
    }
    return *password;
}

class BdsEncodeNode {
public:
    [[nodiscard]] std::optional<EncodedPassword>
    process(const WeatherServicePassword& password) const noexcept
    {
        EncodedPassword encoded{};
        pbook::BinaryWriteStream writer{pbook::asWritableBytes(encoded)};

        if (!ch17::writeBds(writer, password) ||
            writer.bytesWritten() != encoded.size()) {
            return std::nullopt;
        }

        return encoded;
    }
};

class BdsDecodeNode {
public:
    [[nodiscard]] std::optional<WeatherServicePassword>
    process(const EncodedPassword& encoded) noexcept
    {
        m_wasCalled = true;

        WeatherServicePassword password;
        pbook::BinaryReadStream reader{pbook::asBytes(encoded)};

        if (!ch17::readBds(reader, password) ||
            reader.bytesRead() != encoded.size()) {
            return std::nullopt;
        }

        return password;
    }

    [[nodiscard]] bool wasCalled() const noexcept
    {
        return m_wasCalled;
    }

    void reset() noexcept
    {
        m_wasCalled = false;
    }

private:
    bool m_wasCalled{};
};

class EncryptNode {
public:
    explicit EncryptNode(const Key& key) noexcept : m_key{key} {}

    [[nodiscard]] std::optional<EncryptedParameterRecord>
    process(const EncodedPassword& plaintext, const Nonce& nonce) const noexcept
    {
        EncryptedParameterRecord record{
            .nonce = nonce,
            .associatedData = PasswordMetadata,
            .ciphertext = {},
            .tag = {}
        };

        ascon_aead128::ascon_aead128_t encryptor{m_key, record.nonce};

        if (encryptor.absorb_data(record.associatedData) !=
            ascon_aead128::ascon_aead128_status_t::absorbed_data) {
            return std::nullopt;
        }

        if (encryptor.finalize_data() !=
            ascon_aead128::ascon_aead128_status_t::finalized_data_absorption_phase) {
            return std::nullopt;
        }

        if (encryptor.encrypt_plaintext(plaintext, record.ciphertext) !=
            ascon_aead128::ascon_aead128_status_t::encrypted_plaintext) {
            return std::nullopt;
        }

        if (encryptor.finalize_encrypt(record.tag) !=
            ascon_aead128::ascon_aead128_status_t::finalized_encryption_phase) {
            return std::nullopt;
        }

        return record;
    }

private:
    const Key& m_key;
};

class DecryptNode {
public:
    explicit DecryptNode(const Key& key) noexcept : m_key{key} {}

    [[nodiscard]] std::optional<EncodedPassword>
    process(const EncryptedParameterRecord& record) const noexcept
    {
        EncodedPassword candidate{};
        ascon_aead128::ascon_aead128_t decryptor{m_key, record.nonce};

        if (decryptor.absorb_data(record.associatedData) !=
            ascon_aead128::ascon_aead128_status_t::absorbed_data) {
            return std::nullopt;
        }

        if (decryptor.finalize_data() !=
            ascon_aead128::ascon_aead128_status_t::finalized_data_absorption_phase) {
            return std::nullopt;
        }

        if (decryptor.decrypt_ciphertext(record.ciphertext, candidate) !=
            ascon_aead128::ascon_aead128_status_t::decrypted_ciphertext) {
            return std::nullopt;
        }

        const bool authenticated =
            decryptor.finalize_decrypt(record.tag) ==
            ascon_aead128::ascon_aead128_status_t::decryption_success_as_tag_matches;

        if (!authenticated) {
            std::fill(candidate.begin(), candidate.end(), Byte{0});
            return std::nullopt;
        }

        return candidate;
    }

private:
    const Key& m_key;
};

[[nodiscard]] bool restorePassword(
    const EncryptedParameterRecord& record,
    const DecryptNode& decrypt,
    BdsDecodeNode& decode,
    WeatherServicePassword& livePassword)
{
    const auto encoded = decrypt.process(record);
    if (!encoded) {
        return false;
    }

    const auto decoded = decode.process(*encoded);
    if (!decoded) {
        return false;
    }

    // Commit only after authentication and decoding both succeed.
    livePassword = *decoded;
    return true;
}

} // namespace

int main()
{
    constexpr Key key{
        0x00, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f
    };

    constexpr Nonce nonce{
        0x20, 0x21, 0x22, 0x23,
        0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2a, 0x2b,
        0x2c, 0x2d, 0x2e, 0x2f
    };

    const auto configuredPassword =
        makePassword("weather-service-demo-password");

    BdsEncodeNode encode;
    EncryptNode encrypt{key};
    DecryptNode decrypt{key};
    BdsDecodeNode decode;

    std::cout << "Protecting WeatherServicePassword parameter...\n";

    const auto encoded = encode.process(configuredPassword);
    if (!encoded) {
        fail("BDS encoding failed");
    }
    std::cout << "BDS encode node produced " << encoded->size() << " bytes.\n";

    auto record = encrypt.process(*encoded, nonce);
    if (!record) {
        fail("encryption failed");
    }
    std::cout << "Encryption node produced " << record->ciphertext.size()
              << " ciphertext bytes and a " << record->tag.size()
              << "-byte authentication tag.\n";
    std::cout << "Password value not displayed.\n\n";

    auto livePassword = makePassword("initial-value");
    if (!restorePassword(*record, decrypt, decode, livePassword)) {
        fail("the unmodified password record could not be restored");
    }
    if (livePassword != configuredPassword) {
        fail("the restored password does not match the configured value");
    }

    std::cout << "Authentication succeeded.\n";
    std::cout << "BDS decode succeeded.\n";
    std::cout << "WeatherServicePassword restored (value not displayed).\n\n";

    const auto retainedPassword = makePassword("retain-this-value");
    livePassword = retainedPassword;
    decode.reset();

    constexpr std::size_t byteToModify = 12;
    record->ciphertext[byteToModify] ^= Byte{0x01};
    std::cout << "Tampering with encrypted parameter byte "
              << byteToModify << "...\n";

    const bool restored = restorePassword(*record, decrypt, decode, livePassword);

    if (restored) {
        fail("tampered ciphertext was accepted");
    }
    if (decode.wasCalled()) {
        fail("BDS decoder received unauthenticated bytes");
    }
    if (livePassword != retainedPassword) {
        fail("authentication failure modified the live password");
    }

    std::cout << "Authentication failed.\n";
    std::cout << "BDS decode node not called.\n";
    std::cout << "Live WeatherServicePassword unchanged.\n";

    return EXIT_SUCCESS;
}
