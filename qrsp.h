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
void app_set_preview_size(void);
void draw_black_rectangle(void);
VMINT parseTextToArr(void);
void fromArrToDisplay(void);

#endif

