// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "WeatherMonitoringProtocol.h"

#include <cassert>
#include <iostream>
#include <string_view>

namespace
{

void checkSentence(std::string_view body, std::string_view expected)
{
    ch15::SentenceBuffer buffer{};
    std::size_t length{};
    assert(ch15::buildSentence(body, buffer, length));
    assert(std::string_view(buffer.data(), length) == expected);

    ch15::ParsedSentence parsed{};
    assert(ch15::parseSentence(
        std::string_view(buffer.data(), length), parsed) == ch15::ParseStatus::Ok);
}

} // namespace

int main()
{
    checkSentence(
        "WXSTS,1042,174210.125,RUNNING,18432",
        "$WXSTS,1042,174210.125,RUNNING,18432*3E\r\n");

    checkSentence(
        "WXHLT,1043,174210.125,OK,OK,OK,OK",
        "$WXHLT,1043,174210.125,OK,OK,OK,OK*40\r\n");

    checkSentence(
        "WXTMP,1044,174210.125,22.6,C",
        "$WXTMP,1044,174210.125,22.6,C*05\r\n");

    checkSentence(
        "WXPRS,1045,174210.125,1008.4,HPA",
        "$WXPRS,1045,174210.125,1008.4,HPA*0D\r\n");

    checkSentence(
        "WXWND,1046,174210.125,225,12.4,MPH",
        "$WXWND,1046,174210.125,225,12.4,MPH*1D\r\n");

    checkSentence(
        "WXDIA,1047,174210.125,0,7,3",
        "$WXDIA,1047,174210.125,0,7,3*40\r\n");

    checkSentence(
        "WXSUB,201,TMP,1000",
        "$WXSUB,201,TMP,1000*1C\r\n");

    checkSentence(
        "WXUSB,202,TMP",
        "$WXUSB,202,TMP*32\r\n");

    checkSentence(
        "WXACK,201,OK",
        "$WXACK,201,OK*71\r\n");

    ch15::ParsedSentence parsed{};
    assert(ch15::parseSentence(
        "$WXSUB,201,TMP,1000*00\r\n", parsed) ==
        ch15::ParseStatus::ChecksumMismatch);

    std::uint32_t value{};
    assert(ch15::parseUnsigned("201", value));
    assert(value == 201);
    assert(!ch15::parseUnsigned("20x", value));

    assert(ch15::parseReportType("TMP") == ch15::ReportType::Temperature);
    assert(!ch15::parseReportType("BOGUS").has_value());

    std::cout << "All Chapter 15 protocol tests passed.\n";
    return 0;
}
