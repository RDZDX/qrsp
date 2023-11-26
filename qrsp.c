#include "qrsp.h"
#include "zbar.h"

VM_CAMERA_HANDLE handle_ptr;
VMUINT8 *layer_buf0 = NULL;
char *tmp_qr_res = NULL;
int font_height = 16;

VMWCHAR oneDisplay[40][61];

void vm_main(void) {

    layer_hdl[0] = -1;
    vm_reg_sysevt_callback(handle_sysevt);
    vm_reg_keyboard_callback(handle_keyevt);
    vm_font_set_font_size(VM_SMALL_FONT);

}

void handle_sysevt(VMINT message, VMINT param) {

    switch (message) {
        case VM_MSG_CREATE:
        case VM_MSG_ACTIVE:
            layer_hdl[0] = vm_graphic_create_layer(0, 0, vm_graphic_get_screen_width(), vm_graphic_get_screen_height(), -1);
            vm_graphic_set_clip(0, 0, vm_graphic_get_screen_width(), vm_graphic_get_screen_height());
            layer_buf0 = vm_graphic_get_layer_buffer(layer_hdl[0]);
            vm_switch_power_saving_mode(turn_off_mode);
            break;

        case VM_MSG_PAINT:
            vm_switch_power_saving_mode(turn_off_mode);
            start_cam_preview();
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

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_LEFT_SOFTKEY) {
       start_cam_preview();
    }

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_UP) {
    fromArrToDisplay();
    }

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_DOWN) {
    fromArrToDisplay2();
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
                    stop_preview();
                    break;
                }
            }
            break;
        case VM_CAM_PREVIEW_STOP_DONE:
            vm_release_camera_instance(handle_ptr);
            vm_vibrator_once();
            //vm_audio_play_beep();
            parseTextToArr();
            fromArrToDisplay();
            checkFileExist();
            mre_create_msg((VMWSTR)tmp_qr_res);
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
    return decoded_str;

}

void stop_preview() {

     int res = vm_camera_preview_stop(handle_ptr); 

}

void checkFileExist(void) {

    VMFILE f_read;
    VMUINT nwrite;
    VMCHAR new_data[2000];
    VMWCHAR file_pathw[100];
    VMWCHAR fAutoFileName[100];
    VMWCHAR wAutoFileName[100];
    VMWCHAR file_name[100];

    sprintf(new_data, "%s", tmp_qr_res);

    create_app_txt_filename(file_name);
    create_auto_full_path_name(file_pathw, file_name);
    f_read = vm_file_open(file_pathw, MODE_READ, FALSE);

    if (f_read < 0) {
        vm_file_close(f_read);
        f_read = vm_file_open(file_pathw, MODE_CREATE_ALWAYS_WRITE, FALSE);
        vm_file_write(f_read, new_data, strlen(new_data), &nwrite);
        new_data[nwrite] = '\0';
        vm_file_close(f_read);
    } else {
        vm_file_close(f_read);
        create_auto_filename(fAutoFileName);
        create_auto_full_path_name(wAutoFileName, fAutoFileName);
        vm_file_rename((VMWSTR)file_pathw, (VMWSTR)wAutoFileName);
        f_read = vm_file_open(file_pathw, MODE_CREATE_ALWAYS_WRITE, FALSE);
        vm_file_write(f_read, new_data, strlen(new_data), &nwrite);
        new_data[nwrite] = '\0';
        vm_file_close(f_read);
    }

}

VMINT parseTextToArr(void) {

    VMWSTR sKonv_stringas = {0};
    VMCHAR vns_simbl[2] = {0};
    VMCHAR value10[2000] = {0};
    VMCHAR nauj_strng[61] = {0};
    VMCHAR konv_stringas[MRE_STR_SIZE_MAX + 24] = {0};

    int i, strng_plot, isve_i_ekr_eil_sk, nauj_strng_ilg;
    
    strng_plot = 0;
    isve_i_ekr_eil_sk = 0;
    nauj_strng_ilg = 0;

    memset(oneDisplay, 0, sizeof(oneDisplay[0][0]) * 40 * 61);

    sprintf(value10, "%s", tmp_qr_res);

        char *ptr = value10;

        while (*ptr != '\0') {
            sprintf(vns_simbl, "%c", *ptr);
            strcat(nauj_strng, vns_simbl);
            vm_chset_convert(VM_CHSET_UTF8, VM_CHSET_UCS2, nauj_strng, konv_stringas, (strlen(nauj_strng) + 1) * 2);
            sKonv_stringas = (VMWSTR)konv_stringas;
            strng_plot = vm_graphic_get_string_width(sKonv_stringas);

            if (strng_plot > 235) {
                vm_wstrcpy((VMWSTR)&oneDisplay[isve_i_ekr_eil_sk - 1][61], sKonv_stringas);
                nauj_strng_ilg = strlen(nauj_strng) - 1;
                isve_i_ekr_eil_sk = isve_i_ekr_eil_sk + 1;
                strcpy(nauj_strng, "");
                ptr - 2;
            }

            if (isve_i_ekr_eil_sk == 40) {
                break;
            }

            ptr++;
        }

        strcpy(value10, nauj_strng);
        strcpy(nauj_strng, "");
 
        if (isve_i_ekr_eil_sk < 40 && strng_plot == 235 || isve_i_ekr_eil_sk < 40 && strng_plot < 235) {
            vm_wstrcpy((VMWSTR)&oneDisplay[isve_i_ekr_eil_sk - 1][61], sKonv_stringas);
        }

    nauj_strng_ilg = 0;
    strcpy(vns_simbl, "");
    strcpy(nauj_strng, "");
    strcpy(value10, "");
    strcpy(konv_stringas, "");

    return 0;
}

void fromArrToDisplay(void) {

    VMWCHAR s[MRE_STR_SIZE_MAX + 23];
    int w;
    VMINT g_mre_curr_y = 1;

    layer_buf0 = vm_graphic_get_layer_buffer(layer_hdl[0]);
    vm_graphic_fill_rect(layer_buf0, MRE_SCREEN_START_X, MRE_SCREEN_START_Y, vm_graphic_get_screen_width(),
    vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_WHITE);

    vm_wstrcpy(s, (VMWSTR)&oneDisplay[0][0]);
    w = vm_graphic_get_string_width(s);
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[1][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[2][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[3][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[4][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[5][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[6][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[7][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[8][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[9][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[10][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[11][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[12][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[13][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[14][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[15][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[16][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[17][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[18][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[19][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    vm_graphic_flush_layer(layer_hdl, 1);
}

void fromArrToDisplay2(void) {

    VMWCHAR s[MRE_STR_SIZE_MAX + 23];
    int w;
    VMINT g_mre_curr_y = 1;

    layer_buf0 = vm_graphic_get_layer_buffer(layer_hdl[0]);
    vm_graphic_fill_rect(layer_buf0, MRE_SCREEN_START_X, MRE_SCREEN_START_Y, vm_graphic_get_screen_width(),
    vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_WHITE);

    vm_wstrcpy(s, (VMWSTR)&oneDisplay[20][0]);
    w = vm_graphic_get_string_width(s);
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[21][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[22][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[23][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[24][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[25][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[26][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[27][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[28][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[29][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[30][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[31][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[32][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[33][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[34][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[35][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[36][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[37][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[38][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[39][0]);
    g_mre_curr_y = g_mre_curr_y + font_height;
    vm_graphic_textout_by_baseline(layer_buf0, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK, 12);

    vm_graphic_flush_layer(layer_hdl, 1);
}

void app_set_current_preview_size(VM_CAMERA_HANDLE camera_handle)
{
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

VMINT mre_create_msg(VMWSTR data) {

        vm_sms_add_msg_data_t msg_data;
        void *user_data;
        VMWCHAR ucs2_data[2000];
        VMUINT nread;

        mre_delete_msg();

        user_data = vm_malloc(100);
        vm_ascii_to_ucs2(ucs2_data, (wstrlen(data) + 1) * 2, (VMSTR)data);

        msg_data.status = VM_SMS_STATUS_DRAFT;
        msg_data.storage_type = VM_SMS_STORAGE_ME;
        msg_data.sim_id = VM_SMS_SIM_1;
        msg_data.content_size = wstrlen(ucs2_data);
        msg_data.content = (VMSTR)ucs2_data;

        vm_sms_add_msg(&msg_data, mre_msg_callback, user_data);
        vm_free(user_data);
        user_data = NULL;
        return 0;
}

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
                //vm_vibrator_once();
                break;
            case  VM_SMS_ACTION_DELETE :
                //vm_vibrator_once();
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

static void mre_delete_msg()
{
    void *user_data;
    user_data = vm_malloc(100);
    vm_sms_delete_msg_list(VM_SMS_BOX_DRAFTS, VM_SMS_SIM_1, mre_msg_callback, user_data);
    vm_free(user_data);
    user_data = NULL;
 
}

void create_app_txt_filename(VMWSTR text) {

    VMWCHAR fullPath[100];
    VMWCHAR appName[100];
    VMCHAR asciiAppName[100];
    VMCHAR file_name[100];

    vm_get_exec_filename(fullPath);
    vm_get_filename(fullPath, appName);
    vm_ucs2_to_ascii(asciiAppName, wstrlen(appName) + 1, appName);
    memcpy(file_name, asciiAppName, strlen(asciiAppName) - 3);
    file_name[strlen(asciiAppName) - 3] = '\0';
    strcat(file_name, "txt");
    vm_ascii_to_ucs2(text, (strlen(file_name) + 1) * 2, file_name);

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
