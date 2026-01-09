#include <gtest/gtest.h>
#include "version_check.h"

namespace creo_barcode {
namespace testing {

// =============================================================================
// Basic Version Check Tests
// =============================================================================

TEST(VersionCheckTest, CheckCreoVersionAcceptsVersion8) {
    EXPECT_TRUE(checkCreoVersion(CreoVersion(8, 0, 0)));
    EXPECT_TRUE(checkCreoVersion(CreoVersion(8, 0, 1)));
    EXPECT_TRUE(checkCreoVersion(CreoVersion(8, 1, 0)));
}

TEST(VersionCheckTest, CheckCreoVersionAcceptsHigherVersions) {
    EXPECT_TRUE(checkCreoVersion(CreoVersion(9, 0, 0)));
    EXPECT_TRUE(checkCreoVersion(CreoVersion(10, 0, 0)));
}

TEST(VersionCheckTest, CheckCreoVersionRejectsLowerVersions) {
    EXPECT_FALSE(checkCreoVersion(CreoVersion(7, 0, 0)));
    EXPECT_FALSE(checkCreoVersion(CreoVersion(7, 9, 9)));
    EXPECT_FALSE(checkCreoVersion(CreoVersion(6, 0, 0)));
}

// =============================================================================
// Boundary Version Number Tests (边界版本号测试)
// Requirement 4.2: Version >= 8.0 should be compatible, < 8.0 should not
// =============================================================================

TEST(VersionCheckTest, BoundaryExactMinimumVersion) {
    // Exact minimum version 8.0.0 should be compatible
    EXPECT_TRUE(checkCreoVersion(CreoVersion(8, 0, 0)));
}

TEST(VersionCheckTest, BoundaryJustBelowMinimum) {
    // Version 7.9.9 is just below 8.0.0 - should be incompatible
    EXPECT_FALSE(checkCreoVersion(CreoVersion(7, 9, 9)));
    // Version 7.99.99 - still below 8.0.0
    EXPECT_FALSE(checkCreoVersion(CreoVersion(7, 99, 99)));
}

TEST(VersionCheckTest, BoundaryJustAboveMinimum) {
    // Version 8.0.1 is just above minimum - should be compatible
    EXPECT_TRUE(checkCreoVersion(CreoVersion(8, 0, 1)));
    // Version 8.1.0 - should be compatible
    EXPECT_TRUE(checkCreoVersion(CreoVersion(8, 1, 0)));
}

TEST(VersionCheckTest, BoundaryMajorVersionTransition) {
    // Last version of major 7 - incompatible
    EXPECT_FALSE(checkCreoVersion(CreoVersion(7, 255, 255)));
    // First version of major 8 - compatible
    EXPECT_TRUE(checkCreoVersion(CreoVersion(8, 0, 0)));
    // First version of major 9 - compatible
    EXPECT_TRUE(checkCreoVersion(CreoVersion(9, 0, 0)));
}

TEST(VersionCheckTest, BoundaryZeroVersions) {
    // Version 0.0.0 - should be incompatible
    EXPECT_FALSE(checkCreoVersion(CreoVersion(0, 0, 0)));
    // Version 1.0.0 - should be incompatible
    EXPECT_FALSE(checkCreoVersion(CreoVersion(1, 0, 0)));
}

TEST(VersionCheckTest, BoundaryHighVersionNumbers) {
    // Very high version numbers should be compatible
    EXPECT_TRUE(checkCreoVersion(CreoVersion(100, 0, 0)));
    EXPECT_TRUE(checkCreoVersion(CreoVersion(8, 100, 100)));
}

// =============================================================================
// Version Format Parsing Tests (各种版本格式测试)
// =============================================================================

TEST(VersionCheckTest, ParseVersionStringHandlesFullVersion) {
    CreoVersion version;
    EXPECT_TRUE(parseVersionString("8.0.1", version));
    EXPECT_EQ(version.major, 8);
    EXPECT_EQ(version.minor, 0);
    EXPECT_EQ(version.patch, 1);
}

TEST(VersionCheckTest, ParseVersionStringHandlesMajorMinor) {
    CreoVersion version;
    EXPECT_TRUE(parseVersionString("9.1", version));
    EXPECT_EQ(version.major, 9);
    EXPECT_EQ(version.minor, 1);
    EXPECT_EQ(version.patch, 0);
}

TEST(VersionCheckTest, ParseVersionStringHandlesMajorOnly) {
    CreoVersion version;
    EXPECT_TRUE(parseVersionString("10", version));
    EXPECT_EQ(version.major, 10);
    EXPECT_EQ(version.minor, 0);
    EXPECT_EQ(version.patch, 0);
}

TEST(VersionCheckTest, ParseVersionStringRejectsEmpty) {
    CreoVersion version;
    EXPECT_FALSE(parseVersionString("", version));
}

TEST(VersionCheckTest, ParseVersionStringRejectsInvalid) {
    CreoVersion version;
    EXPECT_FALSE(parseVersionString("abc", version));
}

TEST(VersionCheckTest, ParseVersionStringWithLargeNumbers) {
    CreoVersion version;
    EXPECT_TRUE(parseVersionString("100.200.300", version));
    EXPECT_EQ(version.major, 100);
    EXPECT_EQ(version.minor, 200);
    EXPECT_EQ(version.patch, 300);
}

TEST(VersionCheckTest, ParseVersionStringWithZeros) {
    CreoVersion version;
    EXPECT_TRUE(parseVersionString("0.0.0", version));
    EXPECT_EQ(version.major, 0);
    EXPECT_EQ(version.minor, 0);
    EXPECT_EQ(version.patch, 0);
}

TEST(VersionCheckTest, ParseVersionStringWithSingleDigits) {
    CreoVersion version;
    EXPECT_TRUE(parseVersionString("1.2.3", version));
    EXPECT_EQ(version.major, 1);
    EXPECT_EQ(version.minor, 2);
    EXPECT_EQ(version.patch, 3);
}

TEST(VersionCheckTest, ParseVersionStringWithDoubleDigits) {
    CreoVersion version;
    EXPECT_TRUE(parseVersionString("12.34.56", version));
    EXPECT_EQ(version.major, 12);
    EXPECT_EQ(version.minor, 34);
    EXPECT_EQ(version.patch, 56);
}

TEST(VersionCheckTest, ParseVersionStringRejectsNonNumeric) {
    CreoVersion version;
    // Strings starting with non-numeric characters should fail
    EXPECT_FALSE(parseVersionString("8.x.0", version));
    EXPECT_FALSE(parseVersionString("v8.0.0", version));
}

TEST(VersionCheckTest, ParseVersionStringHandlesTrailingSuffix) {
    CreoVersion version;
    // Parser extracts numeric parts, ignoring trailing suffixes
    // This is acceptable behavior - "8.0.0-beta" extracts 8.0.0
    EXPECT_TRUE(parseVersionString("8.0.0-beta", version));
    EXPECT_EQ(version.major, 8);
    EXPECT_EQ(version.minor, 0);
    EXPECT_EQ(version.patch, 0);
}

TEST(VersionCheckTest, ParseVersionStringHandlesLeadingWhitespace) {
    CreoVersion version;
    // istringstream skips leading whitespace by default
    EXPECT_TRUE(parseVersionString(" 8.0.0", version));
    EXPECT_EQ(version.major, 8);
}

// =============================================================================
// Version Comparison Tests
// =============================================================================

TEST(VersionCheckTest, CreoVersionComparison) {
    EXPECT_TRUE(CreoVersion(8, 0, 0) >= CreoVersion(8, 0, 0));
    EXPECT_TRUE(CreoVersion(8, 1, 0) >= CreoVersion(8, 0, 0));
    EXPECT_TRUE(CreoVersion(9, 0, 0) >= CreoVersion(8, 0, 0));
    EXPECT_FALSE(CreoVersion(7, 9, 9) >= CreoVersion(8, 0, 0));
}

TEST(VersionCheckTest, CreoVersionComparisonMajorDominates) {
    // Major version takes precedence
    EXPECT_TRUE(CreoVersion(9, 0, 0) >= CreoVersion(8, 99, 99));
    EXPECT_FALSE(CreoVersion(7, 99, 99) >= CreoVersion(8, 0, 0));
}

TEST(VersionCheckTest, CreoVersionComparisonMinorSecondary) {
    // When major is equal, minor takes precedence
    EXPECT_TRUE(CreoVersion(8, 5, 0) >= CreoVersion(8, 4, 99));
    EXPECT_FALSE(CreoVersion(8, 4, 99) >= CreoVersion(8, 5, 0));
}

TEST(VersionCheckTest, CreoVersionComparisonPatchTertiary) {
    // When major and minor are equal, patch is compared
    EXPECT_TRUE(CreoVersion(8, 0, 5) >= CreoVersion(8, 0, 4));
    EXPECT_FALSE(CreoVersion(8, 0, 4) >= CreoVersion(8, 0, 5));
}

TEST(VersionCheckTest, CreoVersionLessThanOperator) {
    EXPECT_TRUE(CreoVersion(7, 0, 0) < CreoVersion(8, 0, 0));
    EXPECT_TRUE(CreoVersion(8, 0, 0) < CreoVersion(8, 1, 0));
    EXPECT_TRUE(CreoVersion(8, 0, 0) < CreoVersion(8, 0, 1));
    EXPECT_FALSE(CreoVersion(8, 0, 0) < CreoVersion(8, 0, 0));
    EXPECT_FALSE(CreoVersion(9, 0, 0) < CreoVersion(8, 0, 0));
}

// =============================================================================
// Version ToString Tests
// =============================================================================

TEST(VersionCheckTest, CreoVersionToString) {
    CreoVersion version(8, 1, 2);
    EXPECT_EQ(version.toString(), "8.1.2");
}

TEST(VersionCheckTest, CreoVersionToStringWithZeros) {
    EXPECT_EQ(CreoVersion(0, 0, 0).toString(), "0.0.0");
    EXPECT_EQ(CreoVersion(8, 0, 0).toString(), "8.0.0");
}

TEST(VersionCheckTest, CreoVersionToStringWithLargeNumbers) {
    EXPECT_EQ(CreoVersion(100, 200, 300).toString(), "100.200.300");
}

// =============================================================================
// Minimum Version Tests
// =============================================================================

TEST(VersionCheckTest, GetMinimumVersionReturns8) {
    auto minVersion = getMinimumVersion();
    EXPECT_EQ(minVersion.major, 8);
    EXPECT_EQ(minVersion.minor, 0);
}

TEST(VersionCheckTest, GetMinimumVersionMatchesConstants) {
    auto minVersion = getMinimumVersion();
    EXPECT_EQ(minVersion.major, MIN_CREO_MAJOR_VERSION);
    EXPECT_EQ(minVersion.minor, MIN_CREO_MINOR_VERSION);
}

// =============================================================================
// Integration Tests: Parse and Check Combined
// =============================================================================

TEST(VersionCheckTest, ParseAndCheckCompatibleVersion) {
    CreoVersion version;
    ASSERT_TRUE(parseVersionString("8.0.0", version));
    EXPECT_TRUE(checkCreoVersion(version));
}

TEST(VersionCheckTest, ParseAndCheckIncompatibleVersion) {
    CreoVersion version;
    ASSERT_TRUE(parseVersionString("7.9.9", version));
    EXPECT_FALSE(checkCreoVersion(version));
}

TEST(VersionCheckTest, ParseAndCheckHigherVersion) {
    CreoVersion version;
    ASSERT_TRUE(parseVersionString("10.5.3", version));
    EXPECT_TRUE(checkCreoVersion(version));
}

} // namespace testing
} // namespace creo_barcode
