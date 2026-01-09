/**
 * @file barcode_pure_c.c
 * @brief Pure C implementation of Code 128 barcode generator
 * 
 * This implementation avoids C++ runtime dependencies that cause
 * crashes when loaded as a Creo plugin DLL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Code 128 encoding tables */
/* Each pattern is 11 modules (bars and spaces) */
/* Format: bar, space, bar, space, bar, space (widths 1-4) */

static const char* CODE128_PATTERNS[] = {
    "212222", "222122", "222221", "121223", "121322",  /* 0-4 */
    "131222", "122213", "122312", "132212", "221213",  /* 5-9 */
    "221312", "231212", "112232", "122132", "122231",  /* 10-14 */
    "113222", "123122", "123221", "223211", "221132",  /* 15-19 */
    "221231", "213212", "223112", "312131", "311222",  /* 20-24 */
    "321122", "321221", "312212", "322112", "322211",  /* 25-29 */
    "212123", "212321", "232121", "111323", "131123",  /* 30-34 */
    "131321", "112313", "132113", "132311", "211313",  /* 35-39 */
    "231113", "231311", "112133", "112331", "132131",  /* 40-44 */
    "113123", "113321", "133121", "313121", "211331",  /* 45-49 */
    "231131", "213113", "213311", "213131", "311123",  /* 50-54 */
    "311321", "331121", "312113", "312311", "332111",  /* 55-59 */
    "314111", "221411", "431111", "111224", "111422",  /* 60-64 */
    "121124", "121421", "141122", "141221", "112214",  /* 65-69 */
    "112412", "122114", "122411", "142112", "142211",  /* 70-74 */
    "241211", "221114", "413111", "241112", "134111",  /* 75-79 */
    "111242", "121142", "121241", "114212", "124112",  /* 80-84 */
    "124211", "411212", "421112", "421211", "212141",  /* 85-89 */
    "214121", "412121", "111143", "111341", "131141",  /* 90-94 */
    "114113", "114311", "411113", "411311", "113141",  /* 95-99 */
    "114131", "311141", "411131", "211412", "211214",  /* 100-104 */
    "211232",                                          /* 105 (START B) */
    "2331112"                                          /* 106 (STOP) - 13 modules */
};

/* Code 128B value lookup (ASCII 32-127 maps to values 0-95) */
static int code128b_value(char c) {
    if (c >= 32 && c <= 127) {
        return c - 32;
    }
    return 0; /* Default to space for invalid chars */
}

/* Calculate Code 128 checksum */
static int calculate_checksum(const char* data, int startCode) {
    int sum = startCode; /* START code value */
    int i;
    int len = (int)strlen(data);
    
    for (i = 0; i < len; i++) {
        sum += code128b_value(data[i]) * (i + 1);
    }
    
    return sum % 103;
}

/* BMP file header structures */
#pragma pack(push, 1)
typedef struct {
    unsigned short bfType;      /* "BM" */
    unsigned int   bfSize;      /* File size */
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int   bfOffBits;   /* Offset to pixel data */
} BMPFILEHEADER;

typedef struct {
    unsigned int   biSize;          /* Header size (40) */
    int            biWidth;
    int            biHeight;
    unsigned short biPlanes;        /* 1 */
    unsigned short biBitCount;      /* 24 for RGB */
    unsigned int   biCompression;   /* 0 = uncompressed */
    unsigned int   biSizeImage;
    int            biXPelsPerMeter;
    int            biYPelsPerMeter;
    unsigned int   biClrUsed;
    unsigned int   biClrImportant;
} BMPINFOHEADER;
#pragma pack(pop)

/* Generate barcode and save as BMP with improved quality */
int barcode_generate_pure_c(const char* data, const char* outputPath, 
                            int width, int height, int margin) {
    FILE* fp;
    int dataLen;
    int barcodeWidth;
    int moduleWidth;
    int totalModules;
    int checksum;
    int i, j, x, y;
    int rowSize;
    unsigned char* pixels;
    BMPFILEHEADER fileHeader;
    BMPINFOHEADER infoHeader;
    int currentX;
    const char* pattern;
    int patternIdx;
    int barWidth;
    int isBar;
    
    if (!data || !outputPath || data[0] == '\0') {
        return -1;
    }
    
    dataLen = (int)strlen(data);
    if (dataLen > 80) {
        return -2; /* Data too long */
    }
    
    /* Calculate total modules needed */
    /* START(11) + data(11 each) + CHECKSUM(11) + STOP(13) */
    totalModules = 11 + (dataLen * 11) + 11 + 13;
    
    /* Calculate module width to fit desired barcode width (minus margins) */
    barcodeWidth = width - (2 * margin);
    moduleWidth = barcodeWidth / totalModules;
    if (moduleWidth < 2) moduleWidth = 2; /* Minimum 2 pixels per module for better quality */
    
    /* Recalculate actual barcode width */
    barcodeWidth = moduleWidth * totalModules;
    width = barcodeWidth + (2 * margin);
    
    /* Ensure minimum height for readability */
    if (height < 60) height = 60;
    
    /* BMP row size must be multiple of 4 bytes */
    rowSize = ((width * 3 + 3) / 4) * 4;
    
    /* Allocate pixel buffer */
    pixels = (unsigned char*)malloc(rowSize * height);
    if (!pixels) {
        return -3; /* Memory allocation failed */
    }
    
    /* Fill with white background */
    memset(pixels, 255, rowSize * height);
    
    /* Calculate checksum */
    checksum = calculate_checksum(data, 104); /* 104 = START B */
    
    /* Draw barcode with improved quality */
    currentX = margin;
    
    /* Calculate barcode drawing area (leave margins at top and bottom) */
    int barcodeTop = margin;
    int barcodeBottom = height - margin;
    
    /* Helper macro to draw a bar or space with anti-aliasing */
    #define DRAW_PATTERN(pat) do { \
        for (patternIdx = 0; pat[patternIdx] != '\0'; patternIdx++) { \
            barWidth = (pat[patternIdx] - '0') * moduleWidth; \
            isBar = (patternIdx % 2 == 0); /* Even indices are bars */ \
            if (isBar) { \
                /* Draw solid black bars */ \
                for (y = barcodeTop; y < barcodeBottom; y++) { \
                    for (x = currentX; x < currentX + barWidth && x < width - margin; x++) { \
                        int offset = y * rowSize + x * 3; \
                        if (offset + 2 < rowSize * height) { \
                            pixels[offset] = 0;     /* Blue */ \
                            pixels[offset + 1] = 0; /* Green */ \
                            pixels[offset + 2] = 0; /* Red */ \
                        } \
                    } \
                } \
            } \
            currentX += barWidth; \
        } \
    } while(0)
    
    /* Draw START B (code 104) */
    pattern = CODE128_PATTERNS[104];
    DRAW_PATTERN(pattern);
    
    /* Draw data characters */
    for (i = 0; i < dataLen; i++) {
        int value = code128b_value(data[i]);
        if (value >= 0 && value < 106) { /* Validate array bounds */
            pattern = CODE128_PATTERNS[value];
            DRAW_PATTERN(pattern);
        }
    }
    
    /* Draw checksum */
    if (checksum >= 0 && checksum < 106) { /* Validate array bounds */
        pattern = CODE128_PATTERNS[checksum];
        DRAW_PATTERN(pattern);
    }
    
    /* Draw STOP (code 106) */
    pattern = CODE128_PATTERNS[106];
    DRAW_PATTERN(pattern);
    
    #undef DRAW_PATTERN
    
    /* Write BMP file */
    fp = fopen(outputPath, "wb");
    if (!fp) {
        free(pixels);
        return -4; /* Cannot create file */
    }
    
    /* File header */
    fileHeader.bfType = 0x4D42; /* "BM" */
    fileHeader.bfSize = sizeof(BMPFILEHEADER) + sizeof(BMPINFOHEADER) + rowSize * height;
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = sizeof(BMPFILEHEADER) + sizeof(BMPINFOHEADER);
    
    /* Info header */
    infoHeader.biSize = sizeof(BMPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height; /* Positive = bottom-up */
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 24;
    infoHeader.biCompression = 0;
    infoHeader.biSizeImage = rowSize * height;
    infoHeader.biXPelsPerMeter = 2835; /* 72 DPI */
    infoHeader.biYPelsPerMeter = 2835;
    infoHeader.biClrUsed = 0;
    infoHeader.biClrImportant = 0;
    
    fwrite(&fileHeader, sizeof(BMPFILEHEADER), 1, fp);
    fwrite(&infoHeader, sizeof(BMPINFOHEADER), 1, fp);
    
    /* Write pixels (BMP is bottom-up, so flip vertically) */
    for (y = height - 1; y >= 0; y--) {
        fwrite(pixels + y * rowSize, rowSize, 1, fp);
    }
    
    fclose(fp);
    free(pixels);
    
    return 0; /* Success */
}

/* Error message storage */
static char g_purec_error[256] = {0};

const char* barcode_get_error_pure_c(void) {
    return g_purec_error;
}

/* Wrapper function matching the expected API */
int barcode_generate_c(const char* data, int type, int width, int height, 
                       int margin, const char* outputPath) {
    int result;
    
    (void)type; /* Currently only Code 128 is supported */
    
    result = barcode_generate_pure_c(data, outputPath, width, height, margin);
    
    switch (result) {
        case 0:
            g_purec_error[0] = '\0';
            break;
        case -1:
            strcpy(g_purec_error, "Invalid parameters");
            break;
        case -2:
            strcpy(g_purec_error, "Data too long (max 80 characters)");
            break;
        case -3:
            strcpy(g_purec_error, "Memory allocation failed");
            break;
        case -4:
            strcpy(g_purec_error, "Cannot create output file");
            break;
        default:
            sprintf(g_purec_error, "Unknown error: %d", result);
            break;
    }
    
    return result;
}
