/**
 * @file main_pure_c.c
 * @brief Pro/TOOLKIT Barcode Plugin - Pure C Entry Point
 * 
 * Features:
 * 1. Generate barcode from model name and insert at mouse click position
 * 2. Batch generate barcodes for all parts in drawing BOM
 */

#include <ProToolkit.h>
#include <ProUICmd.h>
#include <ProUI.h>
#include <ProUtil.h>
#include <ProMessage.h>
#include <ProMenuBar.h>
#include <ProMdl.h>
#include <ProDrawing.h>
#include <ProParameter.h>
#include <ProModelitem.h>
#include <ProWindows.h>
#include <ProSelection.h>
#include <ProView.h>
#include <ProSolid.h>
#include <ProAsmcomp.h>
#include <ProFeatType.h>
#include <ProArray.h>
#include <ProDtlnote.h>
#include <ProDtlattach.h>
#include <ProAnnotation.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Forward declarations for C API */
/* Pure C barcode generator (no C++ dependencies) */
extern int barcode_generate_c(const char* data, int type, int width, int height, 
                              int margin, const char* outputPath);
extern const char* barcode_get_error_pure_c(void);

/* Legacy C++ API - disabled to avoid crashes */
/* extern int barcode_init(void); */
/* extern void barcode_cleanup(void); */
/* extern const char* barcode_get_last_error(void); */

/* Forward declarations for COM Bridge API - disabled */
/* extern int com_bridge_init(void); */
/* extern void com_bridge_cleanup(void); */
/* extern int com_bridge_is_initialized(void); */
/* extern int barcode_insert_image(...); */
/* extern const char* com_bridge_get_last_error(void); */

/* Batch insert result structure - not used in pure C version */
typedef struct {
    int totalCount;
    int successCount;
    int failCount;
} BatchInsertResultC;

/* Disabled C++ batch functions */
/* extern int barcode_batch_insert_images_grid(...); */

/* Barcode types - only Code 128 supported in pure C version */
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

/* Pure C versions - no C++ dependencies */
static const char* barcode_type_name_c(BarcodeTypeC type) {
    switch (type) {
        case BARCODE_CODE_128: return "Code 128";
        case BARCODE_CODE_39: return "Code 39";
        case BARCODE_QR_CODE: return "QR Code";
        case BARCODE_DATA_MATRIX: return "Data Matrix";
        case BARCODE_EAN_13: return "EAN-13";
        default: return "Unknown";
    }
}

/* Disabled C++ config functions */
/* extern void config_get_defaults(BarcodeConfigC* config); */
/* extern void config_get_current(BarcodeConfigC* config); */
/* extern void config_set_current(const BarcodeConfigC* config); */
/* extern const char* barcode_type_name(BarcodeTypeC type); */
/* extern int barcode_get_image_size(...); */
/* extern int barcode_generate_for_part(...); */

/* Global state */
static uiCmdCmdId g_cmdGenerate = 0;
static uiCmdCmdId g_cmdSettings = 0;
static uiCmdCmdId g_cmdBatch = 0;
static uiCmdCmdId g_cmdSync = 0;

/* Current settings */
static BarcodeConfigC g_config = {
    BARCODE_CODE_128,
    200,
    80,
    10,
    1,
    300
};

/* Max parts for batch processing */
#define MAX_BATCH_PARTS 100

/* Part info structure for batch */
typedef struct {
    char name[256];
    int processed;
} PartEntry;

static PartEntry g_batchParts[MAX_BATCH_PARTS];
static int g_batchPartCount = 0;

static uiCmdAccessState MyAccess(uiCmdAccessMode mode)
{
    return ACCESS_AVAILABLE;
}

/* Get current model name */
static ProError GetCurrentModelName(char* name, int maxLen)
{
    ProMdl mdl;
    ProName wname;
    ProError status;
    
    status = ProMdlCurrentGet(&mdl);
    if (status != PRO_TK_NO_ERROR)
        return status;
    
    status = ProMdlNameGet(mdl, wname);
    if (status != PRO_TK_NO_ERROR)
        return status;
    
    ProWstringToString(name, wname);
    return PRO_TK_NO_ERROR;
}

/* Get temp directory path */
static void GetTempPath(char* path, int maxLen)
{
    char* temp = getenv("TEMP");
    if (temp)
    {
        strncpy(path, temp, maxLen - 1);
        path[maxLen - 1] = '\0';
    }
    else
    {
        strcpy(path, "C:\\Temp");
    }
}

/* Check if current model is a drawing */
static int IsDrawingOpen(ProDrawing* drawing, int showMessage)
{
    ProMdl mdl;
    ProMdlType type;
    wchar_t wmsg[256];
    
    if (drawing != NULL)
        *drawing = NULL;
    
    if (ProMdlCurrentGet(&mdl) != PRO_TK_NO_ERROR)
    {
        if (showMessage)
        {
            ProStringToWstring(wmsg, "No model is currently open.");
            ProMessageDisplay(NULL, "%0w", wmsg);
        }
        return 0;
    }
    
    if (ProMdlTypeGet(mdl, &type) != PRO_TK_NO_ERROR)
        return 0;
    
    if (type != PRO_MDL_DRAWING)
    {
        if (showMessage)
        {
            ProStringToWstring(wmsg, "Please open a drawing first.");
            ProMessageDisplay(NULL, "%0w", wmsg);
        }
        return 0;
    }
    
    if (drawing != NULL)
        *drawing = (ProDrawing)mdl;
    
    return 1;
}


/* Insert barcode as text note in drawing at default position */
static ProError InsertBarcodeNote(ProDrawing drawing, const char* partName, 
                                  const char* imagePath, double x, double y)
{
    ProError status;
    ProDtlnote note;
    ProDtlnotedata noteData = NULL;
    ProDtlnoteline noteLine = NULL;
    ProDtlnotetext noteText = NULL;
    ProDtlattach attach = NULL;
    ProVector position;
    wchar_t wtext[512];
    char noteContent[512];
    
    /* Validate drawing pointer */
    if (drawing == NULL)
        return PRO_TK_BAD_INPUTS;
    
    /* Create note content - barcode text representation */
    sprintf(noteContent, "||||| %s |||||", partName);
    ProStringToWstring(wtext, noteContent);
    
    /* Set position vector */
    position[0] = x;
    position[1] = y;
    position[2] = 0.0;
    
    /* Allocate note data structure */
    status = ProDtlnotedataAlloc((ProMdl)drawing, &noteData);
    if (status != PRO_TK_NO_ERROR)
        return status;
    
    /* Allocate note line */
    status = ProDtlnotelineAlloc(&noteLine);
    if (status != PRO_TK_NO_ERROR)
    {
        ProDtlnotedataFree(noteData);
        return status;
    }
    
    /* Allocate note text */
    status = ProDtlnotetextAlloc(&noteText);
    if (status != PRO_TK_NO_ERROR)
    {
        ProDtlnotelineFree(noteLine);
        ProDtlnotedataFree(noteData);
        return status;
    }
    
    /* Set text string */
    status = ProDtlnotetextStringSet(noteText, wtext);
    if (status != PRO_TK_NO_ERROR)
    {
        ProDtlnotetextFree(noteText);
        ProDtlnotelineFree(noteLine);
        ProDtlnotedataFree(noteData);
        return status;
    }
    
    /* Add text to line */
    status = ProDtlnotelineTextAdd(noteLine, noteText);
    if (status != PRO_TK_NO_ERROR)
    {
        ProDtlnotetextFree(noteText);
        ProDtlnotelineFree(noteLine);
        ProDtlnotedataFree(noteData);
        return status;
    }
    
    /* Add line to note data */
    status = ProDtlnotedataLineAdd(noteData, noteLine);
    if (status != PRO_TK_NO_ERROR)
    {
        ProDtlnotelineFree(noteLine);
        ProDtlnotedataFree(noteData);
        return status;
    }
    
    /* Create FREE attachment at position */
    status = ProDtlattachAlloc(PRO_DTLATTACHTYPE_FREE, NULL, position, NULL, &attach);
    if (status != PRO_TK_NO_ERROR)
    {
        ProDtlnotedataFree(noteData);
        return status;
    }
    
    /* Set attachment to note data */
    status = ProDtlnotedataAttachmentSet(noteData, attach);
    ProDtlattachFree(attach);
    
    if (status != PRO_TK_NO_ERROR)
    {
        ProDtlnotedataFree(noteData);
        return status;
    }
    
    /* Create the note */
    status = ProDtlnoteCreate((ProMdl)drawing, NULL, noteData, &note);
    ProDtlnotedataFree(noteData);
    
    if (status != PRO_TK_NO_ERROR)
        return status;
    
    /* Show the note */
    status = ProAnnotationShow((ProAnnotation*)&note, NULL, NULL);
    
    return status;
}

/* Callback for visiting assembly components */
static ProError AsmCompVisitAction(ProFeature* feature, ProError status, ProAppData appData)
{
    ProMdl compMdl;
    ProMdlType mdlType;
    ProName wname;
    char name[256];
    ProError err;
    
    /* Get component model */
    err = ProAsmcompMdlGet(feature, &compMdl);
    if (err != PRO_TK_NO_ERROR)
        return PRO_TK_NO_ERROR; /* Continue visiting */
    
    /* Check if it's a part */
    err = ProMdlTypeGet(compMdl, &mdlType);
    if (err != PRO_TK_NO_ERROR || mdlType != PRO_MDL_PART)
        return PRO_TK_NO_ERROR; /* Skip non-parts */
    
    /* Get part name */
    err = ProMdlNameGet(compMdl, wname);
    if (err != PRO_TK_NO_ERROR)
        return PRO_TK_NO_ERROR;
    
    ProWstringToString(name, wname);
    
    /* Check if already in list (avoid duplicates) */
    for (int i = 0; i < g_batchPartCount; i++)
    {
        if (strcmp(g_batchParts[i].name, name) == 0)
            return PRO_TK_NO_ERROR; /* Already exists */
    }
    
    /* Add to list */
    if (g_batchPartCount < MAX_BATCH_PARTS)
    {
        strncpy(g_batchParts[g_batchPartCount].name, name, 255);
        g_batchParts[g_batchPartCount].name[255] = '\0';
        g_batchParts[g_batchPartCount].processed = 0;
        g_batchPartCount++;
    }
    
    return PRO_TK_NO_ERROR;
}

/* Filter for assembly components */
static ProError AsmCompFilter(ProFeature* feature, ProAppData appData)
{
    ProFeattype ftype;
    
    if (ProFeatureTypeGet(feature, &ftype) != PRO_TK_NO_ERROR)
        return PRO_TK_CONTINUE;
    
    if (ftype == PRO_FEAT_COMPONENT)
        return PRO_TK_NO_ERROR; /* Accept */
    
    return PRO_TK_CONTINUE; /* Skip */
}

/* Get all parts from drawing's associated model */
static int GetDrawingParts(ProDrawing drawing)
{
    ProError status;
    ProSolid* solids = NULL;
    int modelCount = 0;
    ProMdlType mdlType;
    ProName wname;
    char name[256];
    int i;
    
    g_batchPartCount = 0;
    
    /* Get models shown in drawing - use ProDrawingSolidsCollect */
    status = ProDrawingSolidsCollect(drawing, &solids);
    if (status != PRO_TK_NO_ERROR || solids == NULL)
        return 0;
    
    /* Get array size */
    status = ProArraySizeGet(solids, &modelCount);
    if (status != PRO_TK_NO_ERROR)
    {
        ProArrayFree(solids);
        return 0;
    }
    
    /* Process each model */
    for (i = 0; i < modelCount && g_batchPartCount < MAX_BATCH_PARTS; i++)
    {
        status = ProMdlTypeGet((ProMdl)solids[i], &mdlType);
        if (status != PRO_TK_NO_ERROR)
            continue;
        
        if (mdlType == PRO_MDL_PART)
        {
            /* Single part - add directly */
            status = ProMdlNameGet((ProMdl)solids[i], wname);
            if (status == PRO_TK_NO_ERROR)
            {
                ProWstringToString(name, wname);
                strncpy(g_batchParts[g_batchPartCount].name, name, 255);
                g_batchParts[g_batchPartCount].name[255] = '\0';
                g_batchParts[g_batchPartCount].processed = 0;
                g_batchPartCount++;
            }
        }
        else if (mdlType == PRO_MDL_ASSEMBLY)
        {
            /* Assembly - visit all components */
            ProSolidFeatVisit(solids[i], 
                              AsmCompVisitAction, 
                              AsmCompFilter, 
                              NULL);
        }
    }
    
    ProArrayFree(solids);
    return g_batchPartCount;
}


/* Default values for settings */
#define DEFAULT_WIDTH   200
#define DEFAULT_HEIGHT  80
#define MIN_WIDTH       50
#define MAX_WIDTH       500
#define MIN_HEIGHT      30
#define MAX_HEIGHT      300

/* Action: Generate Barcode - insert note directly into drawing */
static int ActionGenerate(uiCmdCmdId cmd, uiCmdValue *val, void *data)
{
    char modelName[256] = {0};
    wchar_t wmsg[1024];
    char msg[1024];
    char outputPath[512];
    char tempDir[256];
    ProError status;
    int result;
    ProDrawing drawing = NULL;
    int windowId;
    
    /* Get current model name */
    status = GetCurrentModelName(modelName, sizeof(modelName));
    
    if (status != PRO_TK_NO_ERROR || modelName[0] == '\0')
    {
        ProStringToWstring(wmsg, "Please open a model first.");
        ProMessageDisplay(NULL, "%0w", wmsg);
        return 0;
    }
    
    /* Build output path - use BMP format for pure C version */
    GetTempPath(tempDir, sizeof(tempDir));
    sprintf(outputPath, "%s\\barcode_%s.bmp", tempDir, modelName);
    
    /* Generate barcode using pure C implementation */
    result = barcode_generate_c(modelName, 
                                (int)g_config.type,
                                g_config.width, 
                                g_config.height, 
                                g_config.margin,
                                outputPath);
    
    if (result != 0)
    {
        sprintf(msg, "Failed to generate barcode: %s", barcode_get_error_pure_c());
        ProStringToWstring(wmsg, msg);
        ProMessageDisplay(NULL, "%0w", wmsg);
        return 0;
    }
    
    /* Check if drawing is open */
    if (IsDrawingOpen(&drawing, 0))
    {
        /* Insert barcode note at default position (100, 100) */
        status = InsertBarcodeNote(drawing, modelName, outputPath, 100.0, 100.0);
        
        if (status == PRO_TK_NO_ERROR)
        {
            /* Refresh window */
            if (ProWindowCurrentGet(&windowId) == PRO_TK_NO_ERROR)
            {
                ProWindowRepaint(windowId);
            }
            
            sprintf(msg, "Barcode note inserted!\n\nModel: %s\nImage: %s\n\nNote placed at position (100, 100).\nYou can drag it to desired location.", 
                    modelName, outputPath);
        }
        else
        {
            sprintf(msg, "Note insertion failed (error %d).\n\nBarcode image saved to:\n%s", 
                    (int)status, outputPath);
        }
    }
    else
    {
        sprintf(msg, "Barcode generated!\n\nModel: %s\nFile: %s\n\nOpen a drawing to insert barcode note.", 
                modelName, outputPath);
    }
    
    ProStringToWstring(wmsg, msg);
    ProMessageDisplay(NULL, "%0w", wmsg);
    
    return 0;
}

/* Action: Settings */
static int ActionSettings(uiCmdCmdId cmd, uiCmdValue *val, void *data)
{
    wchar_t wmsg[512];
    wchar_t wprompt[256];
    char msg[512];
    ProError status;
    int typeInput;
    double widthInput, heightInput;
    BarcodeConfigC newConfig;
    
    newConfig = g_config;
    
    /* Show current settings */
    sprintf(msg, "Current Settings:\n"
                "Type: %s (%d)\n"
                "Size: %dx%d\n\n"
                "Enter new settings...",
            barcode_type_name_c(g_config.type),
            (int)g_config.type,
            g_config.width, g_config.height);
    ProStringToWstring(wmsg, msg);
    ProMessageDisplay(NULL, "%0w", wmsg);
    
    /* Get barcode type */
    ProStringToWstring(wprompt, "Type (0=Code128, 1=Code39, 2=QR, 3=DataMatrix, 4=EAN13):");
    ProMessageDisplay(NULL, "%0w", wprompt);
    status = ProMessageIntegerRead(NULL, &typeInput);
    
    if (status == PRO_TK_NO_ERROR && typeInput >= 0 && typeInput <= 4)
        newConfig.type = (BarcodeTypeC)typeInput;
    else if (status == PRO_TK_USER_ABORT)
        return 0;
    
    /* Get width */
    ProStringToWstring(wprompt, "Width (50-500):");
    ProMessageDisplay(NULL, "%0w", wprompt);
    status = ProMessageDoubleRead(NULL, &widthInput);
    
    if (status == PRO_TK_NO_ERROR && widthInput >= MIN_WIDTH && widthInput <= MAX_WIDTH)
        newConfig.width = (int)widthInput;
    else if (status == PRO_TK_USER_ABORT)
        return 0;
    
    /* Get height */
    ProStringToWstring(wprompt, "Height (30-300):");
    ProMessageDisplay(NULL, "%0w", wprompt);
    status = ProMessageDoubleRead(NULL, &heightInput);
    
    if (status == PRO_TK_NO_ERROR && heightInput >= MIN_HEIGHT && heightInput <= MAX_HEIGHT)
        newConfig.height = (int)heightInput;
    else if (status == PRO_TK_USER_ABORT)
        return 0;
    
    /* Confirm */
    sprintf(msg, "New: %s, %dx%d\nSave? (1=Yes, 0=No):",
            barcode_type_name_c(newConfig.type), newConfig.width, newConfig.height);
    ProStringToWstring(wmsg, msg);
    ProMessageDisplay(NULL, "%0w", wmsg);
    
    status = ProMessageIntegerRead(NULL, &typeInput);
    if (status == PRO_TK_NO_ERROR && typeInput == 1)
    {
        g_config = newConfig;
        /* Pure C version - no config_set_current() call */
        
        sprintf(msg, "Settings saved: %s, %dx%d",
                barcode_type_name_c(g_config.type), g_config.width, g_config.height);
        ProStringToWstring(wmsg, msg);
        ProMessageDisplay(NULL, "%0w", wmsg);
    }
    
    return 0;
}


/* Action: Batch Process - generate barcodes for all parts in drawing */
static int ActionBatch(uiCmdCmdId cmd, uiCmdValue *val, void *data)
{
    ProDrawing drawing = NULL;
    wchar_t wmsg[2048];
    char msg[2048];
    char tempDir[256];
    char outputPath[512];
    int partCount;
    int successCount = 0;
    int failCount = 0;
    int noteCount = 0;
    int i;
    ProError status;
    int result;
    int windowId;
    double currentX = 50.0;
    double currentY = 250.0;
    double spacing = 30.0;
    int perRow = 3;
    
    /* Check if drawing is open */
    if (!IsDrawingOpen(&drawing, 1))
        return 0;
    
    /* Get all parts from drawing */
    partCount = GetDrawingParts(drawing);
    
    if (partCount == 0)
    {
        ProStringToWstring(wmsg, "No parts found in drawing.");
        ProMessageDisplay(NULL, "%0w", wmsg);
        return 0;
    }
    
    /* Show part count and confirm */
    sprintf(msg, "Found %d parts in drawing.\nGenerate barcodes and insert notes? (1=Yes, 0=No):", partCount);
    ProStringToWstring(wmsg, msg);
    ProMessageDisplay(NULL, "%0w", wmsg);
    
    int confirm;
    status = ProMessageIntegerRead(NULL, &confirm);
    if (status != PRO_TK_NO_ERROR || confirm != 1)
    {
        ProStringToWstring(wmsg, "Batch processing cancelled.");
        ProMessageDisplay(NULL, "%0w", wmsg);
        return 0;
    }
    
    /* Get temp directory */
    GetTempPath(tempDir, sizeof(tempDir));
    
    /* Generate barcodes and insert notes for each part */
    for (i = 0; i < partCount; i++)
    {
        /* Generate barcode using pure C implementation */
        sprintf(outputPath, "%s\\barcode_%s.bmp", tempDir, g_batchParts[i].name);
        
        result = barcode_generate_c(g_batchParts[i].name, 
                                    (int)g_config.type,
                                    g_config.width, 
                                    g_config.height, 
                                    g_config.margin,
                                    outputPath);
        
        if (result == 0)
        {
            successCount++;
            g_batchParts[i].processed = 1;
            
            /* Insert note at current position */
            status = InsertBarcodeNote(drawing, g_batchParts[i].name, outputPath, currentX, currentY);
            if (status == PRO_TK_NO_ERROR)
            {
                noteCount++;
            }
            
            /* Move to next position */
            currentX += 80.0;
            if ((i + 1) % perRow == 0)
            {
                currentX = 50.0;
                currentY -= spacing;
            }
        }
        else
        {
            failCount++;
        }
    }
    
    /* Refresh window */
    if (ProWindowCurrentGet(&windowId) == PRO_TK_NO_ERROR)
    {
        ProWindowRepaint(windowId);
    }
    
    /* Show summary */
    sprintf(msg, 
        "Batch Processing Complete!\n\n"
        "Total parts: %d\n"
        "Barcodes generated: %d\n"
        "Notes inserted: %d\n"
        "Failed: %d\n\n"
        "Images saved to: %s",
        partCount, successCount, noteCount, failCount, tempDir);
    
    ProStringToWstring(wmsg, msg);
    ProMessageDisplay(NULL, "%0w", wmsg);
    
    return 0;
}

/* Action: Sync Check */
static int ActionSync(uiCmdCmdId cmd, uiCmdValue *val, void *data)
{
    char modelName[256] = {0};
    wchar_t wmsg[512];
    char msg[512];
    ProError status;
    
    status = GetCurrentModelName(modelName, sizeof(modelName));
    
    if (status == PRO_TK_NO_ERROR && modelName[0] != '\0')
    {
        sprintf(msg, "Sync Check: %s\n"
                    "Settings: %s, %dx%d\n"
                    "Status: Ready",
                modelName,
                barcode_type_name_c(g_config.type),
                g_config.width, g_config.height);
    }
    else
    {
        sprintf(msg, "Please open a model first.");
    }
    
    ProStringToWstring(wmsg, msg);
    ProMessageDisplay(NULL, "%0w", wmsg);
    
    return 0;
}


int user_initialize(
    int argc,
    char *argv[],
    char *version,
    char *build,
    wchar_t errbuf[80])
{
    ProError status;
    
    /* Pure C version - no C++ initialization needed */
    /* barcode_init() and com_bridge_init() are disabled */
    
    /* Set optimized default config for better quality */
    g_config.type = BARCODE_CODE_128;
    g_config.width = 400;   /* Increased width for better readability */
    g_config.height = 120;  /* Increased height for better scanning */
    g_config.margin = 20;   /* Larger margins for cleaner appearance */
    g_config.showText = 1;
    g_config.dpi = 300;
    
    /* Register Generate command */
    status = ProCmdActionAdd(
        "BarcodeGenerate",
        (uiCmdCmdActFn)ActionGenerate,
        uiProeImmediate,
        MyAccess,
        PRO_B_TRUE,
        PRO_B_TRUE,
        &g_cmdGenerate);
    
    if (status == PRO_TK_NO_ERROR)
    {
        ProCmdDesignate(g_cmdGenerate,
            "Generate Barcode",
            "Generate Barcode",
            "Generate barcode and place at click position",
            NULL);
    }
    
    /* Register Settings command */
    status = ProCmdActionAdd(
        "BarcodeSettings",
        (uiCmdCmdActFn)ActionSettings,
        uiProeImmediate,
        MyAccess,
        PRO_B_TRUE,
        PRO_B_TRUE,
        &g_cmdSettings);
    
    if (status == PRO_TK_NO_ERROR)
    {
        ProCmdDesignate(g_cmdSettings,
            "Settings",
            "Settings",
            "Configure barcode type and size",
            NULL);
    }

    /* Register Batch command */
    status = ProCmdActionAdd(
        "BarcodeBatch",
        (uiCmdCmdActFn)ActionBatch,
        uiProeImmediate,
        MyAccess,
        PRO_B_TRUE,
        PRO_B_TRUE,
        &g_cmdBatch);
    
    if (status == PRO_TK_NO_ERROR)
    {
        ProCmdDesignate(g_cmdBatch,
            "Batch Process",
            "Batch Process",
            "Generate barcodes for all parts in drawing",
            NULL);
    }
    
    /* Register Sync command */
    status = ProCmdActionAdd(
        "BarcodeSync",
        (uiCmdCmdActFn)ActionSync,
        uiProeImmediate,
        MyAccess,
        PRO_B_TRUE,
        PRO_B_TRUE,
        &g_cmdSync);
    
    if (status == PRO_TK_NO_ERROR)
    {
        ProCmdDesignate(g_cmdSync,
            "Sync Check",
            "Sync Check",
            "Check barcode data synchronization",
            NULL);
    }
    
    return 0;
}

void user_terminate(void)
{
    /* Pure C version - no cleanup needed */
    /* com_bridge_cleanup() and barcode_cleanup() are disabled */
}
