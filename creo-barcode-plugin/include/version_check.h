#ifndef VERSION_CHECK_H
#define VERSION_CHECK_H

#include <string>

namespace creo_barcode {

// Minimum supported Creo version
constexpr int MIN_CREO_MAJOR_VERSION = 8;
constexpr int MIN_CREO_MINOR_VERSION = 0;

struct CreoVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    
    CreoVersion() = default;
    CreoVersion(int maj, int min, int pat = 0) : major(maj), minor(min), patch(pat) {}
    
    bool operator>=(const CreoVersion& other) const {
        if (major != other.major) return major > other.major;
        if (minor != other.minor) return minor > other.minor;
        return patch >= other.patch;
    }
    
    bool operator<(const CreoVersion& other) const {
        return !(*this >= other);
    }
    
    std::string toString() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
};

// Check if Creo version is compatible
bool checkCreoVersion(const CreoVersion& version);

// Parse version string (e.g., "8.0.1" or "8.0")
bool parseVersionString(const std::string& versionStr, CreoVersion& version);

// Get minimum required version
CreoVersion getMinimumVersion();

} // namespace creo_barcode

#endif // VERSION_CHECK_H
