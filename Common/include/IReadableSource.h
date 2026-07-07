// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#pragma once

namespace pbook
{

class IReadableSource
{
public:
    virtual ~IReadableSource() = default;

    IReadableSource(const IReadableSource&) = delete;
    IReadableSource& operator=(const IReadableSource&) = delete;

    virtual int fd() const noexcept = 0;
    virtual void on_readable() noexcept = 0;

protected:
    IReadableSource() = default;
};

} // namespace pbook
