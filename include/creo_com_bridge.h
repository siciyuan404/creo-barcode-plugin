/**
 * @file creo_com_bridge.h
 * @brief C++ COM Bridge for Creo VB/COM API
 * 
 * This provides a C++ wrapper for Creo's VB/COM API to enable
 * image insertion functionality in drawings.
 */

#ifndef CREO_COM_BRIDGE_H
#define CREO_COM_BRIDGE_H

#ifdef _WIN32

#include <windows.h>
#include <comdef.h>
#include <string>
#include <vector>
#include <memory>

// Include Creo VB API type definitions
#include "creo_vbapi_types.h"

namespace creo_barcode {

/**
 * @brief Parameters for image insertion
 */
struct ImageInsertParams {
    std::wstring imagePath;     ///< Full path to image file
    double x;                   ///< X coordinate (drawing units)
    double y;                   ///< Y coordinate (drawing units)
    double width;               ///< Width (0 = auto)
    double height;              ///< Height (0 = auto)
    int sheet;                  ///< Sheet number (0 = current)
    
    ImageInsertParams() 
        : x(0.0), y(0.0), width(0.0), height(0.0), sheet(0) {}
};

/**
 * @brief Result of batch insert operation
 */
struct BatchInsertResult {
    int totalCount;                         ///< Total images attempted
    int successCount;                       ///< Successfully inserted
    int failCount;                          ///< Failed to insert
    std::vector<std::string> failedPaths;   ///< Paths of failed images
    std::vector<std::string> errorMessages; ///< Error messages for failures
    
    BatchInsertResult() 
        : totalCount(0), successCount(0), failCount(0) {}
};

/**
 * @brief Information for batch image insertion
 */
struct BatchImageInfo {
    std::string imagePath;      ///< Path to image file
    double x;                   ///< X coordinate
    double y;                   ///< Y coordinate
    double width;               ///< Image width
    double height;              ///< Image height
    
    BatchImageInfo() 
        : x(0.0), y(0.0), width(0.0), height(0.0) {}
};

/**
 * @brief Parameters for grid layout calculation
 */
struct GridLayoutParams {
    double startX;              ///< Starting X coordinate
    double startY;              ///< Starting Y coordinate
    double width;               ///< Image width
    double height;              ///< Image height
    int columns;                ///< Number of columns in grid
    double spacing;             ///< Spacing between images
    
    GridLayoutParams()
        : startX(0.0), startY(0.0), width(50.0), height(50.0)
        , columns(1), spacing(10.0) {}
};

/**
 * @brief Position result from grid calculation
 */
struct GridPosition {
    double x;                   ///< Calculated X coordinate
    double y;                   ///< Calculated Y coordinate
    
    GridPosition() : x(0.0), y(0.0) {}
    GridPosition(double px, double py) : x(px), y(py) {}
};

/**
 * @brief COM Bridge class for Creo VB API operations
 * 
 * Singleton class that manages COM initialization and provides
 * methods for inserting images into Creo drawings via VB/COM API.
 */
class CreoComBridge {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the singleton instance
     */
    static CreoComBridge& getInstance();
    
    // Prevent copying
    CreoComBridge(const CreoComBridge&) = delete;
    CreoComBridge& operator=(const CreoComBridge&) = delete;
    
    /**
     * @brief Initialize COM library and connect to Creo
     * @return true if successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Cleanup COM resources
     */
    void cleanup();
    
    /**
     * @brief Check if COM bridge is initialized
     * @return true if initialized and connected
     */
    bool isInitialized() const;
    
    /**
     * @brief Insert a single image into current drawing
     * @param imagePath Path to image file
     * @param x X coordinate (drawing units)
     * @param y Y coordinate (drawing units)
     * @param width Image width (0 = auto)
     * @param height Image height (0 = auto)
     * @return true if successful
     */
    bool insertImage(const std::string& imagePath,
                     double x, double y,
                     double width, double height);
    
    /**
     * @brief Insert multiple images in batch
     * @param images Vector of image info
     * @return BatchInsertResult with success/failure counts
     */
    BatchInsertResult batchInsertImages(const std::vector<BatchImageInfo>& images);
    
    /**
     * @brief Insert multiple images in a grid layout
     * @param imagePaths Vector of image file paths
     * @param params Grid layout parameters
     * @return BatchInsertResult with success/failure counts
     */
    BatchInsertResult batchInsertImagesGrid(const std::vector<std::string>& imagePaths,
                                            const GridLayoutParams& params);
    
    /**
     * @brief Get last error message
     * @return Error message string
     */
    std::string getLastError() const;
    
    /**
     * @brief Get last HRESULT error code
     * @return HRESULT from last COM operation
     */
    HRESULT getLastHResult() const;
    
    /**
     * @brief Format HRESULT to human-readable string
     * @param hr HRESULT error code
     * @return Formatted error string with hex code and system message
     */
    static std::string formatHResult(HRESULT hr);
    
    /**
     * @brief Get current drawing from Creo session
     * @return Drawing interface pointer, or nullptr if no drawing is open
     */
    IpfcDrawingPtr getCurrentDrawing();

private:
    CreoComBridge();
    ~CreoComBridge();
    
    /**
     * @brief Connect to running Creo instance
     * @return true if connected successfully
     */
    bool connectToCreo();
    
    /**
     * @brief Set error information
     * @param hr HRESULT error code
     * @param context Error context message
     */
    void setError(HRESULT hr, const std::string& context);
    
    /**
     * @brief Set error information (no HRESULT)
     * @param message Error message
     */
    void setError(const std::string& message);
    
    /**
     * @brief Validate image file
     * 
     * Checks:
     * - File path is not empty
     * - File exists
     * - File is a regular file (not directory)
     * - File format is supported (PNG, JPG, BMP)
     * - File is not empty (size > 0)
     * 
     * @param path Path to image file
     * @return true if file is valid, false otherwise (error details in getLastError())
     */
    bool validateImageFile(const std::string& path);
    
    /**
     * @brief Check if a file format is supported
     * @param extension File extension (with or without leading dot)
     * @return true if format is supported
     */
    static bool isFormatSupported(const std::string& extension);
    
    /**
     * @brief Get list of supported image formats
     * @return Vector of supported format extensions (with leading dot)
     */
    static std::vector<std::string> getSupportedFormats();

private:
    bool m_initialized;             ///< Initialization state
    bool m_comInitialized;          ///< COM library initialized
    std::string m_lastError;        ///< Last error message
    HRESULT m_lastHResult;          ///< Last HRESULT code
    
    // COM interface pointers using Creo VB API types
    IpfcAsyncConnectionPtr m_pConnection;   ///< Async connection to Creo
    IpfcSessionPtr m_pSession;              ///< Creo session interface
};

// String conversion utilities
namespace StringUtils {

/**
 * @brief Convert std::wstring to BSTR
 * @param str Wide string to convert
 * @return BSTR (caller must free with SysFreeString)
 */
BSTR stringToBSTR(const std::wstring& str);

/**
 * @brief Convert BSTR to std::wstring
 * @param bstr BSTR to convert
 * @return Wide string
 */
std::wstring bstrToString(BSTR bstr);

/**
 * @brief Convert UTF-8 string to wide string
 * @param utf8 UTF-8 encoded string
 * @return Wide string
 */
std::wstring utf8ToWstring(const std::string& utf8);

/**
 * @brief Convert wide string to UTF-8 string
 * @param wstr Wide string
 * @return UTF-8 encoded string
 */
std::string wstringToUtf8(const std::wstring& wstr);

} // namespace StringUtils

/**
 * @brief Calculate grid position for an image at given index
 * 
 * Calculates the position for an image in a grid layout based on:
 * - x = startX + (index % columns) * (width + spacing)
 * - y = startY - (index / columns) * (height + spacing)
 * 
 * @param index Image index (0-based)
 * @param columns Number of columns in grid
 * @param spacing Spacing between images
 * @param startX Starting X coordinate
 * @param startY Starting Y coordinate
 * @param width Image width
 * @param height Image height
 * @return GridPosition with calculated x, y coordinates
 */
GridPosition calculateGridPosition(int index, int columns, double spacing,
                                   double startX, double startY,
                                   double width, double height);

} // namespace creo_barcode

#else // !_WIN32

// Stub for non-Windows platforms
namespace creo_barcode {

struct ImageInsertParams {
    std::wstring imagePath;
    double x, y, width, height;
    int sheet;
};

struct BatchInsertResult {
    int totalCount, successCount, failCount;
    std::vector<std::string> failedPaths;
    std::vector<std::string> errorMessages;
};

struct BatchImageInfo {
    std::string imagePath;
    double x, y, width, height;
};

class CreoComBridge {
public:
    static CreoComBridge& getInstance() {
        static CreoComBridge instance;
        return instance;
    }
    bool initialize() { return false; }
    void cleanup() {}
    bool isInitialized() const { return false; }
    bool insertImage(const std::string&, double, double, double, double) { return false; }
    BatchInsertResult batchInsertImages(const std::vector<BatchImageInfo>&) { return {}; }
    std::string getLastError() const { return "COM not supported on this platform"; }
    long getLastHResult() const { return -1; }
};

} // namespace creo_barcode

#endif // _WIN32

#endif // CREO_COM_BRIDGE_H
