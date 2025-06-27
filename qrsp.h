#ifndef _VRE_APP_WIZARDTEMPLATE_
#define	_VRE_APP_WIZARDTEMPLATE_

#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmchset.h"
#include "vmstdlib.h"
#include "vmcamera.h"
#include "string.h"
#include "vmmm.h"
#include <time.h>
//#include "vmsms.h"
#include "stdint.h"
#include "zbar.h"

/* ---------------------------------------------------------------------------
 * global variables
 * ------------------------------------------------------------------------ */

VMINT		layer_hdl[1];

const unsigned short tr_color = VM_COLOR_888_TO_565(0, 255, 255);

/* ---------------------------------------------------------------------------
 * local variables
 * ------------------------------------------------------------------------ */

void handle_sysevt(VMINT message, VMINT param);
void handle_keyevt(VMINT event, VMINT keycode);
void start_cam_preview(void);
void cam_message_callback(vm_cam_notify_data_t *notify_data, void *user_data);
void checkFileExist(void);
void writeDataToTextFile(void);
void app_set_current_preview_size(VM_CAMERA_HANDLE camera_handle);
void create_app_txt_filename(VMWSTR text, VMSTR extt);
void create_auto_filename(VMWSTR text, VMSTR extt);
void create_auto_full_path_name(VMWSTR result, VMWSTR fname);
void fromArrToDisplay(VMINT first_row, VMINT count_rows);
//static void mre_msg_callback(vm_sms_callback_t *callback_data);
//VMINT mre_create_msg(char* data);
void invertGrayBuffer(unsigned char *buf, unsigned int size);
void uyvyToGrey(unsigned char *dst, unsigned char *src, unsigned int numberPixels);
char *decode_qr(unsigned char *data, size_t data_sz, int width, int height);
VMINT parseTextToArr(void);
unsigned char claim(int x);

#endif

