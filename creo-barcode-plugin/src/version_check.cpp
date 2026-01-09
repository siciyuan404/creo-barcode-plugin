#include "version_check.h"
#include <sstream>

namespace creo_barcode {

bool checkCreoVersion(const CreoVersion& version) {
    CreoVersion minVersion = getMinimumVersion();
    return version >= minVersion;
}

bool parseVersionString(const std::string& versionStr, CreoVersion& version) {
    if (versionStr.empty()) {
        return false;
    }
    
    std::istringstream iss(versionStr);
    char dot;
    
    // Parse major version
    if (!(iss >> version.major)) {
        return false;
    }
    
    // Check for minor version
    if (iss.peek() == '.') {
        iss >> dot;
        if (!(iss >> version.minor)) {
            return false;
        }
    } else {
        version.minor = 0;
    }
    
    // Check for patch version
    if (iss.peek() == '.') {
        iss >> dot;
        if (!(iss >> version.patch)) {
            return false;
        }
    } else {
        version.patch = 0;
    }
    
    return true;
}

CreoVersion getMinimumVersion() {
    return CreoVersion(MIN_CREO_MAJOR_VERSION, MIN_CREO_MINOR_VERSION, 0);
}

} // namespace creo_barcode
