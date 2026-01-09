#include "batch_processor.h"
#include <sstream>
#include <fstream>

namespace creo_barcode {

void BatchProcessor::addFile(const std::string& filePath) {
    fileQueue_.push_back(filePath);
}

void BatchProcessor::addFiles(const std::vector<std::string>& filePaths) {
    fileQueue_.insert(fileQueue_.end(), filePaths.begin(), filePaths.end());
}

void BatchProcessor::clear() {
    fileQueue_.clear();
}

std::vector<BatchResult> BatchProcessor::process(const BarcodeConfig& config,
                                                  ProgressCallback progressCallback) {
    std::vector<BatchResult> results;
    results.reserve(fileQueue_.size());
    
    int total = static_cast<int>(fileQueue_.size());
    int current = 0;
    
    BarcodeGenerator generator;
    
    for (const auto& filePath : fileQueue_) {
        ++current;
        
        if (progressCallback) {
            progressCallback(current, total);
        }
        
        // For now, simulate processing - actual implementation would:
        // 1. Open the Creo drawing file
        // 2. Extract part name
        // 3. Generate barcode
        // 4. Insert into drawing
        
        // Check if file exists (basic validation)
        std::ifstream file(filePath);
        if (!file.good()) {
            results.emplace_back(filePath, false, "File not found");
            continue;
        }
        
        // Placeholder: In real implementation, this would process the drawing
        results.emplace_back(filePath, true, "");
    }
    
    return results;
}

std::string BatchProcessor::getSummary(const std::vector<BatchResult>& results) {
    int successCount = 0;
    int failureCount = 0;
    std::vector<std::string> failures;
    
    for (const auto& result : results) {
        if (result.success) {
            ++successCount;
        } else {
            ++failureCount;
            failures.push_back(result.filePath + ": " + result.errorMessage);
        }
    }
    
    std::ostringstream summary;
    summary << "Batch Processing Summary\n";
    summary << "========================\n";
    summary << "Total files: " << results.size() << "\n";
    summary << "Successful: " << successCount << "\n";
    summary << "Failed: " << failureCount << "\n";
    
    if (!failures.empty()) {
        summary << "\nFailure details:\n";
        for (const auto& failure : failures) {
            summary << "  - " << failure << "\n";
        }
    }
    
    return summary.str();
}

} // namespace creo_barcode
