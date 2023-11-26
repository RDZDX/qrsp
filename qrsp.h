#ifndef _VRE_APP_WIZARDTEMPLATE_
#define	_VRE_APP_WIZARDTEMPLATE_

#define MRE_STR_SIZE_MAX            (100)
#define MRE_SCREEN_START_X 		(0)
#define MRE_SCREEN_START_Y 		(0)

#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmchset.h"
#include "vmstdlib.h"
#include "ResID.h"
#include "vm4res.h"
#include "vmcamera.h"
#include "vmtimer.h"
#include "string.h"
#include "vmmm.h"
#include "vmpromng.h"
#include "vmvideo.h"
#include <time.h>

#include "vmsms.h"
#include "stdint.h"

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
void handle_penevt(VMINT event, VMINT x, VMINT y);
void start_cam_preview(void);
void cam_message_callback(vm_cam_notify_data_t* notify_data, void* user_data);
unsigned char claim(int x);
void uyvyToGrey(unsigned char *dst, unsigned char *src,  unsigned int numberPixels);
char* decode_qr(void *raw, size_t raw_sz, int width, int height);
void stop_preview(void);
void checkFileExist(void);
VMINT parseTextToArr(void);
void fromArrToDisplay(void);
void fromArrToDisplay2(void);
void app_set_current_preview_size(VM_CAMERA_HANDLE camera_handle);
VMINT mre_create_msg(VMWSTR data);
static void mre_msg_callback(vm_sms_callback_t *callback_data);
static void mre_delete_msg();
void create_app_txt_filename(VMWSTR text);
void create_auto_filename(VMWSTR text);
void create_auto_full_path_name(VMWSTR result, VMWSTR fname);

#endif

