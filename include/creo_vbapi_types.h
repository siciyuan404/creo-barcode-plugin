/**
 * @file creo_vbapi_types.h
 * @brief Creo VB/COM API Type Definitions
 * 
 * This header provides type definitions and forward declarations for
 * Creo's VB/COM API interfaces. It can work in two modes:
 * 
 * 1. With actual Creo type library (when CREO_VBAPI_DIR is defined)
 *    - Uses #import to load pfcls.dll type library
 * 
 * 2. Without Creo type library (development/testing mode)
 *    - Uses forward declarations and mock interfaces
 */

#ifndef CREO_VBAPI_TYPES_H
#define CREO_VBAPI_TYPES_H

#ifdef _WIN32

#include <windows.h>
#include <comdef.h>
#include <atlbase.h>

// Check if we have access to Creo VB API type library
// The type library is typically at: <Creo_Install>/Common Files/vbapi/bin/pfcls.dll
// Or the type library file: <Creo_Install>/Common Files/vbapi/bin/pfcls.tlb

#if defined(CREO_VBAPI_PATH)
    // Import Creo VB API type library
    // This generates smart pointer types and interface definitions
    #pragma message("Importing Creo VB API type library from: " CREO_VBAPI_PATH)
    
    #import CREO_VBAPI_PATH \
        rename_namespace("CreoVBAPI") \
        named_guids \
        raw_interfaces_only \
        no_implementation
    
    #define HAS_CREO_VBAPI 1
    
    namespace creo_barcode {
        // Use imported types from Creo VB API
        using namespace CreoVBAPI;
        
        // Smart pointer type aliases for convenience
        typedef CComPtr<IpfcAsyncConnection> IpfcAsyncConnectionPtr;
        typedef CComPtr<IpfcSession> IpfcSessionPtr;
        typedef CComPtr<IpfcModel> IpfcModelPtr;
        typedef CComPtr<IpfcDrawing> IpfcDrawingPtr;
        typedef CComPtr<IpfcWindow> IpfcWindowPtr;
        typedef CComPtr<IpfcOutline2D> IpfcOutline2DPtr;
        typedef CComPtr<IpfcDraftingImage> IpfcDraftingImagePtr;
        typedef CComPtr<IpfcPoint2D> IpfcPoint2DPtr;
    }

#else
    // No Creo type library available - use forward declarations
    // This allows compilation and testing without Creo installed
    
    #define HAS_CREO_VBAPI 0
    
    namespace creo_barcode {

        /**
         * @brief Model type enumeration (matches Creo's pfcModelType)
         */
        enum pfcModelType {
            pfcMDL_PART = 0,        ///< Part model
            pfcMDL_ASSEMBLY = 1,    ///< Assembly model
            pfcMDL_DRAWING = 2,     ///< Drawing model
            pfcMDL_2D_SECTION = 3,  ///< 2D section
            pfcMDL_LAYOUT = 4,      ///< Layout
            pfcMDL_DWG_FORMAT = 5,  ///< Drawing format
            pfcMDL_MFG = 6,         ///< Manufacturing model
            pfcMDL_REPORT = 7,      ///< Report
            pfcMDL_MARKUP = 8,      ///< Markup
            pfcMDL_DIAGRAM = 9      ///< Diagram
        };

        /**
         * @brief Forward declarations for Creo VB API interfaces
         * These are placeholder interfaces for development without Creo
         */
        
        // IpfcPoint2D - 2D point interface
        struct IpfcPoint2D : public IUnknown {
            virtual HRESULT STDMETHODCALLTYPE get_Item(int index, double* pVal) = 0;
            virtual HRESULT STDMETHODCALLTYPE put_Item(int index, double val) = 0;
        };
        
        // IpfcOutline2D - 2D outline (bounding box) interface
        struct IpfcOutline2D : public IUnknown {
            virtual HRESULT STDMETHODCALLTYPE get_Item(int index, IpfcPoint2D** ppPoint) = 0;
        };
        
        // IpfcDraftingImage - Drafting image interface
        struct IpfcDraftingImage : public IUnknown {
            // Image properties
        };
        
        // IpfcWindow - Window interface for view operations
        struct IpfcWindow : public IUnknown {
            virtual HRESULT STDMETHODCALLTYPE Repaint() = 0;
            virtual HRESULT STDMETHODCALLTYPE Refresh() = 0;
        };
        
        // IpfcModel - Base model interface
        struct IpfcModel : public IUnknown {
            virtual HRESULT STDMETHODCALLTYPE get_Type(pfcModelType* pType) = 0;
            virtual HRESULT STDMETHODCALLTYPE get_FileName(BSTR* pFileName) = 0;
        };
        
        // IpfcDrawing - Drawing model interface (extends IpfcModel)
        struct IpfcDrawing : public IpfcModel {
            virtual HRESULT STDMETHODCALLTYPE CreateDraftingImage(
                BSTR imagePath,
                IpfcOutline2D* outline,
                IpfcDraftingImage** ppImage) = 0;
            virtual HRESULT STDMETHODCALLTYPE get_CurrentSheetNumber(int* pSheet) = 0;
        };
        
        // IpfcSession - Session interface
        struct IpfcSession : public IUnknown {
            virtual HRESULT STDMETHODCALLTYPE get_CurrentModel(IpfcModel** ppModel) = 0;
            virtual HRESULT STDMETHODCALLTYPE get_CurrentWindow(IpfcWindow** ppWindow) = 0;
            virtual HRESULT STDMETHODCALLTYPE UIShowMessageDialog(
                BSTR message,
                void* buttons) = 0;
        };
        
        // IpfcAsyncConnection - Async connection interface
        struct IpfcAsyncConnection : public IUnknown {
            virtual HRESULT STDMETHODCALLTYPE get_Session(IpfcSession** ppSession) = 0;
            virtual HRESULT STDMETHODCALLTYPE Disconnect(int timeout) = 0;
            virtual HRESULT STDMETHODCALLTYPE IsRunning(VARIANT_BOOL* pRunning) = 0;
        };
        
        // Smart pointer type aliases
        typedef CComPtr<IpfcAsyncConnection> IpfcAsyncConnectionPtr;
        typedef CComPtr<IpfcSession> IpfcSessionPtr;
        typedef CComPtr<IpfcModel> IpfcModelPtr;
        typedef CComPtr<IpfcDrawing> IpfcDrawingPtr;
        typedef CComPtr<IpfcWindow> IpfcWindowPtr;
        typedef CComPtr<IpfcOutline2D> IpfcOutline2DPtr;
        typedef CComPtr<IpfcDraftingImage> IpfcDraftingImagePtr;
        typedef CComPtr<IpfcPoint2D> IpfcPoint2DPtr;
        
        /**
         * @brief CLSID for Creo AsyncConnection
         * This is the COM class ID used to create the connection object
         */
        // {5B4D3E3A-6C7D-4E8F-9A0B-1C2D3E4F5A6B} - Placeholder CLSID
        // Actual CLSID should be obtained from Creo type library
        extern const GUID CLSID_pfcAsyncConnection;
        
        /**
         * @brief IID for IpfcAsyncConnection interface
         */
        // {6C8E4F4B-7D8E-5F9A-AB1C-2D3E4F5A6B7C} - Placeholder IID
        extern const GUID IID_IpfcAsyncConnection;
        
        /**
         * @brief IID for IpfcDrawing interface
         */
        // {7D9F5G5C-8E9F-6GAB-BC2D-3E4F5A6B7C8D} - Placeholder IID
        extern const GUID IID_IpfcDrawing;
            
    } // namespace creo_barcode

#endif // CREO_VBAPI_PATH

/**
 * @brief Helper class for creating Creo VB API objects
 */
namespace creo_barcode {

/**
 * @brief Factory class for creating Creo COM objects
 */
class CreoVBAPIFactory {
public:
    /**
     * @brief Create an AsyncConnection to Creo
     * @param ppConnection Output connection pointer
     * @return HRESULT indicating success or failure
     */
    static HRESULT CreateAsyncConnection(IpfcAsyncConnection** ppConnection);
    
    /**
     * @brief Create a 2D point object
     * @param x X coordinate
     * @param y Y coordinate
     * @param ppPoint Output point pointer
     * @return HRESULT indicating success or failure
     */
    static HRESULT CreatePoint2D(double x, double y, IpfcPoint2D** ppPoint);
    
    /**
     * @brief Create a 2D outline (bounding box) object
     * @param x1 Lower-left X
     * @param y1 Lower-left Y
     * @param x2 Upper-right X
     * @param y2 Upper-right Y
     * @param ppOutline Output outline pointer
     * @return HRESULT indicating success or failure
     */
    static HRESULT CreateOutline2D(double x1, double y1, 
                                    double x2, double y2,
                                    IpfcOutline2D** ppOutline);
};

} // namespace creo_barcode

#endif // _WIN32

#endif // CREO_VBAPI_TYPES_H
