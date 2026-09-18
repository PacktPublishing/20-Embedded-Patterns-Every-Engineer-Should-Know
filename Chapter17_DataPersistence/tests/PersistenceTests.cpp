// SPDX-License-Identifier: MIT
#include "BoundedAsciiString.h"
#include "ParameterStorageImage.h"

#include <array>
#include <cassert>
#include <filesystem>
#include <iostream>

namespace
{

template<typename Parameter>
typename Parameter::ValueType get(const ch16::ParameterStore& store)
{
    typename Parameter::ValueType value{};
    assert(store.get<Parameter>(value));
    return value;
}

} // namespace

int main()
{
    using namespace ch17;

    assert(ShortString::make("weather"));
    assert(!ShortString::make("this string is too long"));

    const auto shortText = ShortString::make("weather");
    std::array<std::byte, 1 + ShortString::maxSize> stringBytes{};
    pbook::BinaryWriteStream stringWriter{stringBytes};
    assert(writeBds(stringWriter, *shortText));
    assert(stringWriter.bytesWritten() == stringBytes.size());
    ShortString decodedText;
    pbook::BinaryReadStream stringReader{stringBytes};
    assert(readBds(stringReader, decodedText));
    assert(decodedText == *shortText);

    ch16::ParameterStore original;
    assert(original.set<ch16::ReportingInterval>(
        2500,
        ch16::ParameterSource::Runtime) == ch16::UpdateResult::OK);
    assert(original.set<ch16::TemperatureLimit>(
        55.0F,
        ch16::ParameterSource::CommandLine) == ch16::UpdateResult::OK);
    assert(original.set<ch16::OperatingModeParameter>(
        ch16::OperatingMode::Service,
        ch16::ParameterSource::Runtime) == ch16::UpdateResult::OK);

    ParameterStorageImage image;
    assert(image.create(original).succeeded());

    ch16::ParameterStore restored;
    assert(image.restore(restored).succeeded());
    assert(get<ch16::ReportingInterval>(restored) == 2500);
    assert(get<ch16::TemperatureLimit>(restored) == 55.0F);
    assert(get<ch16::OperatingModeParameter>(restored) ==
           ch16::OperatingMode::Service);
    for (std::size_t index = 0; index < ch16::parameterCount; ++index)
    {
        assert(restored.source(static_cast<ch16::ParameterID>(index)) ==
               ch16::ParameterSource::PersistentStorage);
    }

    const std::array corruptionCases{
        std::pair{Corruption::BadMagic, PersistenceStatus::InvalidImageMagic},
        std::pair{Corruption::BadVersion,
                  PersistenceStatus::UnsupportedImageVersion},
        std::pair{Corruption::BadHeaderCrc,
                  PersistenceStatus::InvalidImageHeader},
        std::pair{Corruption::BadPayload,
                  PersistenceStatus::InvalidBdsMessage},
        std::pair{Corruption::UnknownParameter,
                  PersistenceStatus::UnknownParameter},
        std::pair{Corruption::DuplicateParameter,
                  PersistenceStatus::DuplicateParameter},
        std::pair{Corruption::TypeMismatch,
                  PersistenceStatus::TypeMismatch},
        std::pair{Corruption::InvalidValue,
                  PersistenceStatus::InvalidValue}
    };

    for (const auto& [corruption, expectedStatus] : corruptionCases)
    {
        auto damaged = image;
        assert(corruptImage(damaged.mutableBytes(), corruption));

        ch16::ParameterStore unchanged;
        const auto result = damaged.restore(unchanged);
        assert(result.status == expectedStatus);
        assert(get<ch16::ReportingInterval>(unchanged) ==
               ch16::ReportingInterval::defaultValue);
        assert(get<ch16::TemperatureLimit>(unchanged) ==
               ch16::TemperatureLimit::defaultValue);
        assert(get<ch16::OperatingModeParameter>(unchanged) ==
               ch16::OperatingModeParameter::defaultValue);
    }

    const auto originalImageSize = image.size();
    assert(original.set<ch16::ReportingInterval>(
        5000,
        ch16::ParameterSource::Runtime) == ch16::UpdateResult::OK);
    assert(image.update(
        ch16::ParameterID::ReportingInterval,
        original).succeeded());
    assert(image.size() == originalImageSize);

    const auto testDirectory =
        std::filesystem::temp_directory_path() / "ch17_persistence_tests";
    std::filesystem::create_directories(testDirectory);
    const auto path = testDirectory / "parameters.pimg";

    assert(writeImageFileAtomically(path, image.bytes()).succeeded());

    ParameterStorageImage fromFile;
    assert(readImageFile(path, fromFile).succeeded());
    ch16::ParameterStore fileRestored;
    assert(fromFile.restore(fileRestored).succeeded());
    assert(get<ch16::ReportingInterval>(fileRestored) == 5000);

    assert(original.set<ch16::ReportingInterval>(
        6000,
        ch16::ParameterSource::Runtime) == ch16::UpdateResult::OK);
    assert(image.update(
        ch16::ParameterID::ReportingInterval,
        original).succeeded());

    assert(writeImageFileAtomically(
        path,
        image.bytes(),
        WriteFault::BeforeRename).status == PersistenceStatus::WriteFailed);

    ParameterStorageImage retainedFile;
    assert(readImageFile(path, retainedFile).succeeded());
    ch16::ParameterStore retainedValues;
    assert(retainedFile.restore(retainedValues).succeeded());
    assert(get<ch16::ReportingInterval>(retainedValues) == 5000);

    std::filesystem::remove_all(testDirectory);
    std::cout << "All Chapter 17 persistence tests passed\n";
}
