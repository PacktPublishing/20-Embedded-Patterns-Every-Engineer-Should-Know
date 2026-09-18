// SPDX-License-Identifier: MIT
#include "ParameterStorageImage.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

namespace
{

void usage(const char* program)
{
    std::cout
        << "Usage: " << program
        << " IMAGE [--corrupt MODE --output PATH]\n"
        << "Modes: bad-magic, bad-version, bad-header-crc, truncate,\n"
        << "       bad-payload, unknown-parameter, duplicate-parameter,\n"
        << "       type-mismatch, invalid-value\n";
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::filesystem::path source{argv[1]};
    std::optional<ch17::Corruption> corruption;
    std::filesystem::path output;

    for (int index = 2; index < argc; ++index)
    {
        const std::string_view option{argv[index]};
        if (option == "--corrupt" && index + 1 < argc)
        {
            corruption = ch17::parseCorruption(argv[++index]);
            if (!corruption)
            {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
        else if (option == "--output" && index + 1 < argc)
        {
            output = argv[++index];
        }
        else
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    ch17::ParameterStorageImage image;
    auto result = ch17::readImageFile(source, image);
    if (!result.succeeded())
    {
        std::cerr << "read=" << ch17::toString(result.status) << '\n';
        return EXIT_FAILURE;
    }

    if (corruption)
    {
        if (output.empty())
        {
            std::cerr << "--corrupt requires --output; the source is never modified\n";
            return EXIT_FAILURE;
        }

        std::size_t outputSize = image.size();
        if (*corruption == ch17::Corruption::Truncate)
        {
            if (outputSize <= ch17::StorageImageHeaderWireSize)
            {
                return EXIT_FAILURE;
            }
            --outputSize;
        }
        else if (!ch17::corruptImage(image.mutableBytes(), *corruption))
        {
            std::cerr << "Could not apply corruption\n";
            return EXIT_FAILURE;
        }

        result = ch17::writeImageFileAtomically(
            output,
            image.bytes().first(outputSize));
        std::cout << "corrupt_write=" << ch17::toString(result.status) << '\n';
        return result.succeeded() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    result = ch17::inspectImage(image.bytes());
    std::cout << "image.validation=" << ch17::toString(result.status) << '\n';
    return result.succeeded() ? EXIT_SUCCESS : EXIT_FAILURE;
}
