#include "qrsp.h"

VM_CAMERA_HANDLE handle_ptr;
VMUINT8 *layer_buf0 = NULL;
char *tmp_qr_res = NULL;
int font_height = 16;
VMWCHAR oneDisplay[40][61];
VMWCHAR file_pathw[100];
//VMCHAR new_data[2440];
VMCHAR new_data[1854];
VMFILE f_read;
VMBOOL capture_stage = VM_TRUE;
VMBOOL missing_data_file = VM_FALSE;
//char *sms_data = NULL;

void vm_main(void) {

    layer_hdl[0] = -1;
    vm_reg_sysevt_callback(handle_sysevt);
    vm_reg_keyboard_callback(handle_keyevt);
    vm_font_set_font_size(VM_SMALL_FONT);
    checkFileExist();

}

void handle_sysevt(VMINT message, VMINT param) {

    switch (message) {
        case VM_MSG_CREATE:
        case VM_MSG_ACTIVE:
            layer_hdl[0] = vm_graphic_create_layer(0, 0, vm_graphic_get_screen_width(), vm_graphic_get_screen_height(), -1);
            vm_graphic_set_clip(0, 0, vm_graphic_get_screen_width(), vm_graphic_get_screen_height());
            layer_buf0 = vm_graphic_get_layer_buffer(layer_hdl[0]);
            vm_switch_power_saving_mode(turn_off_mode);
            start_cam_preview();
            break;

        case VM_MSG_PAINT:
            vm_switch_power_saving_mode(turn_off_mode);
            break;

        case VM_MSG_INACTIVE:
            if (layer_hdl[0] != -1) vm_graphic_delete_layer(layer_hdl[0]);
            layer_hdl[0] = -1;
            layer_buf0 = NULL;
            vm_graphic_reset_clip();
            vm_switch_power_saving_mode(turn_on_mode);
            break;

        case VM_MSG_QUIT:
            if (layer_hdl[1] != -1) vm_graphic_delete_layer(layer_hdl[0]);
            layer_hdl[0] = -1;
            layer_buf0 = NULL;
            vm_release_camera_instance(handle_ptr);
            break;
    }

}

void handle_keyevt(VMINT event, VMINT keycode) {

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_RIGHT_SOFTKEY) {
        if (layer_hdl[0] != -1) {
            vm_graphic_delete_layer(layer_hdl[1]);
        }
        vm_exit_app();
    }

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_LEFT_SOFTKEY && capture_stage == VM_FALSE) {
       capture_stage = VM_TRUE;
       start_cam_preview();
    }

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_UP && capture_stage == VM_FALSE) {
    fromArrToDisplay(0, 20);
    }

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_DOWN && capture_stage == VM_FALSE) {
    fromArrToDisplay(20, 20);
    }

}

void start_cam_preview(void) {

    vm_create_camera_instance(VM_CAMERA_MAIN_ID, &handle_ptr);
    vm_camera_register_notify(handle_ptr, (VM_CAMERA_STATUS_NOTIFY)cam_message_callback, NULL);
    app_set_current_preview_size(handle_ptr);
    vm_camera_preview_start(handle_ptr);

}

void cam_message_callback(vm_cam_notify_data_t *notify_data, void *user_data) {

    vm_cam_frame_data_t my_frame_data;
    int i;

    handle_ptr = notify_data->handle;

    switch (notify_data->cam_message) {

        case VM_CAM_PREVIEW_FRAME_RECEIVED:
            if (vm_camera_get_frame(handle_ptr, &my_frame_data) == VM_CAM_SUCCESS) {
                VMUINT row_pixel = my_frame_data.row_pixel;
                VMUINT col_pixel = my_frame_data.col_pixel;
                VMUINT grey_app_frame_data_size = 0;
                VMUINT uyvy_app_frame_data_size = 0;
                VMUINT8 *grey_app_frame_data = NULL;
                VMUINT8 *uyvy_app_frame_data = NULL;
                VMWCHAR *layer_buf_s = (VMWCHAR *)layer_buf0;
                VMUINT8 *s = (VMUINT8 *)my_frame_data.pixtel_data;

                for (i = 0; i < 240 * 320 / 2; ++i) {
                    int u0 = *s++ - 128;
                    int y0 = *s++ - 16;
                    int v0 = *s++ - 128;
                    int y2 = *s++ - 16;

                    int tmp = 298 * y0 + 128;
                    int vv = 409 * v0;
                    int uv = -100 * u0 - 208 * v0;
                    int uu = 516 * u0;

                    layer_buf_s[i * 2] = VM_COLOR_888_TO_565(claim((tmp + vv) >> 8), claim((tmp + uv) >> 8), claim((tmp + uu) >> 8));
                    tmp = 298 * y2 + 128;
                    layer_buf_s[i * 2 + 1] = VM_COLOR_888_TO_565(claim((tmp + vv) >> 8), claim((tmp + uv) >> 8), claim((tmp + uu) >> 8));
                }

                vm_graphic_flush_layer(layer_hdl, 1);

                uyvy_app_frame_data = my_frame_data.pixtel_data;
                uyvy_app_frame_data_size = row_pixel * col_pixel * 4;
                grey_app_frame_data_size = row_pixel * col_pixel * 2;
                grey_app_frame_data = vm_malloc(grey_app_frame_data_size);
                uyvyToGrey(grey_app_frame_data, uyvy_app_frame_data, grey_app_frame_data_size);
                tmp_qr_res = decode_qr(grey_app_frame_data, grey_app_frame_data_size, col_pixel, row_pixel);

                if (tmp_qr_res) {
                    vm_camera_preview_stop(handle_ptr);
                    break;
                }
            }
            break;
        case VM_CAM_PREVIEW_STOP_DONE:
            vm_release_camera_instance(handle_ptr);
            vm_vibrator_once();
            //vm_audio_play_beep();
            parseTextToArr();
            capture_stage = VM_FALSE;
            fromArrToDisplay(0, 20);
            //mre_create_msg(sms_data);
            writeDataToTextFile();
            break;
        default:
            break;
    }

}

unsigned char claim(int x) {

    return x > 255 ? 255 : (x < 0 ? 0 : x); 

}

void uyvyToGrey(unsigned char *dst, unsigned char *src, unsigned int numberPixels) {

    while (numberPixels > 0) {
        uint8_t y1;
        uint8_t y2;
        src++;
        y1 = *(src++);
        src++;
        y2 = *(src++);
        *(dst++) = y1;
        *(dst++) = y2;
        numberPixels -= 2;
    }

}

char *decode_qr(void *raw, size_t raw_sz, int width, int height) {

    char *decoded_str = NULL;
    zbar_image_scanner_t *scanner = NULL;
    zbar_image_t *image = NULL;
    const zbar_symbol_t *symbol = NULL;
    int n = 0;

    scanner = zbar_image_scanner_create();
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);
    image = zbar_image_create();
    zbar_image_set_format(image, *(int *)"GREY");
    zbar_image_set_size(image, width, height);
    zbar_image_set_data(image, raw, width * height, zbar_image_free_data);
    n = zbar_scan_image(scanner, image);
    symbol = zbar_image_first_symbol(image);

    for (; symbol; symbol = zbar_symbol_next(symbol)) {
        zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
        const char *data = zbar_symbol_get_data(symbol);
        decoded_str = vm_malloc(strlen(data) + 1);
        strcpy(decoded_str, data);
        break;
    }

    zbar_image_destroy(image);
    zbar_image_scanner_destroy(scanner);

    vm_free(decoded_str);

    return decoded_str;

}

void checkFileExist(void) {

    VMWCHAR file_name[100];

    create_app_txt_filename(file_name);
    create_auto_full_path_name(file_pathw, file_name);
    f_read = vm_file_open(file_pathw, MODE_READ, FALSE);

    if (f_read < 0) {
        missing_data_file = VM_TRUE;
    }

    vm_file_close(f_read);

}

void writeDataToTextFile(void) {

    VMUINT nwrite;
    VMWCHAR fAutoFileName[100];
    VMWCHAR wAutoFileName[100];


    if (missing_data_file == VM_TRUE) {
       create_auto_filename(fAutoFileName);
       create_auto_full_path_name(wAutoFileName, fAutoFileName);
       f_read = vm_file_open(wAutoFileName, MODE_CREATE_ALWAYS_WRITE, FALSE); //atsidarom autopavadinima faila ir rasom i ji
       vm_file_write(f_read, new_data, strlen(new_data), &nwrite);
       vm_file_close(f_read);
    } else {
        f_read = vm_file_open(file_pathw, MODE_APPEND, FALSE);
        if (f_read < 0){
          f_read = vm_file_open(file_pathw, MODE_CREATE_ALWAYS_WRITE, FALSE);
        }
        vm_file_write(f_read, new_data, strlen(new_data), &nwrite);
        vm_file_write(f_read, "\n",1, &nwrite);
        vm_file_close(f_read);
    }

    strcpy(new_data, "");
}


VMINT parseTextToArr(void) {

    VMWSTR sKonv_stringas = {0};
    VMCHAR vns_simbl[2] = {0};
    VMCHAR nauj_strng[61] = {0};
    VMCHAR konv_stringas[100 + 24] = {0};

    int strng_plot, isve_i_ekr_eil_sk, nauj_strng_ilg;
    
    strng_plot = 0;
    isve_i_ekr_eil_sk = 0;
    nauj_strng_ilg = 0;

    memset(oneDisplay, 0, sizeof(oneDisplay[0][0]) * 40 * 61);

    sprintf(new_data, "%s", tmp_qr_res);
    //sms_data = new_data;

        while (*tmp_qr_res != '\0') {
            sprintf(vns_simbl, "%c", *tmp_qr_res);
            strcat(nauj_strng, vns_simbl);
            vm_chset_convert(VM_CHSET_UTF8, VM_CHSET_UCS2, nauj_strng, konv_stringas, (strlen(nauj_strng) + 1) * 2);
            sKonv_stringas = (VMWSTR)konv_stringas;
            strng_plot = vm_graphic_get_string_width(sKonv_stringas);

            if (strng_plot > 235) {
                vm_wstrcpy(&oneDisplay[isve_i_ekr_eil_sk - 1][61], sKonv_stringas);
                nauj_strng_ilg = strlen(nauj_strng) - 1;
                isve_i_ekr_eil_sk = isve_i_ekr_eil_sk + 1;
                strcpy(nauj_strng, "");
                *tmp_qr_res - 2;
            }

            if (isve_i_ekr_eil_sk == 40) {
                break;
            }

            *tmp_qr_res++;
        }

        strcpy(nauj_strng, "");
 
        if (isve_i_ekr_eil_sk < 40 && strng_plot == 235 || isve_i_ekr_eil_sk < 40 && strng_plot < 235) {
            vm_wstrcpy(&oneDisplay[isve_i_ekr_eil_sk - 1][61], sKonv_stringas);
        }

    nauj_strng_ilg = 0;
    strcpy(vns_simbl, "");
    strcpy(nauj_strng, "");
    strcpy(konv_stringas, "");

    tmp_qr_res = NULL;

    return 0;
}

void app_set_current_preview_size(VM_CAMERA_HANDLE camera_handle) {

	const vm_cam_size_t* ptr = NULL;

	VMUINT size = 0, i = 0;

	if (vm_camera_get_support_preview_size(camera_handle, &ptr, &size) == VM_CAM_SUCCESS){
           vm_cam_size_t my_cam_size;

           for (i = 0; i < size; i++){
               my_cam_size.width  = (ptr + i)->width;
               my_cam_size.height = (ptr + i)->height;
           }

	 my_cam_size.width  = 240;
	 my_cam_size.height = 320;
         vm_camera_set_preview_size(camera_handle, &my_cam_size);

	}
}

void create_app_txt_filename(VMWSTR text) {

    VMWCHAR fullPath[100];
    VMWCHAR appName[100];
    VMWCHAR wfile_extension[4];
    VMCHAR file_extension[4] = "txt";

    vm_get_exec_filename(fullPath);
    vm_get_filename(fullPath, appName);
    vm_ascii_to_ucs2(wfile_extension, (strlen(file_extension) + 1) * 2, file_extension);
    vm_wstrncpy(text, appName, vm_wstrlen(appName) - 3);
    vm_wstrcat(text, wfile_extension);

}

void create_auto_filename(VMWSTR text) {

    struct vm_time_t curr_time;
    VMCHAR fAutoFileName[100];

    vm_get_time(&curr_time);
    sprintf(fAutoFileName, "%02d%02d%02d%02d%02d.txt", curr_time.mon, curr_time.day, curr_time.hour, curr_time.min, curr_time.sec);
    vm_ascii_to_ucs2(text, (strlen(fAutoFileName) + 1) * 2, fAutoFileName);

}

void create_auto_full_path_name(VMWSTR result, VMWSTR fname) {

    VMINT drv;
    VMCHAR fAutoFileName[100];
    VMWCHAR wAutoFileName[100];

    if ((drv = vm_get_removable_driver()) < 0) {
       drv = vm_get_system_driver();
    }

    sprintf(fAutoFileName, "%c:\\", drv);
    vm_ascii_to_ucs2(wAutoFileName, (strlen(fAutoFileName) + 1) * 2, fAutoFileName);
    vm_wstrcat(wAutoFileName, fname);
    vm_wstrcpy(result, wAutoFileName);

}

void fromArrToDisplay(VMINT first_row, VMINT count_rows) { // 0, 20 20, 20

    VMINT x;
    VMINT g_mre_curr_y = 1;

    layer_buf0 = vm_graphic_get_layer_buffer(layer_hdl[0]);
    vm_graphic_fill_rect(layer_buf0, 0, 0, vm_graphic_get_screen_width(), vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_WHITE);

    for (x = 0; x < count_rows; x++) {
        vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, &oneDisplay[x + first_row][0], vm_wstrlen(&oneDisplay[x + first_row][0]), VM_COLOR_BLACK, 12);
        g_mre_curr_y = g_mre_curr_y + font_height;
    }

    vm_graphic_flush_layer(layer_hdl, 1);
}

/*
static void mre_msg_callback(vm_sms_callback_t *callback_data) {
     
    if(callback_data->result == 1)
    {

        if(callback_data->cause == VM_SMS_CAUSE_NO_ERROR)
        {

           switch(callback_data->action)
            {

            case  VM_SMS_ACTION_NONE :
                break;
            case VM_SMS_ACTION_SAVE :
                break;
            case  VM_SMS_ACTION_DELETE :
                break;
            default :
                break;
            }
        }
        else
        {

        }
    }
    else
    {

    }
}

static void mre_delete_msg() {

    void *user_data;
    user_data = vm_malloc(100);

    vm_sms_delete_msg_list(VM_SMS_BOX_DRAFTS, VM_SMS_SIM_1, mre_msg_callback, user_data);

    vm_free(user_data);
    user_data = NULL;
 
}


VMINT mre_create_msg(char* data) {

        vm_sms_add_msg_data_t msg_data;
        struct vm_time_t curr_time;
        void *user_data;
	VMWSTR ucs2_data;

        mre_delete_msg();

        vm_get_time(&curr_time);

        ucs2_data = vm_malloc((strlen(data) + 1) * 2);

        user_data = vm_malloc(100);
        vm_ascii_to_ucs2(ucs2_data, (strlen(data) + 1) * 2, data);

        msg_data.status = VM_SMS_STATUS_DRAFT;
        msg_data.sim_id = VM_SMS_SIM_1;
        msg_data.storage_type = VM_SMS_STORAGE_ME;
        msg_data.timestamp = curr_time;
        msg_data.content_size = (wstrlen(ucs2_data) + 1) * 2;
        msg_data.content = (VMSTR)ucs2_data;

        vm_sms_add_msg(&msg_data, mre_msg_callback, user_data);
        vm_free(user_data);
        user_data = NULL;
        vm_free(ucs2_data);
        sms_data = NULL;
        return 0;
}
*/