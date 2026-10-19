#include "atlasdb/common/version.hpp"

#include <gtest/gtest.h>

TEST(AtlasDBSmokeTest, VersionIsAvailable) {
    EXPECT_EQ(atlasdb::version(), "0.1.0");
}