/**
 * @file barcode_c_api.h
 * @brief C API wrapper for barcode functionality
 * 
 * This provides a C-compatible interface to the C++ barcode functionality,
 * allowing the pure C Pro/TOOLKIT entry point to call C++ code.
 */

#ifndef BARCODE_C_API_H
#define BARCODE_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* Barcode types */
typedef enum {
    BARCODE_CODE_128 = 0,
    BARCODE_CODE_39 = 1,
    BARCODE_QR_CODE = 2,
    BARCODE_DATA_MATRIX = 3,
    BARCODE_EAN_13 = 4
} BarcodeTypeC;

/* Barcode configuration */
typedef struct {
    BarcodeTypeC type;
    int width;
    int height;
    int margin;
    int showText;
    int dpi;
} BarcodeConfigC;

/* Initialize the barcode module */
int barcode_init(void);

/* Cleanup the barcode module */
void barcode_cleanup(void);

/* Generate a barcode image
 * @param data The data to encode
 * @param config Barcode configuration
 * @param outputPath Path to save the barcode image
 * @return 0 on success, non-zero on error
 */
int barcode_generate(const char* data, const BarcodeConfigC* config, const char* outputPath);

/* Decode a barcode from image
 * @param imagePath Path to the barcode image
 * @param outputBuffer Buffer to store decoded data
 * @param bufferSize Size of the output buffer
 * @return 0 on success, non-zero on error
 */
int barcode_decode(const char* imagePath, char* outputBuffer, int bufferSize);

/* Load configuration from file
 * @param configPath Path to config file
 * @return 0 on success, non-zero on error
 */
int config_load(const char* configPath);

/* Save configuration to file
 * @param configPath Path to config file
 * @return 0 on success, non-zero on error
 */
int config_save(const char* configPath);

/* Get default barcode configuration */
void config_get_defaults(BarcodeConfigC* config);

/* Check data synchronization
 * @param partName Current part name
 * @param barcodeData Data from barcode
 * @return 1 if in sync, 0 if out of sync, -1 on error
 */
int sync_check(const char* partName, const char* barcodeData);

/* Get last error message */
const char* barcode_get_last_error(void);

/* Get current configuration */
void config_get_current(BarcodeConfigC* config);

/* Set current configuration */
void config_set_current(const BarcodeConfigC* config);

/* Get barcode type name */
const char* barcode_type_name(BarcodeTypeC type);

/* Drawing position structure */
typedef struct {
    double x;               /* X coordinate (drawing units) */
    double y;               /* Y coordinate (drawing units) */
    int sheet;              /* Drawing sheet number */
} DrawingPositionC;

/* Settings data structure for dialog */
typedef struct {
    BarcodeTypeC type;      /* Barcode type */
    int width;              /* Width (pixels) */
    int height;             /* Height (pixels) */
    int dpi;                /* DPI */
} SettingsDataC;

/* Part info for batch processing */
typedef struct {
    char name[256];         /* Part name */
    char fullPath[512];     /* Full file path */
    int index;              /* Index in BOM */
} PartInfoC;

/* Batch barcode result */
typedef struct {
    char partName[256];     /* Part name */
    char imagePath[512];    /* Generated barcode image path */
    int success;            /* 1 if successful, 0 if failed */
    char errorMsg[256];     /* Error message if failed */
} BatchBarcodeResultC;

/* Get image dimensions from file
 * @param imagePath Path to image file
 * @param width Output width
 * @param height Output height
 * @return 0 on success, non-zero on error
 */
int barcode_get_image_size(const char* imagePath, int* width, int* height);

/* Generate barcode for a part and return image path
 * @param partName Part name to encode
 * @param config Barcode configuration
 * @param outputDir Output directory for barcode image
 * @param outputPath Buffer to receive full output path
 * @param pathSize Size of outputPath buffer
 * @return 0 on success, non-zero on error
 */
int barcode_generate_for_part(const char* partName, 
                              const BarcodeConfigC* config,
                              const char* outputDir,
                              char* outputPath,
                              int pathSize);

/* ============================================================================
 * COM Bridge Functions for VB/COM API Image Insertion
 * ============================================================================ */

/* Initialize COM bridge for Creo VB API
 * Must be called before using image insertion functions.
 * @return 0 on success, non-zero on error
 */
int com_bridge_init(void);

/* Cleanup COM bridge resources
 * Should be called when plugin unloads.
 */
void com_bridge_cleanup(void);

/* Check if COM bridge is initialized
 * @return 1 if initialized, 0 if not
 */
int com_bridge_is_initialized(void);

/* Insert image into current drawing via VB/COM API
 * @param imagePath Path to image file (PNG, JPG, BMP supported)
 * @param x X coordinate (drawing units)
 * @param y Y coordinate (drawing units)
 * @param width Image width (drawing units, 0 = auto)
 * @param height Image height (drawing units, 0 = auto)
 * @return 0 on success, non-zero on error
 */
int barcode_insert_image(const char* imagePath,
                         double x, double y,
                         double width, double height);

/* Batch insert result structure */
typedef struct {
    int totalCount;         /* Total images attempted */
    int successCount;       /* Successfully inserted */
    int failCount;          /* Failed to insert */
} BatchInsertResultC;

/* Batch insert images into current drawing
 * @param imagePaths Array of image file paths
 * @param positions Array of positions (x, y pairs, so length = count * 2)
 * @param count Number of images to insert
 * @param width Uniform width for all images (0 = auto)
 * @param height Uniform height for all images (0 = auto)
 * @param result Output result structure (can be NULL)
 * @return Number of successfully inserted images
 */
int barcode_batch_insert_images(const char** imagePaths,
                                const double* positions,
                                int count,
                                double width, double height,
                                BatchInsertResultC* result);

/* Batch insert images in grid layout
 * @param imagePaths Array of image file paths
 * @param count Number of images
 * @param startX Starting X coordinate
 * @param startY Starting Y coordinate
 * @param width Image width
 * @param height Image height
 * @param columns Number of columns in grid
 * @param spacing Spacing between images
 * @param result Output result structure (can be NULL)
 * @return Number of successfully inserted images
 */
int barcode_batch_insert_images_grid(const char** imagePaths,
                                     int count,
                                     double startX, double startY,
                                     double width, double height,
                                     int columns, double spacing,
                                     BatchInsertResultC* result);

/* Get last COM bridge error message
 * @return Error message string (valid until next COM bridge call)
 */
const char* com_bridge_get_last_error(void);

/* ============================================================================
 * Fallback Mechanism for Image Insertion
 * ============================================================================ */

/* Fallback result codes */
typedef enum {
    FALLBACK_SUCCESS_COM = 0,       /* Success via COM API */
    FALLBACK_SUCCESS_NOTE = 1,      /* Success via note fallback */
    FALLBACK_FAILED = -1            /* Both methods failed */
} FallbackResultCode;

/* Insert image with automatic fallback to note-based approach
 * Tries COM bridge first, falls back to note insertion if COM fails.
 * 
 * @param imagePath Path to image file
 * @param x X coordinate (drawing units)
 * @param y Y coordinate (drawing units)
 * @param width Image width (drawing units, 0 = auto)
 * @param height Image height (drawing units, 0 = auto)
 * @param partName Part name for note text (used in fallback)
 * @param usedFallback Output: set to 1 if fallback was used, 0 if COM succeeded (can be NULL)
 * @return FALLBACK_SUCCESS_COM (0) if COM succeeded,
 *         FALLBACK_SUCCESS_NOTE (1) if fallback succeeded,
 *         FALLBACK_FAILED (-1) if both failed
 */
int barcode_insert_image_with_fallback(const char* imagePath,
                                       double x, double y,
                                       double width, double height,
                                       const char* partName,
                                       int* usedFallback);

/* Get formatted HRESULT error string
 * Converts HRESULT to human-readable error message.
 * 
 * @param hr HRESULT error code
 * @param buffer Output buffer for error string
 * @param bufferSize Size of output buffer
 * @return 0 on success, non-zero on error
 */
int com_bridge_format_hresult(long hr, char* buffer, int bufferSize);

#ifdef __cplusplus
}
#endif

#endif /* BARCODE_C_API_H */
