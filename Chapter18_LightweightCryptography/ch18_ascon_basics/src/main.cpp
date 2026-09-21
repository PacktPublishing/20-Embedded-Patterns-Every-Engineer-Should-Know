// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include <ascon/aead/ascon_aead128.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Byte = std::uint8_t;
using Key = std::array<Byte, ascon_aead128::KEY_BYTE_LEN>;
using Nonce = std::array<Byte, ascon_aead128::NONCE_BYTE_LEN>;
using Tag = std::array<Byte, ascon_aead128::TAG_BYTE_LEN>;

struct EncryptedMessage {
    std::vector<Byte> ciphertext;
    Tag tag{};
};

[[noreturn]] void fail(std::string_view message)
{
    std::cerr << "Ascon operation failed: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

std::vector<Byte> toBytes(std::string_view text)
{
    return {text.begin(), text.end()};
}

void printHex(std::string_view label, std::span<const Byte> bytes)
{
    std::cout << label << " (" << bytes.size() << " bytes):\n  ";

    const auto oldFlags = std::cout.flags();
    const auto oldFill = std::cout.fill();

    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index != 0 && index % 16 == 0) {
            std::cout << "\n  ";
        }

        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned int>(bytes[index]) << ' ';
    }

    std::cout.flags(oldFlags);
    std::cout.fill(oldFill);
    std::cout << "\n";
}

EncryptedMessage encrypt(
    std::span<const Byte> plaintext,
    std::span<const Byte> associatedData,
    const Key& key,
    const Nonce& nonce)
{
    EncryptedMessage result{
        .ciphertext = std::vector<Byte>(plaintext.size()),
        .tag = {}
    };

    ascon_aead128::ascon_aead128_t encryptor(key, nonce);

    if (encryptor.absorb_data(associatedData) !=
        ascon_aead128::ascon_aead128_status_t::absorbed_data) {
        fail("associated data was not accepted");
    }

    if (encryptor.finalize_data() !=
        ascon_aead128::ascon_aead128_status_t::finalized_data_absorption_phase) {
        fail("associated-data processing could not be finalized");
    }

    if (encryptor.encrypt_plaintext(plaintext, result.ciphertext) !=
        ascon_aead128::ascon_aead128_status_t::encrypted_plaintext) {
        fail("plaintext could not be encrypted");
    }

    if (encryptor.finalize_encrypt(result.tag) !=
        ascon_aead128::ascon_aead128_status_t::finalized_encryption_phase) {
        fail("encryption could not be finalized");
    }

    return result;
}

bool decrypt(
    std::span<const Byte> ciphertext,
    std::span<const Byte> associatedData,
    const Tag& tag,
    const Key& key,
    const Nonce& nonce,
    std::vector<Byte>& plaintext)
{
    // Decrypt into a temporary buffer. Do not release any plaintext until the
    // authentication tag has been verified.
    std::vector<Byte> candidate(ciphertext.size());
    ascon_aead128::ascon_aead128_t decryptor(key, nonce);

    if (decryptor.absorb_data(associatedData) !=
        ascon_aead128::ascon_aead128_status_t::absorbed_data) {
        fail("associated data was not accepted during decryption");
    }

    if (decryptor.finalize_data() !=
        ascon_aead128::ascon_aead128_status_t::finalized_data_absorption_phase) {
        fail("associated-data processing could not be finalized during decryption");
    }

    if (decryptor.decrypt_ciphertext(ciphertext, candidate) !=
        ascon_aead128::ascon_aead128_status_t::decrypted_ciphertext) {
        fail("ciphertext could not be processed");
    }

    const bool authenticated =
        decryptor.finalize_decrypt(tag) ==
        ascon_aead128::ascon_aead128_status_t::decryption_success_as_tag_matches;

    if (!authenticated) {
        std::fill(candidate.begin(), candidate.end(), Byte{0});
        plaintext.clear();
        return false;
    }

    plaintext = std::move(candidate);
    return true;
}

std::string toString(std::span<const Byte> bytes)
{
    return {bytes.begin(), bytes.end()};
}

} // namespace

int main()
{
    // Fixed values make this educational example repeatable. A production
    // system must provision secret keys securely and must never reuse a nonce
    // for two encryptions performed with the same key.
    constexpr Key key{
        0x00, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f
    };

    constexpr Nonce nonce{
        0x10, 0x11, 0x12, 0x13,
        0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b,
        0x1c, 0x1d, 0x1e, 0x1f
    };

    const auto plaintext = toBytes("The weather station is operational.");
    const auto associatedData = toBytes("Chapter 18 Lab 1");

    std::cout << "Plaintext:\n  " << toString(plaintext) << "\n\n";
    printHex("Nonce", nonce);
    printHex("Associated data", associatedData);

    auto encrypted = encrypt(plaintext, associatedData, key, nonce);
    printHex("Ciphertext", encrypted.ciphertext);
    printHex("Authentication tag", encrypted.tag);

    std::vector<Byte> recovered;
    if (!decrypt(
            encrypted.ciphertext,
            associatedData,
            encrypted.tag,
            key,
            nonce,
            recovered)) {
        fail("the unmodified message did not authenticate");
    }

    std::cout << "\nAuthentication succeeded.\n";
    std::cout << "Recovered plaintext:\n  " << toString(recovered) << "\n";

    constexpr std::size_t byteToModify = 4;
    encrypted.ciphertext[byteToModify] ^= Byte{0x01};

    std::cout << "\nTampering with ciphertext byte " << byteToModify << "...\n";
    printHex("Modified ciphertext", encrypted.ciphertext);

    if (!decrypt(
            encrypted.ciphertext,
            associatedData,
            encrypted.tag,
            key,
            nonce,
            recovered)) {
        std::cout << "\nAuthentication failed.\n";
        std::cout << "Plaintext rejected.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "ERROR: Tampered ciphertext was accepted.\n";
    return EXIT_FAILURE;
}
