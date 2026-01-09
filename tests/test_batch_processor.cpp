#include <gtest/gtest.h>
#include "batch_processor.h"
#include <filesystem>
#include <fstream>

namespace creo_barcode {
namespace testing {

class BatchProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "batch_test";
        std::filesystem::create_directories(testDir_);
    }
    
    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }
    
    std::filesystem::path testDir_;
    BatchProcessor processor_;
};

TEST_F(BatchProcessorTest, AddFileIncreasesQueueSize) {
    EXPECT_EQ(processor_.getQueueSize(), 0);
    processor_.addFile("file1.drw");
    EXPECT_EQ(processor_.getQueueSize(), 1);
    processor_.addFile("file2.drw");
    EXPECT_EQ(processor_.getQueueSize(), 2);
}

TEST_F(BatchProcessorTest, AddFilesAddsMultiple) {
    processor_.addFiles({"file1.drw", "file2.drw", "file3.drw"});
    EXPECT_EQ(processor_.getQueueSize(), 3);
}

TEST_F(BatchProcessorTest, ClearEmptiesQueue) {
    processor_.addFiles({"file1.drw", "file2.drw"});
    processor_.clear();
    EXPECT_EQ(processor_.getQueueSize(), 0);
}

TEST_F(BatchProcessorTest, ProcessEmptyQueueReturnsEmpty) {
    BarcodeConfig config;
    auto results = processor_.process(config);
    EXPECT_TRUE(results.empty());
}

TEST_F(BatchProcessorTest, ProcessReturnsResultForEachFile) {
    // Create test files
    std::string file1 = (testDir_ / "test1.drw").string();
    std::string file2 = (testDir_ / "test2.drw").string();
    std::ofstream(file1).close();
    std::ofstream(file2).close();
    
    processor_.addFiles({file1, file2});
    
    BarcodeConfig config;
    auto results = processor_.process(config);
    
    EXPECT_EQ(results.size(), 2);
}

TEST_F(BatchProcessorTest, ProcessCallsProgressCallback) {
    std::string file1 = (testDir_ / "test1.drw").string();
    std::ofstream(file1).close();
    
    processor_.addFile(file1);
    
    int callCount = 0;
    BarcodeConfig config;
    processor_.process(config, [&callCount](int current, int total) {
        ++callCount;
    });
    
    EXPECT_EQ(callCount, 1);
}

TEST_F(BatchProcessorTest, GetSummaryCountsCorrectly) {
    std::vector<BatchResult> results = {
        BatchResult("file1.drw", true),
        BatchResult("file2.drw", true),
        BatchResult("file3.drw", false, "Error message")
    };
    
    std::string summary = BatchProcessor::getSummary(results);
    
    EXPECT_NE(summary.find("Total files: 3"), std::string::npos);
    EXPECT_NE(summary.find("Successful: 2"), std::string::npos);
    EXPECT_NE(summary.find("Failed: 1"), std::string::npos);
}

// Unit tests for Requirements 5.2, 5.3, 5.4

// Test empty list processing (需求 5.2)
TEST_F(BatchProcessorTest, EmptyListProcessingReturnsEmptyResults) {
    // Verify that processing an empty queue returns empty results
    BarcodeConfig config;
    auto results = processor_.process(config);
    
    EXPECT_TRUE(results.empty());
    EXPECT_EQ(results.size(), 0);
    
    // Summary should show zero files
    std::string summary = BatchProcessor::getSummary(results);
    EXPECT_NE(summary.find("Total files: 0"), std::string::npos);
    EXPECT_NE(summary.find("Successful: 0"), std::string::npos);
    EXPECT_NE(summary.find("Failed: 0"), std::string::npos);
}

// Test partial failure - some files exist, some don't (需求 5.3)
TEST_F(BatchProcessorTest, PartialFailureContinuesProcessing) {
    // Create one valid file
    std::string validFile = (testDir_ / "valid.drw").string();
    std::ofstream(validFile).close();
    
    // Add valid file and non-existent file
    std::string invalidFile = (testDir_ / "nonexistent.drw").string();
    
    processor_.addFile(validFile);
    processor_.addFile(invalidFile);
    processor_.addFile(validFile);  // Add another valid file
    
    BarcodeConfig config;
    auto results = processor_.process(config);
    
    // All files should have results (processing continues despite failures)
    EXPECT_EQ(results.size(), 3);
    
    // Count successes and failures
    int successCount = 0;
    int failureCount = 0;
    for (const auto& result : results) {
        if (result.success) {
            ++successCount;
        } else {
            ++failureCount;
        }
    }
    
    EXPECT_EQ(successCount, 2);  // Two valid files
    EXPECT_EQ(failureCount, 1);  // One non-existent file
}

// Test that failed files have error messages (需求 5.3)
TEST_F(BatchProcessorTest, FailedFilesHaveErrorMessages) {
    std::string invalidFile = (testDir_ / "nonexistent.drw").string();
    processor_.addFile(invalidFile);
    
    BarcodeConfig config;
    auto results = processor_.process(config);
    
    ASSERT_EQ(results.size(), 1);
    EXPECT_FALSE(results[0].success);
    EXPECT_FALSE(results[0].errorMessage.empty());
    EXPECT_EQ(results[0].filePath, invalidFile);
}

// Test summary with partial failures (需求 5.4)
TEST_F(BatchProcessorTest, SummaryShowsPartialFailureDetails) {
    std::vector<BatchResult> results = {
        BatchResult("file1.drw", true),
        BatchResult("file2.drw", false, "File not found"),
        BatchResult("file3.drw", true),
        BatchResult("file4.drw", false, "Permission denied")
    };
    
    std::string summary = BatchProcessor::getSummary(results);
    
    // Verify counts
    EXPECT_NE(summary.find("Total files: 4"), std::string::npos);
    EXPECT_NE(summary.find("Successful: 2"), std::string::npos);
    EXPECT_NE(summary.find("Failed: 2"), std::string::npos);
    
    // Verify failure details are included
    EXPECT_NE(summary.find("file2.drw"), std::string::npos);
    EXPECT_NE(summary.find("File not found"), std::string::npos);
    EXPECT_NE(summary.find("file4.drw"), std::string::npos);
    EXPECT_NE(summary.find("Permission denied"), std::string::npos);
}

// Test progress callback is called for each file including failures (需求 5.2)
TEST_F(BatchProcessorTest, ProgressCallbackCalledForAllFilesIncludingFailures) {
    std::string validFile = (testDir_ / "valid.drw").string();
    std::ofstream(validFile).close();
    std::string invalidFile = (testDir_ / "nonexistent.drw").string();
    
    processor_.addFile(validFile);
    processor_.addFile(invalidFile);
    processor_.addFile(validFile);
    
    std::vector<std::pair<int, int>> progressCalls;
    BarcodeConfig config;
    processor_.process(config, [&progressCalls](int current, int total) {
        progressCalls.push_back({current, total});
    });
    
    // Progress callback should be called for each file
    EXPECT_EQ(progressCalls.size(), 3);
    
    // Verify progress values
    EXPECT_EQ(progressCalls[0], std::make_pair(1, 3));
    EXPECT_EQ(progressCalls[1], std::make_pair(2, 3));
    EXPECT_EQ(progressCalls[2], std::make_pair(3, 3));
}

// Test all files fail scenario (需求 5.3, 5.4)
TEST_F(BatchProcessorTest, AllFilesFailScenario) {
    processor_.addFile((testDir_ / "nonexistent1.drw").string());
    processor_.addFile((testDir_ / "nonexistent2.drw").string());
    
    BarcodeConfig config;
    auto results = processor_.process(config);
    
    EXPECT_EQ(results.size(), 2);
    
    // All should fail
    for (const auto& result : results) {
        EXPECT_FALSE(result.success);
        EXPECT_FALSE(result.errorMessage.empty());
    }
    
    std::string summary = BatchProcessor::getSummary(results);
    EXPECT_NE(summary.find("Successful: 0"), std::string::npos);
    EXPECT_NE(summary.find("Failed: 2"), std::string::npos);
}

} // namespace testing
} // namespace creo_barcode
