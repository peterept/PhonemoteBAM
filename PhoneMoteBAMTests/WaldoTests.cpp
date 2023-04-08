#include "pch.h"

#include "Waldo.h"

TEST(WaldoUtilitySplitTests, nullOrEmptyOrEmptyNewLineString) {
    const size_t MAX_VALS = 32;
    char *vals[MAX_VALS];
    int valc;

    // NULL
    valc = split(NULL, vals, MAX_VALS);
    EXPECT_EQ(0, valc);

    // Empty
    char str[128] = "";
    valc = split(str, vals, MAX_VALS);
    EXPECT_EQ(0, valc);

    // Empty New Line
    char strLF[128] = "\n";
    valc = split(strLF, vals, MAX_VALS);
    EXPECT_EQ(1, valc);
    EXPECT_STREQ("", vals[0]);
}

TEST(WaldoUtilitySplitTests, multiLineCSVString) {
    const size_t MAX_VALS = 32;
    char *vals[MAX_VALS];
    char *s;
    int valc;

    char str[128] = "a,bb,ccc,dddd\n"
                    "e,\n"
                    "f,g";

    // row 1
    valc = split(str, vals, MAX_VALS, ',', &s);
    EXPECT_EQ(4, valc);
    EXPECT_STREQ("ccc", vals[2]);

    // row 2
    valc = split(s, vals, MAX_VALS, ',', &s);
    EXPECT_EQ(2, valc);
    EXPECT_STREQ("", vals[1]);

    // row 3
    valc = split(s, vals, MAX_VALS, ',', &s);
    EXPECT_EQ(2, valc);
    EXPECT_STREQ("g", vals[1]);
}

TEST(WaldoStateTests, defaultValuesAreNotValid) {
    WaldoState state;

    EXPECT_FALSE(state.info.valid);
    EXPECT_FALSE(state.ping.valid);
    EXPECT_FALSE(state.ar.valid);
    EXPECT_FALSE(state.face.valid);
    EXPECT_FALSE(state.trackedImage.valid);
}
