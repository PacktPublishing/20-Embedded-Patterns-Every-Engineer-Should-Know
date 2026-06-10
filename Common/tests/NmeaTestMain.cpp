// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Mark Wilson

#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

#include "NMEATokenizer.h"
#include "NMEAExtractionStream.h"
#include "NMEAInsertionStream.h"

static int g_failures = 0;

#define CHECK(expr) do { if(!(expr)) { ++g_failures; std::cerr << "FAIL: " #expr " @ " << __FILE__ << ":" << __LINE__ << "\n"; } } while(0)
#define CHECK_OK(st) CHECK((st).ok())

static std::vector<std::string_view> collectFields(const weather::Tokenizer& tok)
{
    std::vector<std::string_view> v;
    for (auto it = tok.begin(); it != tok.end(); ++it)
    {
        v.push_back(*it);
    }
    return v;
}

static void test_checksum_and_identifier()
{
    const std::string_view s = "$GPXYZ,1,2,3*50\r\n";
    weather::Tokenizer tok(s);
    CHECK(tok.status().ok());
    CHECK(tok.checksumValid());
    CHECK(tok.valid());
    CHECK(tok.identifier() == "GPXYZ");
    CHECK(tok.fieldCount() == 3);

    auto f = collectFields(tok);
    CHECK(f.size() == 3);
    CHECK(f[0] == "1");
    CHECK(f[1] == "2");
    CHECK(f[2] == "3");
}

static void test_empty_fields_preserved()
{
    const std::string_view s = "$GPXYZ,1,,3*62\n";
    weather::Tokenizer tok(s);
    CHECK(tok.valid());
    CHECK(tok.fieldCount() == 3);

    auto f = collectFields(tok);
    CHECK(f.size() == 3);
    CHECK(f[0] == "1");
    CHECK(f[1].empty());
    CHECK(f[2] == "3");
}

static void test_trailing_comma_produces_empty_final_field()
{
    const std::string_view s = "$GPXYZ,1,2,*63\n";
    weather::Tokenizer tok(s);
    CHECK(tok.valid());
    CHECK(tok.fieldCount() == 3);

    auto f = collectFields(tok);
    CHECK(f.size() == 3);
    CHECK(f[0] == "1");
    CHECK(f[1] == "2");
    CHECK(f[2].empty());
}

static void test_required_vs_optional()
{
    const std::string_view s = "$GPXYZ,1,,3*62\n";
    weather::Tokenizer tok(s);
    CHECK(tok.valid());

    weather::ExtractionStream xs(tok);

    int a = 0;
    CHECK_OK(xs.readInt(a));
    CHECK(a == 1);

    int b = 0;
    auto st = xs.readInt(b);
    CHECK(!st.ok());
    CHECK(st.code == weather::NmeaParseErrorCode::FieldEmpty);

    weather::ExtractionStream xs2(tok);

    std::optional<int> oa;
    std::optional<int> ob;
    std::optional<int> oc;

    CHECK_OK(xs2.readOptionalInt(oa));
    CHECK(oa.has_value() && *oa == 1);

    CHECK_OK(xs2.readOptionalInt(ob));
    CHECK(!ob.has_value());

    CHECK_OK(xs2.readOptionalInt(oc));
    CHECK(oc.has_value() && *oc == 3);
}


static void test_inserter_round_trip()
{
    weather::InsertionStream ins("GPXYZ");
    std::optional<std::string_view> a = std::string_view("1");
    std::optional<int> b = 2;
    std::optional<int> empty;
    std::optional<double> d = 3.5;

    CHECK_OK(ins.writeOptionalString(a));
    CHECK_OK(ins.writeOptionalInt(b));
    CHECK_OK(ins.writeOptionalInt(empty));
    CHECK_OK(ins.writeOptionalDouble(d));
    CHECK_OK(ins.finalize(false));

    weather::Tokenizer tok(ins.sentence());
    CHECK(tok.status().ok());
    CHECK(tok.checksumValid());
    CHECK(tok.valid());
    CHECK(tok.identifier() == "GPXYZ");
    CHECK(tok.fieldCount() == 4);

    auto f = collectFields(tok);
    CHECK(f.size() == 4);
    CHECK(f[0] == "1");
    CHECK(f[1] == "2");
    CHECK(f[2].empty());
    CHECK(f[3] == "3.5");
}

static void test_inserter_trailing_empty_field()
{
    weather::InsertionStream ins("GPXYZ");
    CHECK_OK(ins.writeInt(1));
    CHECK_OK(ins.writeEmpty());
    CHECK_OK(ins.finalize(false));

    weather::Tokenizer tok(ins.sentence());
    CHECK(tok.valid());
    CHECK(tok.fieldCount() == 2);

    auto f = collectFields(tok);
    CHECK(f.size() == 2);
    CHECK(f[0] == "1");
    CHECK(f[1].empty());
}

static void test_missing_field_is_error_even_for_optional()
{
    const std::string_view s = "$GPXYZ,1,2*4F\n";
    weather::Tokenizer tok(s);
    CHECK(tok.valid());

    weather::ExtractionStream xs(tok);
    std::optional<int> a;
    std::optional<int> b;
    std::optional<int> c;

    CHECK_OK(xs.readOptionalInt(a));
    CHECK_OK(xs.readOptionalInt(b));

    auto st = xs.readOptionalInt(c);
    CHECK(!st.ok());
    CHECK(st.code == weather::NmeaParseErrorCode::FieldMissing);
}

int main()
{
    test_checksum_and_identifier();
    test_empty_fields_preserved();
    test_trailing_comma_produces_empty_final_field();
    test_required_vs_optional();
    test_missing_field_is_error_even_for_optional();
    test_inserter_round_trip();
    test_inserter_trailing_empty_field();

    if (g_failures == 0)
    {
        std::cout << "All tests passed\n";
        return 0;
    }

    std::cerr << g_failures << " test(s) failed\n";
    return 1;
}
