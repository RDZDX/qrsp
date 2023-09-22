#include "qrsp.h"
#include "zbar.h"

VM_CAMERA_HANDLE handle_ptr;
VMUINT8 *layer_buf0 = NULL;
void *my_user_data;
vm_camera_para_struct myPara;
struct vm_time_t curr_time;
VMCHAR file_name[101] = "scannqr.txt";
char *tmp_qr_res = NULL;


//-------------------------------------------------

VMWSTR path;
int dynamicPosition = 0;
//VMWSTR oneDisplay[20][61];
VMWSTR oneDisplay[40][61];

//---------------------------------------------

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
            draw_black_rectangle();
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
            // vm_free(myFilePath);
            // vm_free(myTempFile);
            // vm_free(myRenameFile);
            vm_release_camera_instance(handle_ptr);
            break;
    }

}

void handle_keyevt(VMINT event, VMINT keycode) {

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_RIGHT_SOFTKEY) {
        if (layer_hdl[0] != -1) {
            vm_graphic_delete_layer(layer_hdl[1]);
        }
        // vm_release_camera_instance(handle_ptr);
        vm_exit_app();
    }

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_LEFT_SOFTKEY) {
    //     start_cam_preview();
    fromArrToDisplay();
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
    vm_camera_register_notify(handle_ptr, (VM_CAMERA_STATUS_NOTIFY)cam_message_callback, my_user_data);
    app_set_preview_size();
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
            // vm_vibrator_once();
            vm_audio_play_beep();
            parseTextToArr();
            fromArrToDisplay();
            checkFileExist();
            // vm_release_camera_instance(handle_ptr);
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

    VMCHAR file_path[100];
    VMCHAR new_data[2000];
    VMUINT nwrite;
    VMINT drv;
    VMWCHAR file_pathw[101];
    VMFILE f_read;

    sprintf(new_data, "%s", tmp_qr_res);

    if ((drv = vm_get_removable_driver()) < 0) {
        drv = vm_get_system_driver();
    }

    sprintf(file_path, "%c:\\%s", drv, file_name);
    vm_ascii_to_ucs2(file_pathw, 100, file_path);
    f_read = vm_file_open(file_pathw, MODE_READ, FALSE);

    if (f_read < 0) {
        vm_file_close(f_read);

        f_read = vm_file_open(file_pathw, MODE_CREATE_ALWAYS_WRITE, FALSE);
        vm_file_write(f_read, new_data, strlen(new_data), &nwrite);
        new_data[nwrite] = '\0';
        vm_file_close(f_read);

    } else {
        vm_file_close(f_read);

        VMCHAR fAutoFileName[100];
        VMWCHAR wAutoFileName[101];
        vm_get_time(&curr_time);
        sprintf(fAutoFileName, "%c:\\%d%d%d%d%d.txt", drv, curr_time.mon, curr_time.day, curr_time.hour, curr_time.min, curr_time.sec);
        vm_ascii_to_ucs2(wAutoFileName, 100, fAutoFileName);
        vm_file_rename((VMWSTR)file_pathw, (VMWSTR)wAutoFileName);

        f_read = vm_file_open(file_pathw, MODE_CREATE_ALWAYS_WRITE, FALSE);
        vm_file_write(f_read, new_data, strlen(new_data), &nwrite);
        new_data[nwrite] = '\0';
        vm_file_close(f_read);
    }

}

void app_set_preview_size(void) {

    char tmp[1000];
    vm_cam_size_t *ptr = NULL;
    VMUINT size = 0, i = 0;
    if (vm_camera_get_support_preview_size(handle_ptr, (const vm_cam_size_t **)&ptr, &size) == VM_CAM_SUCCESS) {
        vm_cam_size_t my_cam_size;
        sprintf(tmp, "");
        for (i = 0; i < size; i++) {
            sprintf(tmp, "%s, %dx%d", tmp, (ptr + i)->width, (ptr + i)->height);
            my_cam_size.width = (ptr + i)->width;
            my_cam_size.height = (ptr + i)->height;
        }
        write(tmp);
        my_cam_size.width = 240;
        my_cam_size.height = 320;
        vm_camera_set_preview_size(handle_ptr, &my_cam_size);
    }

}

void draw_black_rectangle(void) {

    layer_buf0 = vm_graphic_get_layer_buffer(layer_hdl[0]);
    vm_graphic_fill_rect(layer_buf0, 0, 0, vm_graphic_get_screen_width(),
                         vm_graphic_get_screen_height(), VM_COLOR_BLACK, VM_COLOR_BLACK);
    vm_graphic_flush_layer(layer_hdl, 1);

}

void draw_white_rectangle(void) {

    layer_buf0 = vm_graphic_get_layer_buffer(layer_hdl[0]);
    vm_graphic_fill_rect(layer_buf0, 0, 0, vm_graphic_get_screen_width(),
                         vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_WHITE);
    vm_graphic_flush_layer(layer_hdl, 1);

}

VMINT parseTextToArr(void) {

    VMWSTR sValue21 = {0};
    VMWSTR sKonv_stringas = {0};
    VMWSTR sTemp_strng = {0};
    VMCHAR value1[MRE_STR_SIZE_MAX + 23] = {0};
    VMCHAR vns_simbl[MRE_STR_SIZE_MAX] = {0};
    //VMCHAR value10[MRE_STR_SIZE_MAX + 23] = {0}; //??????????????????????????????????????????????????????????
    //VMCHAR value10[600] = {0}; //??????????????????????????????????????????????????????????
    VMCHAR value10[2000] = {0}; //??????????????????????????????????????????????????????????
    VMCHAR nauj_strng[MRE_STR_SIZE_MAX + 23] = {0};
    //VMCHAR value21[MRE_STR_SIZE_MAX + 23] = {0}; //????????????????????????????????????????????????????
    //VMCHAR value21[600] = {0}; //????????????????????????????????????????????????????
    VMCHAR value21[2000] = {0}; //????????????????????????????????????????????????????
    VMCHAR konv_stringas[MRE_STR_SIZE_MAX + 23] = {0};
    VMCHAR knv_vns_simblX[MRE_STR_SIZE_MAX] = {0};
    VMWCHAR knv_vns_simbl[MRE_STR_SIZE_MAX] = {0};

    int myFlPosBackCurr = 0;
    int myFlPosBackTemp = 0;
    int myFlPosBackPrev = 0;

    int i, plotis, strng_plot, isve_i_ekr_eil_sk, nauj_strng_ilg, combUodegosIlgis, uodegosIlgis, prasisk_count;
    
    plotis = 0;
    strng_plot = 0;
    isve_i_ekr_eil_sk = 0;
    nauj_strng_ilg = 0;
    combUodegosIlgis = 0;
    uodegosIlgis = 0;
    prasisk_count = 0;

    //memset(oneDisplay, 0, sizeof(oneDisplay[0][0]) * 20 * 61);
    memset(oneDisplay, 0, sizeof(oneDisplay[0][0]) * 40 * 61);

    sprintf(value10, "%s", tmp_qr_res);

    //for (i = 0; i < 20; i++)  // 20 prasisukimu - 20 tekstiniu ekrano eiluciu, kadangi uodega
    for (i = 0; i < 40; i++)  // 20 prasisukimu - 20 tekstiniu ekrano eiluciu, kadangi uodega
    {
        //if (isve_i_ekr_eil_sk == 20) {break;}
        if (isve_i_ekr_eil_sk == 40) {break;}

        vm_chset_convert(VM_CHSET_UTF8, VM_CHSET_UCS2, value10, value21, (strlen(value10) + 1) * 2);
        sValue21 = (VMWSTR)value21;
        plotis = vm_graphic_get_string_width(sValue21);

        if (plotis < 228 && prasisk_count < 4 || plotis == 228 && prasisk_count < 4 ) {
           prasisk_count = prasisk_count + 1;
           continue;
        }

        prasisk_count = 0; 

        char *ptr = value10;

        // while (*ptr != '\0' || *ptr != '\n') {

        while (*ptr != '\0') {
            sprintf(vns_simbl, "%c", *ptr);
            strcat(nauj_strng, vns_simbl);
            vm_chset_convert(VM_CHSET_UTF8, VM_CHSET_UCS2, nauj_strng, konv_stringas, (strlen(nauj_strng) + 1) * 2);
            sKonv_stringas = (VMWSTR)konv_stringas;
            strng_plot = vm_graphic_get_string_width(sKonv_stringas);

            if (strng_plot > 235) {
                vm_wstrcpy((VMWSTR)&oneDisplay[isve_i_ekr_eil_sk - 1][61], sKonv_stringas);
                nauj_strng_ilg = strlen(nauj_strng) - 1;  // uodegos koordinatems nustatyti
                isve_i_ekr_eil_sk = isve_i_ekr_eil_sk + 1;
                strcpy(nauj_strng, "");
                ptr - 2;
            }

            //if (isve_i_ekr_eil_sk == 20) {
            if (isve_i_ekr_eil_sk == 40) {
                break;
            }

            sTemp_strng = sKonv_stringas;

            ptr++;
        }

        combUodegosIlgis = strlen(value10);
        uodegosIlgis = combUodegosIlgis - nauj_strng_ilg;
        strcpy(value10, nauj_strng);
        strcpy(nauj_strng, "");

    }
 
        //if (isve_i_ekr_eil_sk < 20 && strng_plot == 235 || isve_i_ekr_eil_sk < 20 && strng_plot < 235) {
        if (isve_i_ekr_eil_sk < 40 && strng_plot == 235 || isve_i_ekr_eil_sk < 20 && strng_plot < 235) {
            vm_wstrcpy((VMWSTR)&oneDisplay[isve_i_ekr_eil_sk - 1][61], sKonv_stringas);
        }

    uodegosIlgis = 0;
    nauj_strng_ilg = 0;
    combUodegosIlgis = 0;

    strcpy(value1, "");
    strcpy(vns_simbl, "");
    strcpy(nauj_strng, "");
    strcpy(value10, "");
    strcpy(value21, "");
    strcpy(konv_stringas, "");
    strcpy(knv_vns_simblX, "");

    return 0;
}

void fromArrToDisplay(void) {

    draw_white_rectangle();

    VMWCHAR s[MRE_STR_SIZE_MAX + 23];
    VMUINT8 *buf;
    int w;
    VMINT g_mre_curr_y = 0;

    buf = vm_graphic_get_layer_buffer(layer_hdl[0]);
    vm_graphic_fill_rect(buf, MRE_SCREEN_START_X, MRE_SCREEN_START_Y, vm_graphic_get_screen_width(),
    vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_WHITE);

    vm_wstrcpy(s, (VMWSTR)&oneDisplay[0][0]);
    w = vm_graphic_get_string_width(s);
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[1][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[2][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[3][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[4][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[5][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[6][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[7][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[8][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[9][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[10][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[11][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[12][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[13][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[14][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[15][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[16][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[17][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[18][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[19][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    vm_graphic_flush_layer(layer_hdl, 1);
}

void fromArrToDisplay2(void) {

    draw_white_rectangle();

    VMWCHAR s[MRE_STR_SIZE_MAX + 23];
    VMUINT8 *buf;
    int w;
    VMINT g_mre_curr_y = 0;

    buf = vm_graphic_get_layer_buffer(layer_hdl[0]);
    vm_graphic_fill_rect(buf, MRE_SCREEN_START_X, MRE_SCREEN_START_Y, vm_graphic_get_screen_width(),
    vm_graphic_get_screen_height(), VM_COLOR_WHITE, VM_COLOR_WHITE);

    vm_wstrcpy(s, (VMWSTR)&oneDisplay[20][0]);
    w = vm_graphic_get_string_width(s);
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[21][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[22][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[23][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[24][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[25][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[26][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[27][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[28][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[29][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[30][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[31][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[32][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[33][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[34][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[35][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[36][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[37][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[38][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    memset(s, 0, MRE_STR_SIZE_MAX);
    vm_wstrcpy(s, (VMWSTR)&oneDisplay[39][0]);
    g_mre_curr_y = g_mre_curr_y + vm_graphic_get_character_height();
    vm_graphic_textout(buf, 0, g_mre_curr_y, s, wstrlen(s), VM_COLOR_BLACK);

    vm_graphic_flush_layer(layer_hdl, 1);
}