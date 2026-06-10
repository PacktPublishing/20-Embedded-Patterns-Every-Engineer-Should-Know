// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Autumnal Software

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace weather
{
    enum class StatusCode : std::uint8_t
    {
        Ok = 0,
        Failed,
        InvalidArgument,
        BufferTooSmall,
        NotSupported,
        IoError
    };

    struct Status
    {
        StatusCode code = StatusCode::Ok;
        std::size_t detail = 0;        // optional: field index, bytes processed, etc.
        std::string_view context{};    // optional: device name, parser name, etc.
        int sys_errno = 0;             // optional: errno for I/O
    };

    inline bool ok(Status s) noexcept { return s.code == StatusCode::Ok; }
}

