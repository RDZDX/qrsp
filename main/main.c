//#define IBase GS_IBase

//#include "gs_id.h"
#include "qrsp.h"
//#include "srv_ucm_interface.h"
#include "vmsys.h"
#include "vmtimer.h"
#include "vmeditor.h"

#define VM_CHSET_DEST_BYTES 4096
#define UCS2_CHUNK_WORDS    (VM_CHSET_DEST_BYTES / 2)

#define INITIAL_UCS2_BYTES  (4096)
#define READ_CHUNK_BYTES    2048
#define MAX_UCS2_BYTES      (512 * 1024)  //(1500 * 1024) 

vm_editor_font_attribute my_font = {0, 0, 0, 20}; //16 18 14
static vm_input_mode_enum my_input_modes_lower_first[] = {VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_ABC, VM_INPUT_MODE_123, VM_INPUT_MODE_123_SYMBOLS, VM_INPUT_MODE_NONE};

//extern VMINT vm_create_service(VMINT sid, VMINT iid, void** interface_ptr);

VM_CAMERA_HANDLE handle_ptr;
VMUINT8* layer_buf0 = NULL;
char* tmp_qr_res = NULL;
int font_height = 20; //--------------------------------------------
VMBOOL capture_stage = VM_TRUE;
VMBOOL is_malloc = VM_FALSE;
VMINT sw;
VMINT sh;
VMINT scan_timer_id = -1;
VMBOOL scan_enabled = VM_FALSE;

typedef enum { STATE_SCANNING, STATE_IN_CALL, STATE_ERROR_MESSAGE } APP_STATE;

APP_STATE current_state = STATE_SCANNING;
VMINT32 error_timer_id = -1;
char error_display_text[64] = "";

static VMINT32 input_editor = 0;
static VMINT input_h = 0; //64
static VMWCHAR left_label[32];
static VMWCHAR left_label1[32];
static VMWCHAR right_label[32];
static VMWCHAR center_label[32];
static VMWCHAR center_label1[32];
static VMWCHAR center_label2[32];
VMWCHAR outfile[100] = {0};
VMBOOL scan_finish = VM_FALSE;
VMUINT16 *last_preview = NULL;
VMBOOL first_row = VM_FALSE;
VMBOOL full_screen = VM_FALSE;
VMBOOL stop_by_user = VM_FALSE;
VMBOOL scan_paused_by_user = VM_FALSE;
//static VMWSTR sms_data = NULL;

VMINT response = -1;
VMBOOL trigeris1 = VM_FALSE;
VMWCHAR bakfile[100] = {0};
static VMUWSTR ucs2_buf = NULL;
static VMINT ucs2_buf_bytes = 0;
static VMINT ucs2_len_words = 0;

void handle_sysevt(VMINT message, VMINT param);
void handle_keyevt(VMINT event, VMINT keycode);
void start_cam_preview(void);
void app_set_current_preview_size(VM_CAMERA_HANDLE camera_handle);
void cam_message_callback(vm_cam_notify_data_t* notify_data, void* user_data);
void invertGrayBuffer(unsigned char* buf, unsigned int size);
void uyvyToGrey(unsigned char* dst, unsigned char* src, unsigned int numberPixels);
char* decode_qr(unsigned char* data, size_t data_sz, int width, int height);
unsigned char claim(int x);
//static VMBOOL is_phone_number(const char* s);
void scan_enable_timer_callback(VMINT tid);
//void normalize_phone(char* dst, const char* src);
VMUINT32 ime_cb(VMINT32 h, vm_editor_message_struct_p m);
void smoll_size(void);
void full_size(void);
void create_auto_filename(VMWSTR text);
static VMINT save_history_to_utf8_chunked(VMWSTR outfile_path);
void save_history(void);
void append_input(VMSTR msg);
void stop_scan(void);

void clear_history(void){}

void func_center_key(void)
{

    if (scan_paused_by_user)
    {
        scan_paused_by_user = VM_FALSE;
        reset_to_preview();
        return;
    }


     if (scan_finish == VM_TRUE)
     {
         if (full_screen == VM_TRUE)
         {

            save_history();

         } else {

            reset_to_preview();

         }

     } else {

    stop_scan();

    }
}

void func_left_key(void)
{

     if (scan_finish == VM_FALSE)
     {
         stop_scan();
     }

     if (full_screen == VM_TRUE)
     {
         smoll_size();
     } else {
         full_size();
     }

     if (full_screen == VM_FALSE)
     {
         full_screen = VM_TRUE;
     } else {
         full_screen = VM_FALSE;
     }

}

void vm_main(void) {

    layer_hdl[0] = -1;
    handle_ptr = -1;
    vm_reg_sysevt_callback(handle_sysevt);
    sw = vm_graphic_get_screen_width();
    sh = vm_graphic_get_screen_height();
    last_preview = vm_malloc(sw * sw * 2);

    if (!last_preview)
    {
        vm_exit_app();
    }

    input_h = sh - sw - vm_editor_get_softkey_height();

ucs2_buf_init_empty();

if (!ucs2_buf)
{
    vm_exit_app();
}

    vm_ascii_to_ucs2(left_label, sizeof(left_label), (VMSTR)"History");
    vm_ascii_to_ucs2(left_label1, sizeof(left_label1), (VMSTR)"Back");
    vm_ascii_to_ucs2(center_label, sizeof(center_label), (VMSTR)"Next");
    vm_ascii_to_ucs2(center_label1, sizeof(center_label1), (VMSTR)"Save");
    vm_ascii_to_ucs2(center_label2, sizeof(center_label2), (VMSTR)"Stop");
//    vm_ascii_to_ucs2(right_label, sizeof(right_label), (VMSTR)"Clear");
    vm_ascii_to_ucs2(right_label, sizeof(right_label), (VMSTR)"Open");

}

void reset_to_preview(void) {

    scan_finish = VM_FALSE;
    scan_paused_by_user = VM_FALSE;
    stop_by_user = VM_FALSE;

    scan_enabled = VM_FALSE;

    if (scan_timer_id != -1) {
        vm_delete_timer(scan_timer_id);
        scan_timer_id = -1;
    }

    scan_timer_id = vm_create_timer(2000, scan_enable_timer_callback);  // 1000

    current_state = STATE_SCANNING;
    capture_stage = VM_TRUE;
    is_malloc = VM_FALSE;
    if (tmp_qr_res) {
        vm_free(tmp_qr_res);
        tmp_qr_res = NULL;
    }

    if (handle_ptr == -1) {
        start_cam_preview();
    } else {
        if (vm_camera_preview_start(handle_ptr) != VM_CAM_SUCCESS) {
            // vm_camera_preview_stop(handle_ptr);
            vm_release_camera_instance(handle_ptr);
            handle_ptr = -1;
        }
    }

    vm_editor_set_softkey(input_editor, (VMUWSTR)center_label2, VM_CENTER_SOFTKEY, func_center_key); //Stop
    vm_editor_redraw_ime_screen();
    vm_editor_show(input_editor);
}

void stop_scan(void)
{
    scan_paused_by_user = VM_TRUE;

    stop_by_user = VM_TRUE;

    scan_enabled = VM_FALSE;

    if (scan_timer_id != -1)
    {
        vm_delete_timer(scan_timer_id);
        scan_timer_id = -1;
    }

    if (handle_ptr != -1)
    {
        vm_camera_preview_stop(handle_ptr);
    }

    vm_editor_set_softkey(input_editor, (VMUWSTR)center_label, VM_CENTER_SOFTKEY, func_center_key); //Next
    vm_editor_redraw_ime_screen();
    vm_editor_show(input_editor);

}

//static S32 dial_callback(gs_srv_event_struct* param) { return 0; }

//void make_background_call(const char* number_str) {

//    IUcm* ucm = NULL;

//    VMINT ret = vm_create_service(SID_UCMSERVICE, IID_IUCM, (void**)&ucm);

//    if (ret < 0 || !ucm) {
//        return;
//    }

//    gs_srv_ucm_dial_act_req_struct req;

//    memset(&req, 0, sizeof(req));

//    req.call_type = GS_SRV_UCM_VOICE_CALL_TYPE;
//    req.module_id = GS_SRV_UCM_MODULE_ORIGIN_COMMON;
//    req.is_ip_dial = 0;

//    vm_ascii_to_ucs2((VMWSTR)req.num_uri, sizeof(req.num_uri), (VMCHAR*)number_str);

//    if (IUcm_query_act_permit(ucm, GS_SRV_UCM_DIAL_ACT, &req) == GS_SRV_UCM_RESULT_OK) {

//        current_state = STATE_IN_CALL;
//        IUcm_act_request(ucm, GS_SRV_UCM_DIAL_ACT, &req, NULL, dial_callback);

//    } else {
//    }

//    IUcm_release(ucm);

//    if (tmp_qr_res) {
//        vm_free(tmp_qr_res);
//        tmp_qr_res = NULL;
//    }
//}

void handle_sysevt(VMINT message, VMINT param) {

    switch (message) {

        case VM_MSG_CREATE:
        case VM_MSG_ACTIVE:

            if (layer_hdl[0] == -1) {
                layer_hdl[0] = vm_graphic_create_layer(0, 0, sw, sh, -1);
                vm_graphic_set_clip(0, 0, sw, sh);
            }

            layer_buf0 = vm_graphic_get_layer_buffer(layer_hdl[0]);
//            vm_graphic_fill_rect(layer_buf0, 0, 0, sw, sh, VM_COLOR_BLACK, VM_COLOR_BLACK);

           if (scan_paused_by_user && last_preview && full_screen == VM_FALSE)
           {
               memcpy(layer_buf0, last_preview, sw * sw * 2);
               vm_graphic_flush_layer(layer_hdl, 1);
           }

            if (!scan_paused_by_user)
            {
                vm_graphic_fill_rect(layer_buf0, 0, 0, sw, sh, VM_COLOR_BLACK, VM_COLOR_BLACK);
            }

            if(input_editor == 0)
            {
                input_editor = vm_editor_create(VM_EDITOR_MULTILINE, 0, sw, sw, input_h, ucs2_buf, ucs2_buf_bytes, VM_FALSE, layer_hdl[0]);
                vm_editor_set_bg_border_style(input_editor, VM_EDITOR_NO_BORDER, 0xFFFF, 0xFFFF); //White:0xFFFF; Black:0x0000; Green: 0x07E0
                vm_editor_set_multiline_text_font(input_editor, my_font);
                vm_editor_set_margin(input_editor, 0, 0, 0, 0);
                vm_editor_set_IME(input_editor, VM_INPUT_TYPE_SENTENCE, my_input_modes_lower_first, VM_INPUT_MODE_MULTITAP_FIRST_UPPERCASE_ABC, ime_cb);
                vm_editor_set_softkey(input_editor, (VMUWSTR)left_label, VM_LEFT_SOFTKEY, func_left_key); //History
//                vm_editor_set_softkey(input_editor, (VMUWSTR)center_label2, VM_CENTER_SOFTKEY, func_center_key); //Stop

                if (!scan_paused_by_user)
                {
                    vm_editor_set_softkey(input_editor, (VMUWSTR)center_label2, VM_CENTER_SOFTKEY, func_center_key);
                } else {
                    vm_editor_set_softkey(input_editor, (VMUWSTR)center_label, VM_CENTER_SOFTKEY, func_center_key);
                }

                vm_editor_set_softkey(input_editor, (VMUWSTR)right_label, VM_RIGHT_SOFTKEY, change_label_name);
                vm_editor_activate(input_editor, VM_FALSE);
            }

            vm_switch_power_saving_mode(turn_off_mode);

//            if (handle_ptr == -1) {
//                start_cam_preview();
//            }


            if (handle_ptr == -1 && !scan_paused_by_user)
            {
                start_cam_preview();
            }


            break;

        case VM_MSG_PAINT:
            vm_switch_power_saving_mode(turn_off_mode);

            if(input_editor) {
                vm_editor_redraw_ime_screen();
                vm_editor_show(input_editor);
            }

            break;

        case VM_MSG_INACTIVE:

            if (current_state != STATE_IN_CALL) {
                if (handle_ptr != -1) {
                    vm_camera_preview_stop(handle_ptr);
                    vm_release_camera_instance(handle_ptr);
                    handle_ptr = -1;
                }

            if(input_editor)
            {
                vm_editor_close(input_editor);
                input_editor = 0;
            }

                // vm_release_camera_instance(handle_ptr);
                if (layer_hdl[0] != -1) {
                    vm_graphic_delete_layer(layer_hdl[0]);
                    layer_hdl[0] = -1;
                    layer_buf0 = NULL;
                }
                vm_graphic_reset_clip();
                vm_switch_power_saving_mode(turn_on_mode);
            }
            break;

        case VM_MSG_QUIT:
            if (error_timer_id != -1) vm_delete_timer(error_timer_id);

            if(input_editor)
            {
                vm_editor_close(input_editor);
                input_editor = 0;
            }

if (ucs2_buf)
{
    vm_free(ucs2_buf);
    ucs2_buf = NULL;
    ucs2_buf_bytes = 0;
    ucs2_len_words = 0;
}

            if (last_preview)
            {
                vm_free(last_preview);
                last_preview = NULL;
            }

            if (handle_ptr != -1) {
                vm_camera_preview_stop(handle_ptr);
                vm_release_camera_instance(handle_ptr);
                handle_ptr = -1;
            }

            if (scan_timer_id != -1) {
                vm_delete_timer(scan_timer_id);
                scan_timer_id = -1;
            }

            if (layer_hdl[0] != -1) {
                vm_graphic_delete_layer(layer_hdl[0]);
                layer_hdl[0] = -1;
                layer_buf0 = NULL;
            }

            break;

        default:
            break;
    }
}

void start_cam_preview(void) {

    if (vm_create_camera_instance(VM_CAMERA_MAIN_ID, &handle_ptr) != VM_CAM_SUCCESS) {
        return;
    }

    if (vm_camera_register_notify(handle_ptr, (VM_CAMERA_STATUS_NOTIFY)cam_message_callback, NULL) != VM_CAM_SUCCESS) {
        vm_release_camera_instance(handle_ptr);
        handle_ptr = -1;
        return;
    }

    app_set_current_preview_size(handle_ptr);

    if (vm_camera_preview_start(handle_ptr) != VM_CAM_SUCCESS) {
        vm_release_camera_instance(handle_ptr);
        handle_ptr = -1;
        return;
    }

    scan_enabled = VM_FALSE;

    if (scan_timer_id != -1) {
        vm_delete_timer(scan_timer_id);
    }

    scan_timer_id = vm_create_timer(2000, scan_enable_timer_callback);  // 1000
}

void cam_message_callback(vm_cam_notify_data_t* notify_data, void* user_data) {

    vm_cam_frame_data_t my_frame_data;
    int i;

    handle_ptr = notify_data->handle;

    switch (notify_data->cam_message) {
        case VM_CAM_PREVIEW_FRAME_RECEIVED:

            if (vm_camera_get_frame(handle_ptr, &my_frame_data) != VM_CAM_SUCCESS)
                return;

            VMUINT row_pixel = my_frame_data.row_pixel;
            VMUINT col_pixel = my_frame_data.col_pixel;
            VMUINT grey_app_frame_data_size = row_pixel * col_pixel;

            unsigned char* uyvy_data = my_frame_data.pixtel_data;

            tmp_qr_res = NULL;

            if (scan_enabled) {
                unsigned char* grey_app_frame_data = vm_malloc(grey_app_frame_data_size);

                if (!grey_app_frame_data) return;

                uyvyToGrey(grey_app_frame_data, uyvy_data, grey_app_frame_data_size);

                tmp_qr_res = decode_qr(grey_app_frame_data, grey_app_frame_data_size, col_pixel, row_pixel);

                if (!tmp_qr_res) {
                    invertGrayBuffer(grey_app_frame_data, grey_app_frame_data_size);

                    tmp_qr_res = decode_qr(grey_app_frame_data, grey_app_frame_data_size, col_pixel, row_pixel);
                }

                vm_free(grey_app_frame_data);
            }

            if (tmp_qr_res) {
                is_malloc = VM_TRUE;
                vm_camera_preview_stop(handle_ptr);
                return;
            }
            VMWCHAR* layer_buf_s = (VMWCHAR*)layer_buf0;
            if (layer_buf_s) {
                int x, y;

                for (y = 0; y < sw; y++) {
                    for (x = 0; x < sw; x += 2) {
                        int u0 = *uyvy_data++ - 128;
                        int y0 = *uyvy_data++ - 16;
                        int v0 = *uyvy_data++ - 128;
                        int y1 = *uyvy_data++ - 16;

                        int tmp = 298 * y0 + 128;
                        int vv = 409 * v0;
                        int uv = -100 * u0 - 208 * v0;
                        int uu = 516 * u0;

                        layer_buf_s[y * sw + x] = VM_COLOR_888_TO_565(
                            claim((tmp + vv) >> 8), claim((tmp + uv) >> 8),
                            claim((tmp + uu) >> 8));

                        tmp = 298 * y1 + 128;

                        layer_buf_s[y * sw + x + 1] = VM_COLOR_888_TO_565(
                            claim((tmp + vv) >> 8), claim((tmp + uv) >> 8),
                            claim((tmp + uu) >> 8));
                    }
                }

            if (last_preview)
            {
                memcpy(last_preview, layer_buf0, sw * sw * 2);
            }

                vm_graphic_flush_layer(layer_hdl, 1);
            }

            break;
        case VM_CAM_PREVIEW_STOP_DONE:

    if (stop_by_user)
    {
        stop_by_user = VM_FALSE;

 if (handle_ptr != -1)
    {
        vm_release_camera_instance(handle_ptr);
        handle_ptr = -1;
    }

//        if (last_preview)
        if (last_preview && full_screen == VM_FALSE)
        {
            memcpy(layer_buf0, last_preview, sw * sw * 2);
            vm_graphic_flush_layer(layer_hdl, 1);
        }

        break;
    }

            if (tmp_qr_res) {

                if (last_preview)
                {
                    memcpy(last_preview, layer_buf0, sw * sw * 2);
                }

                is_malloc = VM_TRUE;

                vm_graphic_flush_layer(layer_hdl, 1);
                scan_finish = VM_TRUE;
                append_input(tmp_qr_res);

//                char upper_buf[128];

//                strncpy(upper_buf, tmp_qr_res, sizeof(upper_buf) - 1);
//                upper_buf[sizeof(upper_buf) - 1] = '\0';

//                vm_upper_case(upper_buf, upper_buf);

//                if (strncmp(upper_buf, "TEL:", 4) == 0) {
//                    char* phone_num = tmp_qr_res + 4;

//                    if (strchr(phone_num, ';') || strchr(phone_num, ',')) {
//                        /* Multiple phone numbers detected */
//                        // trigger_error_state("Multiple numbers");

//                        reset_to_preview();
//                        break;
//                    }

//                    char normalized[128];

//                    normalize_phone(normalized, phone_num);

//                    if (is_phone_number(normalized)) {
//                        char temp_text[128];

//                        snprintf(temp_text, sizeof(temp_text), "Start phone call: %s", normalized);

//                        vm_ascii_to_ucs2(wtxt, sizeof(wtxt), temp_text);

//                        vm_graphic_fill_rect(layer_buf0, 0, sw, sw, sh / 4, VM_COLOR_WHITE, VM_COLOR_WHITE);

//                        vm_graphic_textout(layer_buf0, 2, 250, wtxt, vm_wstrlen(wtxt), VM_COLOR_BLACK);

//                        vm_graphic_flush_layer(layer_hdl, 1);

//                         make_background_call(normalized);

//                    } else {
//                        //                    trigger_error_state("Wrong number format");
//                    }

//                } else {
//                    //                    trigger_error_state("No number");
//                }
            } else {
                reset_to_preview();
            }
            break;
        default:
            break;
    }
}

void app_set_current_preview_size(VM_CAMERA_HANDLE camera_handle) {

    const vm_cam_size_t* ptr = NULL;
    VMUINT size = 0, i = 0;
    if (vm_camera_get_support_preview_size(camera_handle, &ptr, &size) == VM_CAM_SUCCESS) {
        vm_cam_size_t my_cam_size;
        for (i = 0; i < size; i++) {
            my_cam_size.width = (ptr + i)->width;
            my_cam_size.height = (ptr + i)->height;
        }
        my_cam_size.width = sw;   // 240; 240; sw
        my_cam_size.height = sh;  // 320; 240; sh

        vm_camera_set_preview_size(camera_handle, &my_cam_size);
    }
}

void invertGrayBuffer(unsigned char* buf, unsigned int size) {

    unsigned int i;
    for (i = 0; i < size; ++i) {
        buf[i] = 255 - buf[i];
    }
}

void uyvyToGrey(unsigned char* dst, unsigned char* src, unsigned int numberPixels) {

    while (numberPixels > 0) {
        src++;
        uint8_t y1 = *(src++);
        src++;
        uint8_t y2 = *(src++);
        *(dst++) = y1;
        *(dst++) = y2;
        numberPixels -= 2;
    }
}

char* decode_qr(unsigned char* data, size_t data_sz, int width, int height) {

    char* decoded_str = NULL;
    zbar_image_scanner_t* scanner = zbar_image_scanner_create();
    zbar_image_t* image = zbar_image_create();
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);
    zbar_image_set_format(image, *(int*)"GREY");
    zbar_image_set_size(image, width, height);
    zbar_image_set_data(image, data, data_sz, NULL);
    if (zbar_scan_image(scanner, image) > 0) {
        const zbar_symbol_t* symbol = zbar_image_first_symbol(image);
        if (symbol) {
            const char* symbol_data = zbar_symbol_get_data(symbol);
            decoded_str = vm_malloc(strlen(symbol_data) + 1);
            if (decoded_str) {
                strcpy(decoded_str, symbol_data);
            }
        }
    }
    zbar_image_destroy(image);
    zbar_image_scanner_destroy(scanner);
    return decoded_str;
}

unsigned char claim(int x) { return x > 255 ? 255 : (x < 0 ? 0 : x); }

//static VMBOOL is_phone_number(const char* s) {

//    int digits = 0;

//    if (!s || !*s) return VM_FALSE;

//    if (*s == '+') s++;

//    while (*s) {
//        if (*s < '0' || *s > '9') return VM_FALSE;

//        digits++;
//        s++;
//    }

//    return (digits >= 5);
//}

void scan_enable_timer_callback(VMINT tid) {

    vm_delete_timer(tid);
    scan_timer_id = -1;

    scan_enabled = VM_TRUE;
}

//void normalize_phone(char* dst, const char* src) {

//    char* dst_start = dst;

//    while (*src == ' ' || *src == '\t') {
//        src++;
//    }

//    while (*src) {
//        if (*src >= '0' && *src <= '9') {
//            *dst++ = *src;
//        } else if (*src == '+' && dst == dst_start) {
//            *dst++ = *src;
//        }

//        src++;
//    }

//    *dst = '\0';
//}

VMUINT32 ime_cb(VMINT32 h, vm_editor_message_struct_p m) {

    if (!m) return 0;
    switch (m->message_id) {
        case VM_EDITOR_MESSAGE_ACTIVATE:
        case VM_EDITOR_MESSAGE_REDRAW_FLOATING_UI:
        case VM_EDITOR_MESSAGE_REDRAW_IMUI_RECTANGLE:
            break;
        case VM_EDITOR_MESSAGE_DEACTIVATE:
            break;
        default:
            break;
    }
    return 0;
}

void smoll_size(void) //smoll size
{

    vm_editor_set_softkey(input_editor, (VMUWSTR)left_label, VM_LEFT_SOFTKEY, func_left_key); //History
    vm_editor_set_softkey(input_editor, (VMUWSTR)center_label, VM_CENTER_SOFTKEY, func_center_key); //Stop
    vm_editor_set_pos(input_editor, 0, sw);
    vm_editor_set_size(input_editor, sw, input_h);

    if (last_preview)
    {
        memcpy(layer_buf0, last_preview, sw * sw * 2);
    }

    vm_graphic_flush_layer(layer_hdl, 1);
    vm_editor_redraw_ime_screen();
    vm_editor_show(input_editor);

}

void full_size(void) //full size
{

    vm_editor_set_softkey(input_editor, (VMUWSTR)left_label1, VM_LEFT_SOFTKEY, func_left_key); //History
    vm_editor_set_softkey(input_editor, (VMUWSTR)center_label1, VM_CENTER_SOFTKEY, func_center_key); //Save
    vm_editor_set_pos(input_editor, 0, 0);
    vm_editor_set_size(input_editor, sw, sh - vm_editor_get_softkey_height());
    vm_editor_redraw_ime_screen();
    vm_editor_show(input_editor);
}

void create_auto_filename(VMWSTR text) {

    VMINT drv;
    VMCHAR fAutoFileName[100] = {0};
    VMCHAR fData_text[100] = {0};
    struct vm_time_t curr_time;

    vm_get_time(&curr_time);

    if ((drv = vm_get_removable_driver()) < 0) {
       drv = vm_get_system_driver();
    }

    sprintf(fAutoFileName, "%c:\\", drv);
    sprintf(fData_text, "%02d%02d%02d%02d%02d.txt", curr_time.mon, curr_time.day, curr_time.hour, curr_time.min, curr_time.sec);
    strcat(fAutoFileName, fData_text);
    vm_ascii_to_ucs2(text, (strlen(fAutoFileName) + 1) * 2, fAutoFileName);
}

static VMINT save_history_to_utf8_chunked(VMWSTR outfile_path)
{
    if (!outfile_path)
        return -1;

    VMFILE fh = vm_file_open(outfile_path, MODE_CREATE_ALWAYS_WRITE, FALSE);

    if (fh < 0)
        return -2;

    VMWCHAR *u_chunk = (VMWCHAR *)vm_calloc((UCS2_CHUNK_WORDS + 4) * sizeof(VMWCHAR));

    VMCHAR *out_utf8 = (VMCHAR *)vm_calloc(VM_CHSET_DEST_BYTES + 8);

    if (!u_chunk || !out_utf8)
    {
        if (u_chunk)
            vm_free(u_chunk);

        if (out_utf8)
            vm_free(out_utf8);

        vm_file_close(fh);

        return -3;
    }

VMINT history_len = ucs2_len_words;

    VMINT remaining = history_len;
    VMINT pos = 0;

    while (remaining > 0)
    {
        VMINT take = (remaining > UCS2_CHUNK_WORDS) ? UCS2_CHUNK_WORDS : remaining;

vm_wstrncpy(u_chunk, ucs2_buf + pos, take);

        u_chunk[take] = 0;

        VMINT conv = vm_chset_convert(VM_CHSET_UCS2, VM_CHSET_UTF8, (VMCHAR *)u_chunk, (VMCHAR *)out_utf8, VM_CHSET_DEST_BYTES);

        if (conv != VM_CHSET_CONVERT_SUCCESS &&
            conv != 0)
        {
            vm_free(u_chunk);
            vm_free(out_utf8);
            vm_file_close(fh);

            return -4;
        }

        VMUINT written = 0;

        vm_file_write(fh, out_utf8, (VMUINT)strlen((VMSTR)out_utf8), &written);

        pos += take;
        remaining -= take;
    }

    vm_free(u_chunk);
    vm_free(out_utf8);

    vm_file_close(fh);

    return 0;
}

void save_history(void)
{
    create_auto_filename(outfile);
    save_history_to_utf8_chunked(outfile);
}

void append_input(VMSTR msg)
{

    vm_editor_set_softkey(input_editor, (VMUWSTR)center_label, VM_CENTER_SOFTKEY, func_center_key); //Next

if (!ucs2_buf || !msg || msg[0] == 0)
    return;

    VMWCHAR scan_text[VM_CHSET_DEST_BYTES]; //4096
    VMWCHAR nl[2];

VMINT used = ucs2_len_words;

    vm_chset_convert(VM_CHSET_UTF8, VM_CHSET_UCS2, msg, (VMSTR)scan_text, VM_CHSET_DEST_BYTES);

VMINT needed = (used + vm_wstrlen(scan_text) + 2) * sizeof(VMWCHAR);

if (ensure_ucs2_capacity_bytes(needed) < 0)
{
    return;
}

//    mre_create_msg(scan_text);
//    vm_wstrcat((VMWSTR)input_buf, scan_text);

    nl[0] = '\n';
    nl[1] = 0;

if (ucs2_len_words > 0)
{
    first_row = VM_TRUE;
}
else
{
    first_row = VM_FALSE;
}


    if(first_row == VM_TRUE)
    {
       append_ucs2_words(nl, 1);

    } else {
       first_row = VM_TRUE;
    }


append_ucs2_words(scan_text, vm_wstrlen(scan_text));

vm_editor_set_text(input_editor, ucs2_buf, ucs2_buf_bytes);
    vm_editor_redraw_ime_screen();
    vm_editor_show(input_editor);

}

//static void mre_msg_callback(vm_sms_callback_t* callback_data) {

//    if (callback_data->result != 1) {
//        if (sms_data)
//        {
//            vm_free(sms_data);
//            sms_data = NULL;
//        }
//        return;
//    }

//    if (callback_data->cause != VM_SMS_CAUSE_NO_ERROR) {
//       if (sms_data)
//       {
//           vm_free(sms_data);
//           sms_data = NULL;
//       }
//        return;
//    }

//    switch (callback_data->action) {

//        case VM_SMS_ACTION_NONE:
//            sms_data = NULL;
//            if (sms_data)
//            {
//                vm_free(sms_data);
//                sms_data = NULL;
//            }
//            break;

//        case VM_SMS_ACTION_SAVE:
//            if (sms_data)
//            {
//                vm_free(sms_data);
//                sms_data = NULL;
//            }
//            break;

//        case VM_SMS_ACTION_DELETE:
//            if (sms_data)
//            {
//                vm_free(sms_data);
//                sms_data = NULL;
//            }
//            break;

//        default:
//            if (sms_data)
//            {
//                vm_free(sms_data);
//                sms_data = NULL;
//            }
//            break;
//    }
//}

//VMINT mre_create_msg(VMWSTR data)
//{
//    vm_sms_add_msg_data_t msg_data;
//    struct vm_time_t curr_time;
//    VMWSTR ucs2_data;

//    memset(&msg_data, 0, sizeof(msg_data));

//    vm_get_time(&curr_time);

//    VMINT len = vm_wstrlen(data);

//    ucs2_data = vm_malloc((len + 1) * sizeof(VMWCHAR));
//    if (!ucs2_data)
//        return -1;

//    vm_wstrcpy(ucs2_data, data);

//    msg_data.status = VM_SMS_STATUS_DRAFT;
////    msg_data.sim_id = VM_SMS_SIM_1;
////    msg_data.sim_id = VM_SMS_SIM_NONE;
////    msg_data.storage_type = VM_SMS_STORAGE_ME;
////    msg_data.timestamp = curr_time;

//    msg_data.content = (VMSTR)ucs2_data;

//    /* check SDK docs, usually UCS2 characters count */
////    msg_data.content_size = len + 1;
////    msg_data.content_size = vm_wstrlen(ucs2_data) + 1;
//    msg_data.content_size = (vm_wstrlen(ucs2_data) + 1) * sizeof(VMWCHAR);

////sms_data = NULL;

//sms_data = ucs2_data;

//vm_sms_add_msg(&msg_data, mre_msg_callback, NULL);

////    vm_sms_add_msg(&msg_data, mre_msg_callback, NULL);

////    vm_free(ucs2_data);

//    return 0;
//}

void change_label_name(void) {

    stop_scan();

    if (handle_ptr != -1)
    {
        vm_release_camera_instance(handle_ptr);
        handle_ptr = -1;
    }

    response = vm_selector_run(0, 0, job);

}

static VMINT job(VMWCHAR *filepath, VMINT wlen) {

    trigeris1 = VM_TRUE;

    vm_wstrcpy(outfile, filepath);

    app_load_file_into_editor(filepath);

    return 0;

}

VMINT app_load_file_into_editor(VMWSTR inpath)
{
    if (!inpath)
        return -1;

    ucs2_buf_init_empty();

    VMINT res = load_utf8_file_to_ucs2_chunked(inpath);

    if (res < 0)
        return res;

    vm_editor_set_default_text(input_editor, ucs2_buf);

    return res;
}

static void ucs2_buf_init_empty(void)
{
    if (ucs2_buf) {
        vm_free(ucs2_buf);
        ucs2_buf = NULL;
    }

    ucs2_buf_bytes = 0;
    ucs2_len_words = 0;
    ensure_ucs2_capacity_bytes(INITIAL_UCS2_BYTES);
    if (ucs2_buf) ucs2_buf[0] = 0;
}

static VMINT load_utf8_file_to_ucs2_chunked(VMWSTR file_path)
{
    if (!file_path) return -1;
    VMFILE fh = vm_file_open(file_path, MODE_READ, TRUE);  //TRUE FALSE
    if (fh < 0) return -1;

    uint8_t *read_buf = (uint8_t *)vm_calloc(READ_CHUNK_BYTES + 8); /* +8 safety */
    VMWCHAR *temp_ucs2 = (VMWCHAR *)vm_calloc((UCS2_CHUNK_WORDS + 4) * sizeof(VMWCHAR));
    if (!read_buf || !temp_ucs2) {
        if (read_buf) vm_free(read_buf);
        if (temp_ucs2) vm_free(temp_ucs2);
        vm_file_close(fh);
        return -1;
    }

    ucs2_len_words = 0;
    VMUINT nread = 0;
    while (!vm_file_is_eof(fh)) {
        VMINT rc = vm_file_read(fh, read_buf, READ_CHUNK_BYTES, &nread);
        if (rc <= 0 && !vm_file_is_eof(fh)) {
            vm_free(read_buf);
            vm_free(temp_ucs2);
            vm_file_close(fh);
            return -2;
        }
        if (nread == 0) break;

        read_buf[nread] = 0;

        VMINT conv = vm_chset_convert(VM_CHSET_UTF8, VM_CHSET_UCS2, (VMCHAR *)read_buf, (VMCHAR *)temp_ucs2, VM_CHSET_DEST_BYTES);
        if (conv != VM_CHSET_CONVERT_SUCCESS && conv != 0) {
            vm_free(read_buf);
            vm_free(temp_ucs2);
            vm_file_close(fh);
            return -3;
        }

        VMINT words = vm_wstrlen(temp_ucs2);
        if (words > 0) {
            if (append_ucs2_words(temp_ucs2, words) < 0) {
                vm_free(read_buf);
                vm_free(temp_ucs2);
                vm_file_close(fh);
                return -4; /* out of space */
            }
        }
    }

    vm_free(read_buf);
    vm_free(temp_ucs2);
    vm_file_close(fh);
    return ucs2_len_words;
}

static VMINT append_ucs2_words(VMWSTR src, VMINT src_words)
{
    if (!src || src_words <= 0) return 0;

    VMINT needed_bytes = (ucs2_len_words + src_words + 1) * sizeof(VMWCHAR);
    if (needed_bytes > MAX_UCS2_BYTES) return -1;

    if (ensure_ucs2_capacity_bytes(needed_bytes) < 0) return -1;

    VMINT free_words = (ucs2_buf_bytes / sizeof(VMWCHAR)) - ucs2_len_words;
    if (free_words <= 0) return -1;

VMINT copied = vm_safe_wstrcpy(ucs2_buf + ucs2_len_words, free_words, src);

ucs2_len_words += copied;
ucs2_buf[ucs2_len_words] = 0;

    return copied;
}

static VMINT ensure_ucs2_capacity_bytes(VMINT needed_bytes)
{
    if (needed_bytes <= ucs2_buf_bytes) return 0;
    if (needed_bytes > MAX_UCS2_BYTES) return -1;
    VMINT new_bytes = ucs2_buf_bytes ? ucs2_buf_bytes * 2 : INITIAL_UCS2_BYTES;
    while (new_bytes < needed_bytes) {
        new_bytes *= 2;
        if (new_bytes > MAX_UCS2_BYTES) { new_bytes = MAX_UCS2_BYTES; break; }
    }
    if (new_bytes > MAX_UCS2_BYTES) new_bytes = MAX_UCS2_BYTES;
    if (new_bytes < needed_bytes) return -1;

    if (ucs2_buf == NULL) {
        ucs2_buf = (VMUWSTR)vm_malloc(new_bytes);
        if (!ucs2_buf) {
            ucs2_buf_bytes = 0;
            return -1;
        }
    } else {
        void *p = NULL;
        p = vm_realloc(ucs2_buf, new_bytes);
        if (!p) return -1;
        ucs2_buf = (VMUWSTR)p;
    }
    ucs2_buf_bytes = new_bytes;
    return 0;
}
