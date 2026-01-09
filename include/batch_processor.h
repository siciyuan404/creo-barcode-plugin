#ifndef BATCH_PROCESSOR_H
#define BATCH_PROCESSOR_H

#include <string>
#include <vector>
#include <functional>
#include "barcode_generator.h"

namespace creo_barcode {

struct BatchResult {
    std::string filePath;
    bool success;
    std::string errorMessage;
    
    BatchResult() : success(false) {}
    BatchResult(const std::string& path, bool ok, const std::string& err = "")
        : filePath(path), success(ok), errorMessage(err) {}
};

class BatchProcessor {
public:
    using ProgressCallback = std::function<void(int current, int total)>;
    
    BatchProcessor() = default;
    ~BatchProcessor() = default;
    
    // Add file to processing queue
    void addFile(const std::string& filePath);
    
    // Add multiple files
    void addFiles(const std::vector<std::string>& filePaths);
    
    // Clear the queue
    void clear();
    
    // Get queue size
    size_t getQueueSize() const { return fileQueue_.size(); }
    
    // Execute batch processing
    std::vector<BatchResult> process(const BarcodeConfig& config,
                                     ProgressCallback progressCallback = nullptr);
    
    // Get processing summary
    static std::string getSummary(const std::vector<BatchResult>& results);
    
private:
    std::vector<std::string> fileQueue_;
};

} // namespace creo_barcode

#endif // BATCH_PROCESSOR_H
