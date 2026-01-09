#include "drawing_interface.h"
#include <algorithm>
#include <fstream>

namespace creo_barcode {

// Note: This implementation provides a simulation layer for testing purposes.
// In actual Creo environment, these methods would call real Creo Toolkit API functions.
// The simulation allows unit testing without requiring Creo to be running.

namespace {

// Simulated state for testing - in real implementation, these would be Creo API calls
static ProDrawing g_currentDrawing = nullptr;
static ProMdl g_associatedModel = nullptr;
static ModelType g_modelType = ModelType::PART;
static std::string g_partName = "";
static std::vector<PartInfo> g_assemblyParts;
static Size g_drawingSheetSize = {297.0, 210.0}; // A4 default

// Test helper functions - these would be removed in production
void setSimulatedDrawing(ProDrawing drawing) { g_currentDrawing = drawing; }
void setSimulatedModel(ProMdl model) { g_associatedModel = model; }
void setSimulatedModelType(ModelType type) { g_modelType = type; }
void setSimulatedPartName(const std::string& name) { g_partName = name; }
void setSimulatedAssemblyParts(const std::vector<PartInfo>& parts) { g_assemblyParts = parts; }
void setSimulatedSheetSize(const Size& size) { g_drawingSheetSize = size; }

} // anonymous namespace

void DrawingInterface::setError(ErrorCode code, const std::string& message, const std::string& details) {
    lastError_ = ErrorInfo(code, message, details);
}

ProError DrawingInterface::getCurrentDrawing(ProDrawing* drawing) {
    if (!drawing) {
        setError(ErrorCode::INVALID_DATA, "Null pointer provided for drawing");
        return PRO_TK_E_GENERAL_ERROR;
    }
    
    // In real implementation: ProMdlCurrentGet(&mdl) then check if it's a drawing
    // For simulation, we use the global state
    if (g_currentDrawing == nullptr) {
        setError(ErrorCode::NO_DRAWING_OPEN, "No drawing is currently open");
        return PRO_TK_E_NOT_FOUND;
    }
    
    *drawing = g_currentDrawing;
    return PRO_TK_NO_ERROR;
}


ProError DrawingInterface::getAssociatedModel(ProDrawing drawing, ProMdl* model) {
    if (!drawing) {
        setError(ErrorCode::NO_DRAWING_OPEN, "Invalid drawing handle");
        return PRO_TK_E_NOT_FOUND;
    }
    
    if (!model) {
        setError(ErrorCode::INVALID_DATA, "Null pointer provided for model");
        return PRO_TK_E_GENERAL_ERROR;
    }
    
    // In real implementation: ProDrawingCurrentSheetGet, ProDrawingViewsCollect, etc.
    // For simulation, we use the global state
    if (g_associatedModel == nullptr) {
        setError(ErrorCode::NO_MODEL_ASSOCIATED, "No model is associated with this drawing");
        return PRO_TK_E_NOT_FOUND;
    }
    
    *model = g_associatedModel;
    return PRO_TK_NO_ERROR;
}

ProError DrawingInterface::getPartName(ProMdl model, std::string& partName) {
    if (!model) {
        setError(ErrorCode::NO_MODEL_ASSOCIATED, "Invalid model handle");
        return PRO_TK_E_NOT_FOUND;
    }
    
    // In real implementation: ProMdlNameGet(model, name)
    // For simulation, we use the global state or extract from model
    std::string name = extractModelName(model);
    
    if (name.empty()) {
        // Fall back to simulated name
        if (g_partName.empty()) {
            setError(ErrorCode::INVALID_DATA, "Could not retrieve part name");
            return PRO_TK_E_GENERAL_ERROR;
        }
        name = g_partName;
    }
    
    partName = name;
    return PRO_TK_NO_ERROR;
}

ProError DrawingInterface::getAssemblyParts(ProMdl assembly, std::vector<PartInfo>& parts) {
    if (!assembly) {
        setError(ErrorCode::NO_MODEL_ASSOCIATED, "Invalid assembly handle");
        return PRO_TK_E_NOT_FOUND;
    }
    
    // Check if model is actually an assembly
    ModelType type = getModelType(assembly);
    if (type != ModelType::ASSEMBLY) {
        setError(ErrorCode::INVALID_DATA, "Model is not an assembly", 
                 "Expected assembly type, got " + modelTypeToString(type));
        return PRO_TK_E_INVALID_TYPE;
    }
    
    // In real implementation: ProSolidFeatVisit with filter for components
    // For simulation, we use the global state
    parts = g_assemblyParts;
    
    return PRO_TK_NO_ERROR;
}


ProError DrawingInterface::insertImage(ProDrawing drawing,
                                       const std::string& imagePath,
                                       const Position& pos,
                                       const Size& size) {
    if (!drawing) {
        setError(ErrorCode::NO_DRAWING_OPEN, "Invalid drawing handle");
        return PRO_TK_E_NOT_FOUND;
    }
    
    if (imagePath.empty()) {
        setError(ErrorCode::INVALID_DATA, "Image path is empty");
        return PRO_TK_E_GENERAL_ERROR;
    }
    
    // Check if image file exists
    std::ifstream file(imagePath);
    if (!file.good()) {
        setError(ErrorCode::FILE_NOT_FOUND, "Image file not found", imagePath);
        return PRO_TK_E_NOT_FOUND;
    }
    file.close();
    
    if (!size.isValid()) {
        setError(ErrorCode::INVALID_SIZE, "Invalid image size specified");
        return PRO_TK_E_GENERAL_ERROR;
    }
    
    // Validate position is within drawing bounds
    if (!validatePosition(drawing, pos)) {
        setError(ErrorCode::INVALID_DATA, "Position is outside drawing bounds");
        return PRO_TK_E_GENERAL_ERROR;
    }
    
    // In real implementation:
    // 1. ProDrawingDraftingEntityCreate or ProDtlnoteCreate
    // 2. ProDtlattachAlloc for attachment
    // 3. ProDtlnoteTextSet with image reference
    // For simulation, we just validate inputs and return success
    
    return PRO_TK_NO_ERROR;
}

ProError DrawingInterface::createSymbolInstance(ProDrawing drawing,
                                                const std::string& symbolName,
                                                const Position& pos) {
    if (!drawing) {
        setError(ErrorCode::NO_DRAWING_OPEN, "Invalid drawing handle");
        return PRO_TK_E_NOT_FOUND;
    }
    
    if (symbolName.empty()) {
        setError(ErrorCode::INVALID_DATA, "Symbol name is empty");
        return PRO_TK_E_GENERAL_ERROR;
    }
    
    // Validate position is within drawing bounds
    if (!validatePosition(drawing, pos)) {
        setError(ErrorCode::INVALID_DATA, "Position is outside drawing bounds");
        return PRO_TK_E_GENERAL_ERROR;
    }
    
    // In real implementation:
    // 1. ProDtlsyminst structure allocation
    // 2. ProDtlsyminstCreate to create the symbol instance
    // 3. ProDtlsyminstAttachmentSet for positioning
    // For simulation, we just validate inputs and return success
    
    return PRO_TK_NO_ERROR;
}


ModelType DrawingInterface::getModelType(ProMdl model) {
    if (!model) {
        return ModelType::UNKNOWN;
    }
    
    // In real implementation: ProMdlTypeGet(model, &type)
    // For simulation, we use the global state
    return g_modelType;
}

bool DrawingInterface::isDrawingOpen() {
    ProDrawing drawing = nullptr;
    return getCurrentDrawing(&drawing) == PRO_TK_NO_ERROR;
}

bool DrawingInterface::validatePosition(ProDrawing drawing, const Position& pos) {
    if (!drawing) {
        return false;
    }
    
    Size sheetSize;
    if (!getDrawingSheetSize(drawing, sheetSize)) {
        // If we can't get sheet size, assume position is valid
        return true;
    }
    
    // Check if position is within sheet bounds (with some margin for negative coords)
    const double margin = 10.0;
    return pos.x >= -margin && pos.x <= sheetSize.width + margin &&
           pos.y >= -margin && pos.y <= sheetSize.height + margin;
}

bool DrawingInterface::getDrawingSheetSize(ProDrawing drawing, Size& size) {
    if (!drawing) {
        return false;
    }
    
    // In real implementation: ProDrawingCurrentSheetGet, ProDrawingSheetSizeGet
    // For simulation, we use the global state
    size = g_drawingSheetSize;
    return true;
}

std::string DrawingInterface::extractModelName(ProMdl model) {
    if (!model) {
        return "";
    }
    
    // In real implementation: ProMdlNameGet(model, name)
    // For simulation, we return the simulated part name
    return g_partName;
}

std::string DrawingInterface::getModelPath(ProMdl model) {
    if (!model) {
        return "";
    }
    
    // In real implementation: ProMdlPathGet(model, path)
    // For simulation, we return empty string
    return "";
}

std::string modelTypeToString(ModelType type) {
    switch (type) {
        case ModelType::PART: return "PART";
        case ModelType::ASSEMBLY: return "ASSEMBLY";
        case ModelType::DRAWING: return "DRAWING";
        case ModelType::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

// Test helper functions exposed for unit testing
// These would be conditionally compiled out in production builds
namespace testing {

void setSimulatedDrawingState(ProDrawing drawing, ProMdl model, 
                              ModelType modelType, const std::string& partName) {
    g_currentDrawing = drawing;
    g_associatedModel = model;
    g_modelType = modelType;
    g_partName = partName;
}

void setSimulatedAssemblyParts(const std::vector<PartInfo>& parts) {
    g_assemblyParts = parts;
}

void setSimulatedSheetSize(double width, double height) {
    g_drawingSheetSize = Size(width, height);
}

void resetSimulatedState() {
    g_currentDrawing = nullptr;
    g_associatedModel = nullptr;
    g_modelType = ModelType::PART;
    g_partName = "";
    g_assemblyParts.clear();
    g_drawingSheetSize = Size(297.0, 210.0);
}

} // namespace testing

} // namespace creo_barcode
