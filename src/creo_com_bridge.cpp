/**
 * @file creo_com_bridge.cpp
 * @brief Implementation of COM Bridge for Creo VB/COM API
 */

#include "creo_com_bridge.h"
#include "logger.h"

#ifdef _WIN32

#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <objbase.h>
#include <atlbase.h>

// Define GUIDs for Creo VB API interfaces (when not using actual type library)
#if !HAS_CREO_VBAPI
// These GUIDs are placeholders - actual values should come from Creo type library
// CLSID_pfcAsyncConnection
const GUID creo_barcode::CLSID_pfcAsyncConnection = 
    { 0x5B4D3E3A, 0x6C7D, 0x4E8F, { 0x9A, 0x0B, 0x1C, 0x2D, 0x3E, 0x4F, 0x5A, 0x6B } };

// IID_IpfcAsyncConnection
const GUID creo_barcode::IID_IpfcAsyncConnection = 
    { 0x6C8E4F4B, 0x7D8E, 0x5F9A, { 0xAB, 0x1C, 0x2D, 0x3E, 0x4F, 0x5A, 0x6B, 0x7C } };

// IID_IpfcDrawing
const GUID creo_barcode::IID_IpfcDrawing = 
    { 0x7D9F5A5C, 0x8E9F, 0x6AAB, { 0xBC, 0x2D, 0x3E, 0x4F, 0x5A, 0x6B, 0x7C, 0x8D } };
#endif

namespace creo_barcode {

// ============================================================================
// CreoVBAPIFactory Implementation
// ============================================================================

HRESULT CreoVBAPIFactory::CreateAsyncConnection(IpfcAsyncConnection** ppConnection) {
    if (!ppConnection) {
        return E_POINTER;
    }
    *ppConnection = nullptr;
    
#if HAS_CREO_VBAPI
    // Use actual Creo type library
    return CoCreateInstance(
        CLSID_pfcAsyncConnection,
        nullptr,
        CLSCTX_LOCAL_SERVER,
        IID_IpfcAsyncConnection,
        reinterpret_cast<void**>(ppConnection)
    );
#else
    // Without Creo type library, we cannot create the connection
    // Return a specific error code indicating Creo is not available
    return REGDB_E_CLASSNOTREG;  // Class not registered
#endif
}

HRESULT CreoVBAPIFactory::CreatePoint2D(double x, double y, IpfcPoint2D** ppPoint) {
    if (!ppPoint) {
        return E_POINTER;
    }
    *ppPoint = nullptr;
    
#if HAS_CREO_VBAPI
    // Use actual Creo type library to create point
    // Implementation depends on Creo's factory methods
    return E_NOTIMPL;
#else
    return E_NOTIMPL;
#endif
}

HRESULT CreoVBAPIFactory::CreateOutline2D(double x1, double y1, 
                                           double x2, double y2,
                                           IpfcOutline2D** ppOutline) {
    if (!ppOutline) {
        return E_POINTER;
    }
    *ppOutline = nullptr;
    
#if HAS_CREO_VBAPI
    // Use actual Creo type library to create outline
    // Implementation depends on Creo's factory methods
    return E_NOTIMPL;
#else
    return E_NOTIMPL;
#endif
}

// ============================================================================
// String Conversion Utilities
// ============================================================================

namespace StringUtils {

BSTR stringToBSTR(const std::wstring& str) {
    if (str.empty()) {
        return SysAllocString(L"");
    }
    return SysAllocStringLen(str.c_str(), static_cast<UINT>(str.length()));
}

std::wstring bstrToString(BSTR bstr) {
    if (bstr == nullptr) {
        return std::wstring();
    }
    // BSTR length is stored before the string data
    UINT len = SysStringLen(bstr);
    if (len == 0) {
        return std::wstring();
    }
    return std::wstring(bstr, len);
}

std::wstring utf8ToWstring(const std::string& utf8) {
    if (utf8.empty()) {
        return std::wstring();
    }
    
    // Calculate required buffer size
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, 
                                       utf8.c_str(), 
                                       static_cast<int>(utf8.length()),
                                       nullptr, 0);
    if (wideLen == 0) {
        return std::wstring();
    }
    
    // Perform conversion
    std::wstring result(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0,
                        utf8.c_str(),
                        static_cast<int>(utf8.length()),
                        &result[0], wideLen);
    
    return result;
}

std::string wstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) {
        return std::string();
    }
    
    // Calculate required buffer size
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0,
                                       wstr.c_str(),
                                       static_cast<int>(wstr.length()),
                                       nullptr, 0,
                                       nullptr, nullptr);
    if (utf8Len == 0) {
        return std::string();
    }
    
    // Perform conversion
    std::string result(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0,
                        wstr.c_str(),
                        static_cast<int>(wstr.length()),
                        &result[0], utf8Len,
                        nullptr, nullptr);
    
    return result;
}

} // namespace StringUtils

// ============================================================================
// CreoComBridge Implementation
// ============================================================================

CreoComBridge::CreoComBridge()
    : m_initialized(false)
    , m_comInitialized(false)
    , m_lastHResult(S_OK)
    , m_pConnection(nullptr)
    , m_pSession(nullptr)
{
}

CreoComBridge::~CreoComBridge() {
    cleanup();
}

CreoComBridge& CreoComBridge::getInstance() {
    static CreoComBridge instance;
    return instance;
}

bool CreoComBridge::initialize() {
    if (m_initialized) {
        return true;
    }
    
    LOG_INFO("Initializing COM bridge for Creo VB API");
    
    // Initialize COM library
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        // RPC_E_CHANGED_MODE means COM was already initialized with different mode
        // which is acceptable
        setError(hr, "Failed to initialize COM library");
        return false;
    }
    
    m_comInitialized = true;
    LOG_INFO("COM library initialized successfully");
    
    // Connect to Creo
    if (!connectToCreo()) {
        // Connection failed, but COM is initialized
        // We'll try to connect later when needed
        LOG_WARNING("Could not connect to Creo instance, will retry on demand");
    }
    
    m_initialized = true;
    LOG_INFO("COM bridge initialized");
    return true;
}

void CreoComBridge::cleanup() {
    if (!m_initialized && !m_comInitialized) {
        return;
    }
    
    LOG_INFO("Cleaning up COM bridge");
    
    // Release COM interfaces (smart pointers handle Release automatically)
    m_pSession.Release();
    m_pConnection.Release();
    
    // Uninitialize COM
    if (m_comInitialized) {
        CoUninitialize();
        m_comInitialized = false;
        LOG_INFO("COM library uninitialized");
    }
    
    m_initialized = false;
    LOG_INFO("COM bridge cleanup complete");
}

bool CreoComBridge::isInitialized() const {
    return m_initialized;
}

bool CreoComBridge::connectToCreo() {
    LOG_INFO("Attempting to connect to Creo instance");
    
    // Release any existing connection
    m_pSession.Release();
    m_pConnection.Release();
    
    // Create AsyncConnection object
    IpfcAsyncConnection* pConnection = nullptr;
    HRESULT hr = CreoVBAPIFactory::CreateAsyncConnection(&pConnection);
    
    if (FAILED(hr)) {
        if (hr == REGDB_E_CLASSNOTREG) {
            setError(hr, "Creo VB API not registered - ensure Creo is installed and VB API is available");
        } else {
            setError(hr, "Failed to create Creo AsyncConnection");
        }
        return false;
    }
    
    m_pConnection.Attach(pConnection);
    LOG_INFO("AsyncConnection object created");
    
    // Check if Creo is running
    VARIANT_BOOL isRunning = VARIANT_FALSE;
    hr = m_pConnection->IsRunning(&isRunning);
    if (FAILED(hr)) {
        setError(hr, "Failed to check if Creo is running");
        m_pConnection.Release();
        return false;
    }
    
    if (isRunning == VARIANT_FALSE) {
        setError("Creo is not running - please start Creo first");
        m_pConnection.Release();
        return false;
    }
    
    LOG_INFO("Creo is running, obtaining session...");
    
    // Get session from connection
    IpfcSession* pSession = nullptr;
    hr = m_pConnection->get_Session(&pSession);
    
    if (FAILED(hr) || !pSession) {
        setError(hr, "Failed to get Creo session from connection");
        m_pConnection.Release();
        return false;
    }
    
    m_pSession.Attach(pSession);
    LOG_INFO("Successfully connected to Creo session");
    
    return true;
}

IpfcDrawingPtr CreoComBridge::getCurrentDrawing() {
    LOG_INFO("Getting current drawing from Creo session");
    
    // Ensure we have a valid session
    if (!m_pSession) {
        // Try to connect if not connected
        if (!connectToCreo()) {
            setError("Cannot get drawing - not connected to Creo");
            return nullptr;
        }
    }
    
    // Get current model from session
    IpfcModel* pModel = nullptr;
    HRESULT hr = m_pSession->get_CurrentModel(&pModel);
    
    if (FAILED(hr)) {
        setError(hr, "Failed to get current model from session");
        return nullptr;
    }
    
    if (!pModel) {
        setError("No model is currently open in Creo");
        return nullptr;
    }
    
    // Check if the model is a drawing
    pfcModelType modelType;
    hr = pModel->get_Type(&modelType);
    
    if (FAILED(hr)) {
        setError(hr, "Failed to get model type");
        pModel->Release();
        return nullptr;
    }
    
    if (modelType != pfcMDL_DRAWING) {
        // Get model filename for error message
        BSTR bstrFileName = nullptr;
        pModel->get_FileName(&bstrFileName);
        std::wstring fileName = StringUtils::bstrToString(bstrFileName);
        if (bstrFileName) {
            SysFreeString(bstrFileName);
        }
        
        std::string fileNameUtf8 = StringUtils::wstringToUtf8(fileName);
        setError("Current model is not a drawing: " + fileNameUtf8 + 
                 " (type: " + std::to_string(static_cast<int>(modelType)) + ")");
        pModel->Release();
        return nullptr;
    }
    
    // Cast to IpfcDrawing interface
    IpfcDrawing* pDrawing = nullptr;
    hr = pModel->QueryInterface(IID_IpfcDrawing, reinterpret_cast<void**>(&pDrawing));
    
    // Release the model reference (we now have drawing reference)
    pModel->Release();
    
    if (FAILED(hr) || !pDrawing) {
        setError(hr, "Failed to cast model to drawing interface");
        return nullptr;
    }
    
    LOG_INFO("Successfully obtained current drawing interface");
    
    // Return smart pointer (takes ownership)
    IpfcDrawingPtr drawingPtr;
    drawingPtr.Attach(pDrawing);
    return drawingPtr;
}

void CreoComBridge::setError(HRESULT hr, const std::string& context) {
    m_lastHResult = hr;
    
    // Format HRESULT as hex string
    std::ostringstream oss;
    oss << context << " (HRESULT: 0x" << std::hex << std::setfill('0') 
        << std::setw(8) << static_cast<unsigned long>(hr);
    
    // Try to get system error message for the HRESULT
    LPWSTR msgBuffer = nullptr;
    DWORD msgLen = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&msgBuffer),
        0,
        nullptr
    );
    
    if (msgLen > 0 && msgBuffer != nullptr) {
        // Convert wide string to UTF-8 and append to error message
        std::wstring wMsg(msgBuffer, msgLen);
        // Remove trailing newlines
        while (!wMsg.empty() && (wMsg.back() == L'\n' || wMsg.back() == L'\r')) {
            wMsg.pop_back();
        }
        std::string utf8Msg = StringUtils::wstringToUtf8(wMsg);
        oss << " - " << utf8Msg;
        LocalFree(msgBuffer);
    }
    
    oss << ")";
    m_lastError = oss.str();
    
    // Log the error with full details
    LOG_ERROR(m_lastError);
}

void CreoComBridge::setError(const std::string& message) {
    // Use E_FAIL as the default HRESULT for simple string errors
    // and include it in the message for consistency with the HRESULT overload
    m_lastHResult = E_FAIL;
    
    // Format the error message to include HRESULT in hex format
    // This ensures Property 7 (error messages contain HRESULT) is satisfied
    std::ostringstream oss;
    oss << message << " (HRESULT: 0x" << std::hex << std::setfill('0') 
        << std::setw(8) << static_cast<unsigned long>(E_FAIL) << ")";
    m_lastError = oss.str();
    
    LOG_ERROR(m_lastError);
}

std::string CreoComBridge::getLastError() const {
    return m_lastError;
}

HRESULT CreoComBridge::getLastHResult() const {
    return m_lastHResult;
}

std::string CreoComBridge::formatHResult(HRESULT hr) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setfill('0') << std::setw(8) 
        << static_cast<unsigned long>(hr);
    
    // Try to get system error message for the HRESULT
    LPWSTR msgBuffer = nullptr;
    DWORD msgLen = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&msgBuffer),
        0,
        nullptr
    );
    
    if (msgLen > 0 && msgBuffer != nullptr) {
        std::wstring wMsg(msgBuffer, msgLen);
        // Remove trailing newlines
        while (!wMsg.empty() && (wMsg.back() == L'\n' || wMsg.back() == L'\r')) {
            wMsg.pop_back();
        }
        std::string utf8Msg = StringUtils::wstringToUtf8(wMsg);
        oss << " (" << utf8Msg << ")";
        LocalFree(msgBuffer);
    }
    
    return oss.str();
}

bool CreoComBridge::validateImageFile(const std::string& path) {
    LOG_INFO("Validating image file: " + path);
    
    // Check if path is empty
    if (path.empty()) {
        setError("Image file path is empty");
        return false;
    }
    
    // Check if file exists
    std::error_code ec;
    bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        setError("Error checking file existence: " + path + " (" + ec.message() + ")");
        return false;
    }
    
    if (!exists) {
        setError("Image file not found: " + path);
        return false;
    }
    
    // Check if it's a regular file (not a directory)
    bool isRegularFile = std::filesystem::is_regular_file(path, ec);
    if (ec) {
        setError("Error checking file type: " + path + " (" + ec.message() + ")");
        return false;
    }
    
    if (!isRegularFile) {
        setError("Path is not a regular file: " + path);
        return false;
    }
    
    // Check file extension
    std::filesystem::path filePath(path);
    std::string ext = filePath.extension().string();
    
    // Convert to lowercase for comparison
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Supported formats: PNG, JPG/JPEG, BMP
    static const std::vector<std::string> supportedFormats = {
        ".png", ".jpg", ".jpeg", ".bmp"
    };
    
    bool formatSupported = false;
    for (const auto& fmt : supportedFormats) {
        if (ext == fmt) {
            formatSupported = true;
            break;
        }
    }
    
    if (!formatSupported) {
        setError("Unsupported image format: " + ext + " (supported: PNG, JPG, BMP). File: " + path);
        return false;
    }
    
    // Check file size (must be > 0)
    auto fileSize = std::filesystem::file_size(path, ec);
    if (ec) {
        setError("Error getting file size: " + path + " (" + ec.message() + ")");
        return false;
    }
    
    if (fileSize == 0) {
        setError("Image file is empty (0 bytes): " + path);
        return false;
    }
    
    LOG_INFO("Image file validation passed: " + path + " (format: " + ext + ", size: " + std::to_string(fileSize) + " bytes)");
    return true;
}

bool CreoComBridge::isFormatSupported(const std::string& extension) {
    std::string ext = extension;
    
    // Add leading dot if not present
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }
    
    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    auto formats = getSupportedFormats();
    for (const auto& fmt : formats) {
        if (ext == fmt) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> CreoComBridge::getSupportedFormats() {
    return { ".png", ".jpg", ".jpeg", ".bmp" };
}

bool CreoComBridge::insertImage(const std::string& imagePath,
                                 double x, double y,
                                 double width, double height) {
    LOG_INFO("Inserting image: " + imagePath);
    LOG_INFO("Position: (" + std::to_string(x) + ", " + std::to_string(y) + ")");
    LOG_INFO("Size: " + std::to_string(width) + " x " + std::to_string(height));
    
    // Parameter validation
    if (imagePath.empty()) {
        setError("Image path cannot be empty");
        return false;
    }
    
    if (width < 0 || height < 0) {
        setError("Image dimensions cannot be negative (width: " + 
                 std::to_string(width) + ", height: " + std::to_string(height) + ")");
        return false;
    }
    
    // Validate image file
    if (!validateImageFile(imagePath)) {
        // Error already set by validateImageFile
        return false;
    }
    
    if (!m_initialized) {
        setError("COM bridge not initialized - call initialize() first");
        return false;
    }
    
    // Get current drawing
    IpfcDrawingPtr pDrawing = getCurrentDrawing();
    if (!pDrawing) {
        // Error already set by getCurrentDrawing
        return false;
    }
    
    // Convert image path to wide string for COM
    std::wstring wImagePath = StringUtils::utf8ToWstring(imagePath);
    BSTR bstrImagePath = StringUtils::stringToBSTR(wImagePath);
    
    if (!bstrImagePath) {
        setError("Failed to convert image path to BSTR");
        return false;
    }
    
    // Calculate image bounds
    // If width/height are 0, use default size (auto-size based on image)
    double actualWidth = (width > 0) ? width : 50.0;   // Default 50 drawing units
    double actualHeight = (height > 0) ? height : 50.0; // Default 50 drawing units
    
    // Create outline for image position and size
    // Outline defines the bounding box: (x1, y1) = lower-left, (x2, y2) = upper-right
    double x1 = x;
    double y1 = y;
    double x2 = x + actualWidth;
    double y2 = y + actualHeight;
    
    LOG_INFO("Creating image outline: (" + std::to_string(x1) + ", " + std::to_string(y1) + 
             ") to (" + std::to_string(x2) + ", " + std::to_string(y2) + ")");
    
    // Create outline using factory
    IpfcOutline2D* pOutline = nullptr;
    HRESULT hr = CreoVBAPIFactory::CreateOutline2D(x1, y1, x2, y2, &pOutline);
    
    if (FAILED(hr) || !pOutline) {
        SysFreeString(bstrImagePath);
        if (hr == E_NOTIMPL) {
            setError(hr, "CreateOutline2D not implemented - Creo type library required");
        } else {
            setError(hr, "Failed to create outline for image bounds");
        }
        return false;
    }
    
    IpfcOutline2DPtr outlinePtr;
    outlinePtr.Attach(pOutline);
    
    // Call CreateDraftingImage on the drawing
    IpfcDraftingImage* pImage = nullptr;
    hr = pDrawing->CreateDraftingImage(bstrImagePath, pOutline, &pImage);
    
    // Free the BSTR
    SysFreeString(bstrImagePath);
    
    if (FAILED(hr)) {
        setError(hr, "Failed to create drafting image in drawing");
        return false;
    }
    
    if (!pImage) {
        setError("CreateDraftingImage returned null image pointer");
        return false;
    }
    
    // Release the image pointer (we don't need to keep it)
    pImage->Release();
    
    LOG_INFO("Image inserted successfully");
    
    // Refresh the view
    if (m_pSession) {
        IpfcWindow* pWindow = nullptr;
        hr = m_pSession->get_CurrentWindow(&pWindow);
        
        if (SUCCEEDED(hr) && pWindow) {
            hr = pWindow->Repaint();
            if (FAILED(hr)) {
                LOG_WARNING("Failed to repaint window after image insertion (HRESULT: 0x" + 
                           std::to_string(hr) + ")");
                // Don't fail the operation just because repaint failed
            } else {
                LOG_INFO("Window repainted successfully");
            }
            pWindow->Release();
        } else {
            LOG_WARNING("Could not get current window for repaint");
        }
    }
    
    return true;
}

BatchInsertResult CreoComBridge::batchInsertImages(const std::vector<BatchImageInfo>& images) {
    BatchInsertResult result;
    result.totalCount = static_cast<int>(images.size());
    result.successCount = 0;
    result.failCount = 0;
    
    if (images.empty()) {
        return result;
    }
    
    LOG_INFO("Batch inserting " + std::to_string(images.size()) + " images");
    
    for (const auto& img : images) {
        if (insertImage(img.imagePath, img.x, img.y, img.width, img.height)) {
            result.successCount++;
        } else {
            result.failCount++;
            result.failedPaths.push_back(img.imagePath);
            result.errorMessages.push_back(m_lastError);
        }
    }
    
    LOG_INFO("Batch insert complete: " + std::to_string(result.successCount) + 
             " succeeded, " + std::to_string(result.failCount) + " failed");
    
    return result;
}

BatchInsertResult CreoComBridge::batchInsertImagesGrid(const std::vector<std::string>& imagePaths,
                                                        const GridLayoutParams& params) {
    BatchInsertResult result;
    result.totalCount = static_cast<int>(imagePaths.size());
    result.successCount = 0;
    result.failCount = 0;
    
    if (imagePaths.empty()) {
        return result;
    }
    
    // Validate columns parameter
    int columns = params.columns;
    if (columns < 1) {
        columns = 1;
        LOG_WARNING("Invalid columns parameter (" + std::to_string(params.columns) + 
                   "), using default value of 1");
    }
    
    LOG_INFO("Batch inserting " + std::to_string(imagePaths.size()) + 
             " images in grid layout (" + std::to_string(columns) + " columns)");
    LOG_INFO("Grid start position: (" + std::to_string(params.startX) + ", " + 
             std::to_string(params.startY) + ")");
    LOG_INFO("Image size: " + std::to_string(params.width) + " x " + 
             std::to_string(params.height) + ", spacing: " + std::to_string(params.spacing));
    
    for (size_t i = 0; i < imagePaths.size(); ++i) {
        // Calculate grid position for this image
        GridPosition pos = calculateGridPosition(
            static_cast<int>(i),
            columns,
            params.spacing,
            params.startX,
            params.startY,
            params.width,
            params.height
        );
        
        LOG_INFO("Image " + std::to_string(i) + " position: (" + 
                 std::to_string(pos.x) + ", " + std::to_string(pos.y) + ")");
        
        // Insert the image at calculated position
        if (insertImage(imagePaths[i], pos.x, pos.y, params.width, params.height)) {
            result.successCount++;
        } else {
            result.failCount++;
            result.failedPaths.push_back(imagePaths[i]);
            result.errorMessages.push_back(m_lastError);
            // Continue with remaining images even if one fails
        }
    }
    
    LOG_INFO("Grid batch insert complete: " + std::to_string(result.successCount) + 
             " succeeded, " + std::to_string(result.failCount) + " failed");
    
    return result;
}

// ============================================================================
// Grid Position Calculation
// ============================================================================

GridPosition calculateGridPosition(int index, int columns, double spacing,
                                   double startX, double startY,
                                   double width, double height) {
    // Ensure columns is at least 1 to avoid division by zero
    if (columns < 1) {
        columns = 1;
    }
    
    // Calculate column and row for this index
    int col = index % columns;
    int row = index / columns;
    
    // Calculate position using the formula from design document:
    // x = startX + (i % C) * (width + S)
    // y = startY - (i / C) * (height + S)
    GridPosition pos;
    pos.x = startX + col * (width + spacing);
    pos.y = startY - row * (height + spacing);
    
    return pos;
}

} // namespace creo_barcode

#endif // _WIN32
