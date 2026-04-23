#include <gtest/gtest.h>

#include <format>
#include <string>
#include <vector>

#include <demiplane/ink>

namespace ink = demiplane::ink;

TEST(InkTable, RowDslAsciiAutoWidth) {
    const auto got = ink::table()
                         .headers("name", "p99")
                         .add_row("insert", 1.2)
                         .add_row("select_by_pk", 0.3)
                         .render();

    constexpr std::string_view expected =
        "+--------------+-----+\n"
        "| name         | p99 |\n"
        "+--------------+-----+\n"
        "| insert       | 1.2 |\n"
        "+--------------+-----+\n"
        "| select_by_pk | 0.3 |\n"
        "+--------------+-----+";

    EXPECT_EQ(got, expected);
}

TEST(InkTable, RowDslTerminateAddsSingleTrailingNewline) {
    const auto got = ink::table().headers("a").add_row(1).terminate().render();
    ASSERT_FALSE(got.empty());
    EXPECT_EQ(got.back(), '\n');
    EXPECT_NE(got[got.size() - 2], '\n') << "exactly one trailing newline";
}

TEST(InkTable, EmptyTableEmptyString) {
    EXPECT_EQ(ink::table().render(), "");
}

TEST(InkTable, HeadersOnlyNoRowsSingleHeaderBlock) {
    const auto got = ink::table().headers("a", "b").render();
    constexpr std::string_view expected =
        "+---+---+\n"
        "| a | b |\n"
        "+---+---+";
    EXPECT_EQ(got, expected);
}

TEST(InkTable, AnsiStyledCellPreservesAlignment) {
    const auto got = ink::table()
                         .headers("name", "status")
                         .add_row("svcA", ink::colors::make_red("FAIL"))
                         .add_row("svcBBBB", "OK")
                         .render();

    // name col width: max(4, 4, 7) = 7.
    // status col width: max(6, visible("FAIL")=4, 2) = 6.
    const std::string red_fail = ink::colors::make_red("FAIL");
    const std::string expected =
        std::string{"+---------+--------+\n"}
        + "| name    | status |\n"
        + "+---------+--------+\n"
        + "| svcA    | " + red_fail + "   |\n"
        + "+---------+--------+\n"
        + "| svcBBBB | OK     |\n"
        + "+---------+--------+";
    EXPECT_EQ(got, expected);
}

TEST(InkTable, MultiLineCellExpandsRowHeight) {
    const auto got = ink::table()
                         .headers("name", "dump")
                         .add_row("insert", "{\n  id: 42\n}")
                         .add_row("select", "null")
                         .render();

    // name col width = max(4, 6, 6) = 6.
    // dump col width = max(4, 1("{"), 8("  id: 42"), 1("}"), 4("null")) = 8.
    constexpr std::string_view expected =
        "+--------+----------+\n"
        "| name   | dump     |\n"
        "+--------+----------+\n"
        "| insert | {        |\n"
        "|        |   id: 42 |\n"
        "|        | }        |\n"
        "+--------+----------+\n"
        "| select | null     |\n"
        "+--------+----------+";
    EXPECT_EQ(got, expected);
}

TEST(InkTable, UnicodeBorderByteExactOutput) {
    const auto got = ink::table()
                         .headers("a")
                         .add_row("x")
                         .border(ink::border::unicode)
                         .render();

    constexpr std::string_view expected =
        "\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90\n"  // ┌───┐
        "\xE2\x94\x82 a \xE2\x94\x82\n"                                    // │ a │
        "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\xA4\n"  // ├───┤
        "\xE2\x94\x82 x \xE2\x94\x82\n"                                    // │ x │
        "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98";   // └───┘
    EXPECT_EQ(got, expected);
}

TEST(InkTable, FormatPassthroughDefaultSpec) {
    const std::string val = std::format("{}", 3.14159);
    const auto got        = ink::table().headers("v").add_row(3.14159).render();

    const std::size_t w     = std::max<std::size_t>(1, val.size());
    const std::string dashes(w + 2, '-');
    const std::string expected =
        std::string{"+"} + dashes + "+\n"
        + "| v" + std::string(w - 1, ' ') + " |\n"
        + "+" + dashes + "+\n"
        + "| " + val + std::string(w - val.size(), ' ') + " |\n"
        + "+" + dashes + "+";
    EXPECT_EQ(got, expected);
}

TEST(InkTable, ColumnDslOverRangeOfStructs) {
    struct Bench {
        std::string name;
        double p99;
    };
    const std::vector<Bench> bs{{"insert", 1.2}, {"select", 0.3}};

    const auto got = ink::table(bs)
                         .column("name", [](const Bench& b) { return b.name; })
                         .column("p99", [](const Bench& b) { return b.p99; })
                         .render();

    constexpr std::string_view expected =
        "+--------+-----+\n"
        "| name   | p99 |\n"
        "+--------+-----+\n"
        "| insert | 1.2 |\n"
        "+--------+-----+\n"
        "| select | 0.3 |\n"
        "+--------+-----+";
    EXPECT_EQ(got, expected);
}

TEST(InkTable, MinWidthAppliesFloorPerColumn) {
    const auto got = ink::table().headers("a", "b").add_row(1, 2).min_width(5).render();
    constexpr std::string_view expected =
        "+-------+-------+\n"
        "| a     | b     |\n"
        "+-------+-------+\n"
        "| 1     | 2     |\n"
        "+-------+-------+";
    EXPECT_EQ(got, expected);
}

TEST(InkTable, ColumnAlignRightPadsCellsRight) {
    const auto got = ink::table()
                         .headers("name", "throughput")
                         .add_row("insert", 42)
                         .add_row("select_by_pk", 1234567)
                         .column_align(1, ink::Align::Right)
                         .render();

    // Column widths: name = max(4, 6, 12) = 12, throughput = max(10, 2, 7) = 10.
    // Headers always left-aligned. Column 0 (name) uses default Left. Column 1 uses Right.
    constexpr std::string_view expected =
        "+--------------+------------+\n"
        "| name         | throughput |\n"
        "+--------------+------------+\n"
        "| insert       |         42 |\n"
        "+--------------+------------+\n"
        "| select_by_pk |    1234567 |\n"
        "+--------------+------------+";
    EXPECT_EQ(got, expected);
}

TEST(InkTable, DefaultAlignAppliesToAllDataCells) {
    const auto got = ink::table()
                         .headers("a", "b")
                         .add_row(1, 22)
                         .add_row(333, 4)
                         .align(ink::Align::Right)
                         .render();

    // Column widths: a = 3, b = 2. Headers remain left-aligned; cells right.
    constexpr std::string_view expected =
        "+-----+----+\n"
        "| a   | b  |\n"
        "+-----+----+\n"
        "|   1 | 22 |\n"
        "+-----+----+\n"
        "| 333 |  4 |\n"
        "+-----+----+";
    EXPECT_EQ(got, expected);
}
