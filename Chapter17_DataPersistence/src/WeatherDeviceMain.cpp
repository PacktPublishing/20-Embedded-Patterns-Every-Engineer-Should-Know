// SPDX-License-Identifier: MIT
#include "ParameterStorageImage.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

#include "ParameterRegistry.h"

namespace
{

void usage(const char* program)
{
    std::cout
        << "Usage: " << program << " [options]\n"
        << "  --image PATH                 Image file (default parameters.pimg)\n"
        << "  --no-restore                 Start from compiled defaults\n"
        << "  --set-reporting-ms N         Set 100..60000 ms\n"
        << "  --set-temperature-limit N    Set -40..125 degrees C\n"
        << "  --set-operating-mode MODE    Set normal, service, or safe\n"
        << "  --save                       Atomically persist the image\n"
        << "  --simulate-write-failure     Fail before atomic rename\n"
        << "  --help                       Show this help\n";
}

} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path path{"parameters.pimg"};
    bool restore = true;
    bool save = false;
    bool simulateWriteFailure = false;
    std::optional<std::string_view> reporting;
    std::optional<std::string_view> temperature;
    std::optional<std::string_view> mode;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view option{argv[index]};
        const auto argument = [&]() -> std::optional<std::string_view>
        {
            if (index + 1 >= argc)
            {
                return std::nullopt;
            }
            return std::string_view{argv[++index]};
        };

        if (option == "--help")
        {
            usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (option == "--no-restore")
        {
            restore = false;
            continue;
        }
        if (option == "--save")
        {
            save = true;
            continue;
        }
        if (option == "--simulate-write-failure")
        {
            simulateWriteFailure = true;
            continue;
        }
        if (option == "--image")
        {
            const auto value = argument();
            if (!value)
            {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
            path = *value;
        }
        else if (option == "--set-reporting-ms")
        {
            reporting = argument();
            if (!reporting)
            {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
        else if (option == "--set-temperature-limit")
        {
            temperature = argument();
            if (!temperature)
            {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
        else if (option == "--set-operating-mode")
        {
            mode = argument();
            if (!mode)
            {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
        else
        {
            std::cerr << "Unknown option: " << option << '\n';
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    ch16::ParameterStore parameters;
    ch17::ParameterStorageImage image;
    auto result = image.create(parameters);
    if (!result.succeeded())
    {
        std::cerr << "Could not create the in-memory image: "
                  << ch17::toString(result.status) << '\n';
        return EXIT_FAILURE;
    }

    if (restore)
    {
        ch17::ParameterStorageImage stored;
        result = ch17::readImageFile(path, stored);
        if (result.status == ch17::PersistenceStatus::ImageNotFound)
        {
            std::cout << "restore=ImageNotFound; using compiled defaults\n";
        }
        else if (!result.succeeded())
        {
            std::cerr << "restore=" << ch17::toString(result.status)
                      << "; defaults retained\n";
        }
        else
        {
            result = stored.restore(parameters);
            if (!result.succeeded())
            {
                std::cerr << "restore=" << ch17::toString(result.status)
                          << " records_processed=" << result.recordsProcessed;
                if (result.parameterId)
                {
                    std::cerr << " parameter="
                              << ch17::toString(*result.parameterId);
                }
                std::cerr << "; active parameters unchanged\n";
            }
            else
            {
                image = stored;
                std::cout << "restore=Ok records_processed="
                          << result.recordsProcessed << '\n';
            }
        }
    }

    const auto apply = [&](ch16::ParameterID id, std::string_view text)
    {
        const auto updateResult = parameters.set(
            id,
            text,
            ch16::ParameterSource::CommandLine);
        if (updateResult != ch16::UpdateResult::OK)
        {
            std::cerr << "Parameter update rejected for "
                      << ch17::toString(id) << '\n';
            return false;
        }

        const auto imageResult = image.update(id, parameters);
        if (!imageResult.succeeded())
        {
            std::cerr << "Image update failed for " << ch17::toString(id)
                      << ": " << ch17::toString(imageResult.status) << '\n';
            return false;
        }
        return true;
    };

    if (reporting &&
        !apply(ch16::ParameterID::ReportingInterval, *reporting))
    {
        return EXIT_FAILURE;
    }
    if (temperature &&
        !apply(ch16::ParameterID::TemperatureLimit, *temperature))
    {
        return EXIT_FAILURE;
    }
    if (mode && !apply(ch16::ParameterID::OperatingMode, *mode))
    {
        return EXIT_FAILURE;
    }

    if (save)
    {
        std::array<std::byte, ch17::MaxStorageImageSize> snapshot{};
        std::copy(image.bytes().begin(), image.bytes().end(), snapshot.begin());

        result = ch17::writeImageFileAtomically(
            path,
            std::span{snapshot}.first(image.size()),
            simulateWriteFailure
                ? ch17::WriteFault::BeforeRename
                : ch17::WriteFault::None);

        std::cout << "save=" << ch17::toString(result.status) << '\n';
        if (!result.succeeded())
        {
            return EXIT_FAILURE;
        }
    }

    ch17::printParameters(parameters);
    std::cout << "PARAMETERS_READY\n";
    return EXIT_SUCCESS;
}
