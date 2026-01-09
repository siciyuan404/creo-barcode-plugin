/**
 * @file main_minimal.c
 * @brief Minimal Pro/TOOLKIT plugin - pure C, no dependencies
 */

#include <ProToolkit.h>
#include <ProMenu.h>
#include <ProMenuBar.h>
#include <ProMessage.h>
#include <ProUICmd.h>

/* Access function for menu availability */
static uiCmdAccessState BarcodeAccessFn(uiCmdAccessMode access_mode)
{
    return ACCESS_AVAILABLE;
}

/* Action function when menu is clicked */
static int BarcodeActionFn(uiCmdCmdId command, uiCmdValue *p_value, void *p_push_cmd_data)
{
    ProMessageClear();
    return 0;
}

/**
 * Pro/TOOLKIT entry point
 */
int user_initialize(
    int argc,
    char *argv[],
    char *version,
    char *build,
    wchar_t errbuf[80])
{
    ProError status;
    uiCmdCmdId cmd_id;
    
    /* Register command action */
    status = ProCmdActionAdd(
        "BarcodeGen",
        (uiCmdCmdActFn)BarcodeActionFn,
        uiProe2ndImmediate,
        BarcodeAccessFn,
        PRO_B_TRUE,
        PRO_B_TRUE,
        &cmd_id);
    
    if (status != PRO_TK_NO_ERROR) {
        return 0;  /* Still return success to not block Creo */
    }
    
    /* Add menu button */
    status = ProMenubarmenuPushbuttonAdd(
        "File",
        "BarcodeGen", 
        "BarcodeGen",
        "Generate Barcode",
        NULL,
        PRO_B_TRUE,
        cmd_id,
        NULL);
    
    return 0;
}

/**
 * Pro/TOOLKIT exit point
 */
void user_terminate()
{
    /* Nothing to clean up */
}
