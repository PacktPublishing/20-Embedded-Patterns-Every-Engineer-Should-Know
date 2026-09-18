// SPDX-License-Identifier: MIT
#include "ParameterStorageImage.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <system_error>
#include <unistd.h>

#include "BinaryReadStream.h"
#include "BinaryWriteStream.h"
#include "BdsCommon.h"
#include "MessageFrame.h"
#include "MessageHeader.h"

namespace ch17
{
namespace
{

constexpr Endianness ImageWireEndianness = Endianness::Little;

template<typename Parameter>
constexpr ParameterMessageType messageTypeFor() noexcept;

template<>
constexpr ParameterMessageType messageTypeFor<ch16::ReportingInterval>() noexcept
{
    return ParameterMessageType::UInt32;
}

template<>
constexpr ParameterMessageType messageTypeFor<ch16::TemperatureLimit>() noexcept
{
    return ParameterMessageType::Float32;
}

template<>
constexpr ParameterMessageType messageTypeFor<ch16::OperatingModeParameter>() noexcept
{
    return ParameterMessageType::Enum8;
}

constexpr ParameterMessageType expectedMessageType(
    ch16::ParameterID id) noexcept
{
    switch (id)
    {
    case ch16::ParameterID::ReportingInterval:
        return messageTypeFor<ch16::ReportingInterval>();
    case ch16::ParameterID::TemperatureLimit:
        return messageTypeFor<ch16::TemperatureLimit>();
    case ch16::ParameterID::OperatingMode:
        return messageTypeFor<ch16::OperatingModeParameter>();
    case ch16::ParameterID::Count:
        break;
    }
    return ParameterMessageType::UInt32;
}

constexpr std::size_t encodedPayloadSize(ch16::ParameterID id) noexcept
{
    switch (id)
    {
    case ch16::ParameterID::ReportingInterval:
        return ch16::ReportingInterval::encodedSize;
    case ch16::ParameterID::TemperatureLimit:
        return ch16::TemperatureLimit::encodedSize;
    case ch16::ParameterID::OperatingMode:
        return ch16::OperatingModeParameter::encodedSize;
    case ch16::ParameterID::Count:
        break;
    }
    return 0;
}

template<typename Parameter>
bool encodeParameterPayload(
    const ch16::ParameterStore& store,
    std::span<std::byte> destination) noexcept
{
    if (destination.size() != Parameter::encodedSize)
    {
        return false;
    }

    typename Parameter::ValueType value{};
    if (!store.get<Parameter>(value))
    {
        return false;
    }

    pbook::BinaryWriteStream writer(
        pbook::MutableByteView(destination.data(), destination.size()),
        ch16::ParameterImageEndianness);
    Parameter::encode(writer, value);

    return writer.ok() && writer.bytesWritten() == Parameter::encodedSize;
}

bool encodePayload(
    ch16::ParameterID id,
    const ch16::ParameterStore& store,
    std::span<std::byte> destination) noexcept
{
    switch (id)
    {
    case ch16::ParameterID::ReportingInterval:
        return encodeParameterPayload<ch16::ReportingInterval>(store, destination);
    case ch16::ParameterID::TemperatureLimit:
        return encodeParameterPayload<ch16::TemperatureLimit>(store, destination);
    case ch16::ParameterID::OperatingMode:
        return encodeParameterPayload<ch16::OperatingModeParameter>(store, destination);
    case ch16::ParameterID::Count:
        return false;
    }
    return false;
}

template<typename Parameter>
PersistenceStatus decodeAndStage(
    pbook::ImmutableByteView payload,
    ch16::ParameterStore& staged) noexcept
{
    if (payload.size() != Parameter::encodedSize)
    {
        return PersistenceStatus::TypeMismatch;
    }

    pbook::BinaryReadStream reader(payload, ch16::ParameterImageEndianness);
    typename Parameter::ValueType value{};
    Parameter::decode(reader, value);

    if (!reader.ok() || reader.bytesRead() != Parameter::encodedSize)
    {
        return PersistenceStatus::InvalidBdsMessage;
    }

    const auto update = staged.set<Parameter>(
        value,
        ch16::ParameterSource::PersistentStorage);

    switch (update)
    {
    case ch16::UpdateResult::OK:
    case ch16::UpdateResult::LOWER_PRECEDENCE:
        return PersistenceStatus::Ok;
    case ch16::UpdateResult::UNKNOWN_PARAMETER:
        return PersistenceStatus::UnknownParameter;
    case ch16::UpdateResult::INVALID_VALUE:
    case ch16::UpdateResult::OUT_OF_RANGE:
        return PersistenceStatus::InvalidValue;
    case ch16::UpdateResult::READ_ONLY:
    case ch16::UpdateResult::NOT_RUNTIME_WRITABLE:
        return PersistenceStatus::InvalidValue;
    }
    return PersistenceStatus::InvalidValue;
}

PersistenceStatus decodeAndStage(
    ch16::ParameterID id,
    const MessageHeaderV1& header,
    pbook::ImmutableByteView payload,
    ch16::ParameterStore& staged) noexcept
{
    if (header.serviceId != ParameterPersistenceServiceId ||
        header.messageType !=
            static_cast<std::uint16_t>(expectedMessageType(id)) ||
        header.payloadEndian !=
            static_cast<std::uint8_t>(ch16::ParameterImageEndianness))
    {
        return PersistenceStatus::TypeMismatch;
    }

    switch (id)
    {
    case ch16::ParameterID::ReportingInterval:
        return decodeAndStage<ch16::ReportingInterval>(payload, staged);
    case ch16::ParameterID::TemperatureLimit:
        return decodeAndStage<ch16::TemperatureLimit>(payload, staged);
    case ch16::ParameterID::OperatingMode:
        return decodeAndStage<ch16::OperatingModeParameter>(payload, staged);
    case ch16::ParameterID::Count:
        return PersistenceStatus::UnknownParameter;
    }
    return PersistenceStatus::UnknownParameter;
}

bool encodeImageHeader(
    std::span<std::byte> destination,
    std::uint16_t recordCount) noexcept
{
    if (destination.size() < StorageImageHeaderWireSize)
    {
        return false;
    }

    std::copy(
        StorageImageMagic.begin(),
        StorageImageMagic.end(),
        destination.begin());

    pbook::BinaryWriteStream writer(
        pbook::MutableByteView(
            destination.data() + StorageImageMagic.size(),
            StorageImageHeaderWireSize - StorageImageMagic.size()),
        ImageWireEndianness);

    writer.writeUInt16(StorageImageVersion)
        .writeUInt16(recordCount)
        .writeUInt16(0u);

    if (!writer.ok())
    {
        return false;
    }

    const auto crc = crc16CcittFalse(
        pbook::ImmutableByteView(destination.data(), 8));

    pbook::BinaryWriteStream crcWriter(
        pbook::MutableByteView(destination.data() + 8, 2),
        ImageWireEndianness);
    crcWriter.writeUInt16(crc);
    return crcWriter.ok();
}

PersistenceResult decodeImageHeader(
    std::span<const std::byte> image,
    StorageImageHeader& header) noexcept
{
    if (image.size() < StorageImageHeaderWireSize)
    {
        return {PersistenceStatus::InvalidImageHeader};
    }

    std::copy_n(image.begin(), 4, header.magic.begin());
    if (header.magic != StorageImageMagic)
    {
        return {PersistenceStatus::InvalidImageMagic};
    }

    pbook::BinaryReadStream reader(
        pbook::ImmutableByteView(image.data() + 4, 6),
        ImageWireEndianness);
    reader.readUInt16(header.version)
        .readUInt16(header.recordCount)
        .readUInt16(header.headerCrc);

    if (!reader.ok())
    {
        return {PersistenceStatus::InvalidImageHeader};
    }

    if (header.version != StorageImageVersion)
    {
        return {PersistenceStatus::UnsupportedImageVersion};
    }

    const auto expectedCrc = crc16CcittFalse(
        pbook::ImmutableByteView(image.data(), 8));
    if (header.headerCrc != expectedCrc)
    {
        return {PersistenceStatus::InvalidImageHeader};
    }

    if (header.recordCount != ch16::parameterCount)
    {
        return {PersistenceStatus::InvalidRecordCount};
    }

    return {};
}

std::optional<std::uint16_t> readUInt16(
    std::span<const std::byte> bytes,
    std::size_t offset) noexcept
{
    if (offset + 2 > bytes.size())
    {
        return std::nullopt;
    }

    std::uint16_t value{};
    pbook::BinaryReadStream reader(
        pbook::ImmutableByteView(bytes.data() + offset, 2),
        ImageWireEndianness);
    reader.readUInt16(value);
    return reader.ok() ? std::optional{value} : std::nullopt;
}

bool writeUInt16(
    std::span<std::byte> bytes,
    std::size_t offset,
    std::uint16_t value) noexcept
{
    if (offset + 2 > bytes.size())
    {
        return false;
    }

    pbook::BinaryWriteStream writer(
        pbook::MutableByteView(bytes.data() + offset, 2),
        ImageWireEndianness);
    writer.writeUInt16(value);
    return writer.ok();
}

template<typename Callback>
PersistenceResult walkRecords(
    std::span<const std::byte> image,
    Callback&& callback) noexcept
{
    StorageImageHeader imageHeader;
    auto result = decodeImageHeader(image, imageHeader);
    if (!result.succeeded())
    {
        return result;
    }

    std::array<bool, ch16::parameterCount> seen{};
    std::size_t offset = StorageImageHeaderWireSize;

    for (std::size_t recordIndex = 0;
         recordIndex < imageHeader.recordCount;
         ++recordIndex)
    {
        const auto rawId = readUInt16(image, offset);
        if (!rawId)
        {
            return {PersistenceStatus::ReadFailed, recordIndex};
        }
        offset += 2;

        if (!ch16::isValidParameterId(*rawId))
        {
            return {PersistenceStatus::UnknownParameter, recordIndex};
        }

        const auto id = static_cast<ch16::ParameterID>(*rawId);
        if (seen[*rawId])
        {
            return {PersistenceStatus::DuplicateParameter, recordIndex, id};
        }
        seen[*rawId] = true;

        if (offset + MessageHeaderV1WireSize > image.size())
        {
            return {PersistenceStatus::InvalidBdsMessage, recordIndex, id};
        }

        MessageHeaderV1 header;
        pbook::BinaryReadStream headerReader(
            pbook::ImmutableByteView(
                image.data() + offset,
                MessageHeaderV1WireSize),
            HeaderWireEndianness);
        readHeaderV1(headerReader, header);

        if (!headerReader.ok() ||
            header.payloadSize > MaxStorageImageSize - MessageHeaderV1WireSize)
        {
            return {PersistenceStatus::InvalidBdsMessage, recordIndex, id};
        }

        const auto frameSize = MessageHeaderV1WireSize +
            static_cast<std::size_t>(header.payloadSize);
        if (offset + frameSize > image.size())
        {
            return {PersistenceStatus::InvalidBdsMessage, recordIndex, id};
        }

        const auto frame = image.subspan(offset, frameSize);
        pbook::ImmutableByteView payload;
        const auto frameStatus = readFrameV1(
            pbook::ImmutableByteView(frame.data(), frame.size()),
            header,
            payload);
        if (frameStatus != MessageFrameStatus::Ok)
        {
            return {PersistenceStatus::InvalidBdsMessage, recordIndex, id};
        }

        const auto status = callback(
            ParameterRecordView{id, frame},
            header,
            payload);
        if (status != PersistenceStatus::Ok)
        {
            return {status, recordIndex, id};
        }

        offset += frameSize;
        result.recordsProcessed = recordIndex + 1;
    }

    if (offset != image.size())
    {
        return {PersistenceStatus::InvalidRecordCount, result.recordsProcessed};
    }

    return result;
}

std::optional<std::pair<std::size_t, std::size_t>> findRecord(
    std::span<const std::byte> image,
    ch16::ParameterID wanted) noexcept
{
    std::size_t foundOffset{};
    std::size_t foundSize{};

    const auto result = walkRecords(
        image,
        [&](ParameterRecordView record,
            const MessageHeaderV1&,
            pbook::ImmutableByteView)
        {
            if (record.parameterId == wanted)
            {
                foundOffset = static_cast<std::size_t>(
                    record.bdsMessage.data() - image.data());
                foundSize = record.bdsMessage.size();
            }
            return PersistenceStatus::Ok;
        });

    if (!result.succeeded() || foundSize == 0)
    {
        return std::nullopt;
    }
    return std::pair{foundOffset, foundSize};
}

bool makeFrame(
    ch16::ParameterID id,
    const ch16::ParameterStore& parameters,
    std::span<std::byte> destination,
    std::size_t& bytesWritten) noexcept
{
    std::array<std::byte, 256> payloadStorage{};
    auto payload = std::span{payloadStorage}.first(encodedPayloadSize(id));
    if (!encodePayload(id, parameters, payload))
    {
        return false;
    }

    MessageHeaderV1 header;
    header.serviceId = ParameterPersistenceServiceId;
    header.messageType = static_cast<std::uint16_t>(expectedMessageType(id));
    header.payloadEndian =
        static_cast<std::uint8_t>(ch16::ParameterImageEndianness);

    return writeFrameV1(
        pbook::MutableByteView(destination.data(), destination.size()),
        header,
        pbook::ImmutableByteView(payload.data(), payload.size()),
        bytesWritten) == MessageFrameStatus::Ok;
}

bool writeAll(
    int descriptor,
    std::span<const std::byte> bytes) noexcept
{
    std::size_t written{};
    while (written < bytes.size())
    {
        const auto result = ::write(
            descriptor,
            bytes.data() + written,
            bytes.size() - written);
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        if (result <= 0)
        {
            return false;
        }
        written += static_cast<std::size_t>(result);
    }
    return true;
}

} // namespace

PersistenceResult ParameterStorageImage::create(
    const ch16::ParameterStore& parameters) noexcept
{
    size_ = StorageImageHeaderWireSize;
    if (!encodeImageHeader(
            data_,
            static_cast<std::uint16_t>(ch16::parameterCount)))
    {
        return {PersistenceStatus::InvalidImageHeader};
    }

    for (std::size_t rawId = 0; rawId < ch16::parameterCount; ++rawId)
    {
        const auto id = static_cast<ch16::ParameterID>(rawId);
        const auto maximumFrameSize = MessageHeaderV1WireSize +
            encodedPayloadSize(id);
        const auto required = 2 + maximumFrameSize;

        if (size_ + required > data_.size())
        {
            return {PersistenceStatus::ImageFull, rawId, id};
        }

        writeUInt16(data_, size_, static_cast<std::uint16_t>(id));
        size_ += 2;

        std::size_t frameSize{};
        if (!makeFrame(
                id,
                parameters,
                std::span{data_}.subspan(size_, maximumFrameSize),
                frameSize) ||
            frameSize != maximumFrameSize)
        {
            return {PersistenceStatus::InvalidValue, rawId, id};
        }
        size_ += frameSize;
    }

    return {PersistenceStatus::Ok, ch16::parameterCount};
}

PersistenceResult ParameterStorageImage::update(
    ch16::ParameterID id,
    const ch16::ParameterStore& parameters) noexcept
{
    const auto location = findRecord(bytes(), id);
    if (!location)
    {
        return {PersistenceStatus::UnknownParameter, 0, id};
    }

    std::array<std::byte, MessageHeaderV1WireSize + 256> scratch{};
    std::size_t frameSize{};
    if (!makeFrame(id, parameters, scratch, frameSize) ||
        frameSize != location->second)
    {
        return {PersistenceStatus::TypeMismatch, 0, id};
    }

    std::copy_n(
        scratch.begin(),
        frameSize,
        data_.begin() + static_cast<std::ptrdiff_t>(location->first));
    return {PersistenceStatus::Ok, 1, id};
}

PersistenceResult ParameterStorageImage::restore(
    ch16::ParameterStore& parameters) const noexcept
{
    ch16::ParameterStore staged = parameters;

    const auto result = walkRecords(
        bytes(),
        [&](ParameterRecordView record,
            const MessageHeaderV1& header,
            pbook::ImmutableByteView payload)
        {
            return decodeAndStage(
                record.parameterId,
                header,
                payload,
                staged);
        });

    if (result.succeeded())
    {
        parameters = staged;
    }
    return result;
}

PersistenceResult ParameterStorageImage::assign(
    std::span<const std::byte> bytesToAssign) noexcept
{
    if (bytesToAssign.size() > data_.size())
    {
        return {PersistenceStatus::ImageFull};
    }
    std::copy(bytesToAssign.begin(), bytesToAssign.end(), data_.begin());
    size_ = bytesToAssign.size();
    return {};
}

PersistenceResult readImageFile(
    const std::filesystem::path& path,
    ParameterStorageImage& image) noexcept
{
    const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0)
    {
        return {errno == ENOENT
                    ? PersistenceStatus::ImageNotFound
                    : PersistenceStatus::OpenFailed};
    }

    std::array<std::byte, MaxStorageImageSize + 1> bytes{};
    std::size_t size{};
    while (size < bytes.size())
    {
        const auto count = ::read(
            descriptor,
            bytes.data() + size,
            bytes.size() - size);
        if (count < 0 && errno == EINTR)
        {
            continue;
        }
        if (count < 0)
        {
            ::close(descriptor);
            return {PersistenceStatus::ReadFailed};
        }
        if (count == 0)
        {
            break;
        }
        size += static_cast<std::size_t>(count);
    }

    if (::close(descriptor) != 0)
    {
        return {PersistenceStatus::ReadFailed};
    }
    if (size > MaxStorageImageSize)
    {
        return {PersistenceStatus::ImageFull};
    }
    return image.assign(std::span{bytes}.first(size));
}

PersistenceResult writeImageFileAtomically(
    const std::filesystem::path& path,
    std::span<const std::byte> snapshot,
    WriteFault fault) noexcept
{
    if (snapshot.empty() || snapshot.size() > MaxStorageImageSize)
    {
        return {PersistenceStatus::WriteFailed};
    }

    const auto temporary = path.string() + ".tmp";
    const int descriptor = ::open(
        temporary.c_str(),
        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
        0644);
    if (descriptor < 0)
    {
        return {PersistenceStatus::OpenFailed};
    }

    const bool written = writeAll(descriptor, snapshot);
    const bool synchronized = written && ::fsync(descriptor) == 0;
    const bool closed = ::close(descriptor) == 0;

    if (!written || !synchronized || !closed ||
        fault == WriteFault::BeforeRename)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return {PersistenceStatus::WriteFailed};
    }

    if (::rename(temporary.c_str(), path.c_str()) != 0)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return {PersistenceStatus::ReplaceFailed};
    }

    auto directory = path.parent_path();
    if (directory.empty())
    {
        directory = ".";
    }

    const int directoryDescriptor = ::open(
        directory.c_str(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directoryDescriptor < 0)
    {
        return {PersistenceStatus::ReplaceFailed};
    }

    const bool directorySynchronized = ::fsync(directoryDescriptor) == 0;
    const bool directoryClosed = ::close(directoryDescriptor) == 0;
    return {
        directorySynchronized && directoryClosed
            ? PersistenceStatus::Ok
            : PersistenceStatus::ReplaceFailed};
}

PersistenceResult inspectImage(
    std::span<const std::byte> image,
    bool printRecords) noexcept
{
    StorageImageHeader header;
    const auto headerResult = decodeImageHeader(image, header);
    std::cout << "image.size=" << image.size() << '\n';

    if (!headerResult.succeeded())
    {
        std::cout << "image.status=" << toString(headerResult.status) << '\n';
        return headerResult;
    }

    std::cout << "image.magic=PIMG\n"
              << "image.version=" << header.version << '\n'
              << "image.records=" << header.recordCount << '\n'
              << "image.header_crc=OK\n";

    return walkRecords(
        image,
        [&](ParameterRecordView record,
            const MessageHeaderV1& bds,
            pbook::ImmutableByteView)
        {
            if (printRecords)
            {
                std::cout << "record[" << toString(record.parameterId) << "]"
                          << " type="
                          << toString(static_cast<ParameterMessageType>(
                                 bds.messageType))
                          << " payload=" << bds.payloadSize
                          << " header_crc=OK payload_crc=OK\n";
            }

            return bds.serviceId == ParameterPersistenceServiceId &&
                   bds.messageType == static_cast<std::uint16_t>(
                       expectedMessageType(record.parameterId))
                ? PersistenceStatus::Ok
                : PersistenceStatus::TypeMismatch;
        });
}

bool corruptImage(
    std::span<std::byte> image,
    Corruption mode) noexcept
{
    if (image.size() < StorageImageHeaderWireSize)
    {
        return false;
    }

    if (mode == Corruption::BadMagic)
    {
        image[0] ^= std::byte{1};
        return true;
    }
    if (mode == Corruption::BadVersion)
    {
        return writeUInt16(image, 4, 99);
    }
    if (mode == Corruption::BadHeaderCrc)
    {
        image[8] ^= std::byte{1};
        return true;
    }
    if (mode == Corruption::Truncate)
    {
        return false;
    }

    const auto first = findRecord(image, ch16::ParameterID::ReportingInterval);
    if (!first)
    {
        return false;
    }

    const auto frameOffset = first->first;
    const auto recordOffset = frameOffset - 2;

    switch (mode)
    {
    case Corruption::BadPayload:
        image[frameOffset + MessageHeaderV1WireSize] ^= std::byte{1};
        return true;

    case Corruption::UnknownParameter:
        return writeUInt16(image, recordOffset, 0xffffu);

    case Corruption::DuplicateParameter:
    {
        const auto second = findRecord(image, ch16::ParameterID::TemperatureLimit);
        return second && writeUInt16(
            image,
            second->first - 2,
            static_cast<std::uint16_t>(ch16::ParameterID::ReportingInterval));
    }

    case Corruption::TypeMismatch:
    case Corruption::InvalidValue:
    {
        MessageHeaderV1 header;
        pbook::BinaryReadStream reader(
            pbook::ImmutableByteView(
                image.data() + frameOffset,
                MessageHeaderV1WireSize),
            HeaderWireEndianness);
        readHeaderV1(reader, header);
        if (!reader.ok())
        {
            return false;
        }

        auto payload = image.subspan(
            frameOffset + MessageHeaderV1WireSize,
            header.payloadSize);

        if (mode == Corruption::TypeMismatch)
        {
            header.messageType = static_cast<std::uint16_t>(
                ParameterMessageType::Float32);
        }
        else
        {
            std::fill(payload.begin(), payload.end(), std::byte{});
            header.payloadCrc = crc16CcittFalse(
                pbook::ImmutableByteView(payload.data(), payload.size()));
        }

        header.headerCrc = computeHeaderCrc(header);
        pbook::BinaryWriteStream writer(
            pbook::MutableByteView(
                image.data() + frameOffset,
                MessageHeaderV1WireSize),
            HeaderWireEndianness);
        writeHeaderV1(writer, header);
        return writer.ok();
    }

    default:
        return false;
    }
}

const char* toString(PersistenceStatus status) noexcept
{
    static constexpr std::array names{
        "Ok", "ImageNotFound", "OpenFailed", "ReadFailed", "WriteFailed",
        "ReplaceFailed", "ImageFull", "InvalidImageMagic",
        "UnsupportedImageVersion", "InvalidImageHeader", "InvalidRecordCount",
        "UnknownParameter", "DuplicateParameter", "InvalidBdsMessage",
        "TypeMismatch", "InvalidValue"};
    return names[static_cast<std::size_t>(status)];
}

const char* toString(ch16::ParameterID id) noexcept
{
    switch (id)
    {
    case ch16::ParameterID::ReportingInterval:
        return "ReportingInterval";
    case ch16::ParameterID::TemperatureLimit:
        return "TemperatureLimit";
    case ch16::ParameterID::OperatingMode:
        return "OperatingMode";
    case ch16::ParameterID::Count:
        return "Count";
    }
    return "Unknown";
}

const char* toString(ParameterMessageType type) noexcept
{
    switch (type)
    {
    case ParameterMessageType::UInt32: return "UInt32";
    case ParameterMessageType::Float32: return "Float32";
    case ParameterMessageType::Enum8: return "Enum8";
    case ParameterMessageType::Ascii16: return "Ascii16";
    case ParameterMessageType::Ascii32: return "Ascii32";
    case ParameterMessageType::Ascii255: return "Ascii255";
    }
    return "Unknown";
}

std::optional<Corruption> parseCorruption(std::string_view text) noexcept
{
    if (text == "bad-magic") return Corruption::BadMagic;
    if (text == "bad-version") return Corruption::BadVersion;
    if (text == "bad-header-crc") return Corruption::BadHeaderCrc;
    if (text == "truncate") return Corruption::Truncate;
    if (text == "bad-payload") return Corruption::BadPayload;
    if (text == "unknown-parameter") return Corruption::UnknownParameter;
    if (text == "duplicate-parameter") return Corruption::DuplicateParameter;
    if (text == "type-mismatch") return Corruption::TypeMismatch;
    if (text == "invalid-value") return Corruption::InvalidValue;
    return std::nullopt;
}

void printParameters(const ch16::ParameterStore& parameters)
{
    ch16::ReportingInterval::ValueType reporting{};
    ch16::TemperatureLimit::ValueType temperature{};
    ch16::OperatingModeParameter::ValueType mode{};

    parameters.get<ch16::ReportingInterval>(reporting);
    parameters.get<ch16::TemperatureLimit>(temperature);
    parameters.get<ch16::OperatingModeParameter>(mode);

    const auto modeName = [mode]
    {
        switch (mode)
        {
        case ch16::OperatingMode::Normal: return "normal";
        case ch16::OperatingMode::Service: return "service";
        case ch16::OperatingMode::Safe: return "safe";
        }
        return "unknown";
    }();

    std::cout << "reporting_interval_ms=" << reporting << '\n'
              << "temperature_limit=" << temperature << '\n'
              << "operating_mode=" << modeName << '\n';
}

} // namespace ch17
