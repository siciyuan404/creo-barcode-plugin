#ifndef DRAWING_INTERFACE_H
#define DRAWING_INTERFACE_H

#include <string>
#include <vector>
#include <functional>
#include "error_codes.h"

namespace creo_barcode {

// Forward declarations for Creo Toolkit types
// In actual implementation, these would be from ProToolkit.h
// For now, we use placeholder types that can be replaced with actual Creo types
#ifndef PRO_TOOLKIT_TYPES_DEFINED
#define PRO_TOOLKIT_TYPES_DEFINED
typedef void* ProDrawing;
typedef void* ProMdl;
typedef int ProError;
const int PRO_TK_NO_ERROR = 0;
const int PRO_TK_E_NOT_FOUND = -1;
const int PRO_TK_E_INVALID_TYPE = -2;
const int PRO_TK_E_GENERAL_ERROR = -3;
#endif

// Position structure for placement
struct Position {
    double x = 0.0;
    double y = 0.0;
    
    Position() = default;
    Position(double px, double py) : x(px), y(py) {}
};

// Size structure for dimensions
struct Size {
    double width = 0.0;
    double height = 0.0;
    
    Size() = default;
    Size(double w, double h) : width(w), height(h) {}
    
    bool isValid() const { return width > 0.0 && height > 0.0; }
};

// Part information structure
struct PartInfo {
    std::string name;
    std::string fullPath;
    ProMdl handle = nullptr;
    
    PartInfo() = default;
    PartInfo(const std::string& n, const std::string& path, ProMdl h = nullptr)
        : name(n), fullPath(path), handle(h) {}
};


// Model type enumeration
enum class ModelType {
    PART,
    ASSEMBLY,
    DRAWING,
    UNKNOWN
};

class DrawingInterface {
public:
    DrawingInterface() = default;
    ~DrawingInterface() = default;
    
    // Get current active drawing
    // Returns PRO_TK_NO_ERROR on success, error code otherwise
    ProError getCurrentDrawing(ProDrawing* drawing);
    
    // Get the model associated with a drawing
    // Returns PRO_TK_NO_ERROR on success, error code otherwise
    ProError getAssociatedModel(ProDrawing drawing, ProMdl* model);
    
    // Get part name from a model
    // Returns PRO_TK_NO_ERROR on success, error code otherwise
    ProError getPartName(ProMdl model, std::string& partName);
    
    // Get list of parts in an assembly
    // Returns PRO_TK_NO_ERROR on success, error code otherwise
    ProError getAssemblyParts(ProMdl assembly, std::vector<PartInfo>& parts);
    
    // Insert an image into a drawing at specified position
    // Returns PRO_TK_NO_ERROR on success, error code otherwise
    ProError insertImage(ProDrawing drawing,
                        const std::string& imagePath,
                        const Position& pos,
                        const Size& size);
    
    // Create a symbol instance in the drawing
    // Returns PRO_TK_NO_ERROR on success, error code otherwise
    ProError createSymbolInstance(ProDrawing drawing,
                                  const std::string& symbolName,
                                  const Position& pos);
    
    // Get model type (part, assembly, etc.)
    ModelType getModelType(ProMdl model);
    
    // Check if a drawing is currently open
    bool isDrawingOpen();
    
    // Get last error information
    ErrorInfo getLastError() const { return lastError_; }
    
    // Validate position is within drawing bounds
    bool validatePosition(ProDrawing drawing, const Position& pos);
    
    // Get drawing sheet size
    bool getDrawingSheetSize(ProDrawing drawing, Size& size);
    
private:
    ErrorInfo lastError_;
    
    // Helper to set error info
    void setError(ErrorCode code, const std::string& message, const std::string& details = "");
    
    // Internal helper to extract name from model
    std::string extractModelName(ProMdl model);
    
    // Internal helper to get model path
    std::string getModelPath(ProMdl model);
};

// Utility functions
std::string modelTypeToString(ModelType type);

} // namespace creo_barcode

#endif // DRAWING_INTERFACE_H
