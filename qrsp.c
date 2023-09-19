#include "qrsp.h"
#include "zbar.h"

VM_CAMERA_HANDLE handle_ptr;
VMUINT8 *layer_buf0 = NULL;
VMUINT8 *buffer = NULL;
VMBYTE *camera_buffer;
void *my_user_data;
vm_camera_para_struct myPara;
int i;
int trigeris = 0;
int trigeris1 = 0;
vm_cam_capture_data_t my_capture_data;
// vm_cam_frame_raw_data_t my_raw_frame_data;
// vm_cam_frame_data_t my_frame_data;
struct vm_time_t curr_time;
VMWSTR myCaptureFile;
VMWSTR myTempFile;
VMWSTR myRenameFile;
VMCHAR file_name[101] = "e:\\tmp.jpg";
VMINT sizex;
char *g_last_decoded_qr = NULL;
// char* g_last_decoded_qr_arr[QR_ARR_MAX_SIZE] = {0};

char *tmp_qr_res = NULL;
//char *tmp_qr_res = "test";
//VMCHAR myText[100];

void vm_main(void) {
    layer_hdl[0] = -1;
    vm_reg_sysevt_callback(handle_sysevt);
    vm_reg_keyboard_callback(handle_keyevt);
    // vm_camera_startup();

    sizex = (strlen(file_name) + 1) * 2;
    myTempFile = vm_malloc(sizex);
    vm_ascii_to_ucs2(myTempFile, sizex, file_name);
    myRenameFile = vm_malloc(100);
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
            draw_hello();
            //start_cam_preview();
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
            vm_free(myTempFile);
            vm_free(myRenameFile);
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
        start_cam_preview();
    }

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_OK) {
        my_user_data = 1;
    }

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_NUM1) {
        draw_hello();
    }

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_NUM2) {
       draw_hello1();
    }

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_NUM3) {
       scene_1();
    }

    if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_NUM4) {
       scene_2();
    }

    // if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_NUM5) {
    //    vm_graphic_active_layer(layer_hdl[0]);
    // }

    // if (event == VM_KEY_EVENT_UP && keycode == VM_KEY_NUM6) {
    //    vm_graphic_draw_image_from_file(layer_hdl[0], 0, 0, myCaptureFile);
    //    vm_graphic_flush_layer(layer_hdl, 1);
    // }
}

static void draw_hello(void) {
    VMCHAR myText[100];
    VMWSTR s;
    VMWCHAR display_string[100];
    int x;
    int y;
    int wstr_len;
    vm_graphic_color color;

    vm_get_time(&curr_time);
    sprintf(myText, "%d.%d.%d %d:%d:%d", curr_time.year, curr_time.mon, curr_time.day, curr_time.hour, curr_time.min, curr_time.sec);
    vm_ascii_to_ucs2(display_string, 100, myText);
    s = (VMWSTR)display_string;

    wstr_len = vm_graphic_get_string_width(s);
    x = (vm_graphic_get_screen_width() - wstr_len) / 2;
    y = (vm_graphic_get_screen_height() - vm_graphic_get_character_height()) / 2;
    color.vm_color_565 = VM_COLOR_WHITE;
    vm_graphic_setcolor(&color);
    vm_graphic_fill_rect_ex(layer_hdl[0], 0, 0, vm_graphic_get_screen_width(), vm_graphic_get_screen_height());
    color.vm_color_565 = VM_COLOR_RED;
    vm_graphic_setcolor(&color);
    vm_graphic_textout_to_layer(layer_hdl[0], x, y, s, wstr_len);
    vm_graphic_flush_layer(layer_hdl, 1);
}

static void draw_hello1(void) {
    VMCHAR myText[100];
    VMWSTR s;
    VMWCHAR display_string[100];
    int x;
    int y;
    int wstr_len;
    vm_graphic_color color;
    vm_ascii_to_ucs2(display_string, 100, tmp_qr_res);
    s = (VMWSTR)display_string;
    wstr_len = vm_graphic_get_string_width(s);
    x = (vm_graphic_get_screen_width() - wstr_len) / 2;
    y = (vm_graphic_get_screen_height() - vm_graphic_get_character_height()) / 2;
    color.vm_color_565 = VM_COLOR_WHITE;
    vm_graphic_setcolor(&color);
    vm_graphic_fill_rect_ex(layer_hdl[0], 0, 0, vm_graphic_get_screen_width(), vm_graphic_get_screen_height());
    color.vm_color_565 = VM_COLOR_RED;
    vm_graphic_setcolor(&color);
    vm_graphic_textout_to_layer(layer_hdl[0], x, y, s, wstr_len);
    vm_graphic_flush_layer(layer_hdl, 1);
}

void start_cam_preview(void) {
    int i = 0;
    int layer_count = 0;

    // memset(&my_frame_data, 0, sizeof(my_frame_data));
    // memset(&my_raw_frame_data, 0, sizeof(my_raw_frame_data));
    // memset(&my_capture_data, 0, sizeof(my_capture_data));

    vm_create_camera_instance(VM_CAMERA_MAIN_ID, &handle_ptr);

    vm_camera_enable_osd_layer(TRUE);
    vm_camera_set_lcd_update(TRUE);

    vm_camera_register_notify(handle_ptr, (VM_CAMERA_STATUS_NOTIFY)cam_message_callback, my_user_data);
    app_set_current_preview_size(handle_ptr);
    vm_camera_set_preview_fps(handle_ptr, 1);

    draw_hello();  // Butina bes sito nepasileidzia vm_camera_preview_start // !!!!!!!!!!!!!!!!!!!!!!!!!!

    // vm_graphic_delete_layer(layer_hdl[0]);

    layer_count = vm_graphic_get_layer_count();
    for (i = 0; i < layer_count; i++) {
        vm_graphic_delete_layer(i);
    }

    vm_graphic_flush_layer(layer_hdl, 1);

    vm_camera_preview_start(handle_ptr);
}

void cam_message_callback(vm_cam_notify_data_t *notify_data, void *user_data) {

    // vm_cam_frame_data_t frame;
    vm_cam_frame_data_t my_frame_data;
    int i;

    // char* tmp_qr_res = NULL;

    layer_hdl[0] = vm_graphic_create_layer(0, 0, vm_graphic_get_screen_width(), vm_graphic_get_screen_height(), -1);
    // vm_camera_get_default_parameter(handle_ptr, &myPara);

    handle_ptr = notify_data->handle;

    switch (notify_data->cam_message) {

        case VM_CAM_CAPTURE_ABORT:
            // break;
        case VM_CAM_PREVIEW_START_DONE:
            // break;
        case VM_CAM_PREVIEW_STOP_ABORT:
            // break;
        case VM_CAM_PREVIEW_STOP_DONE:
            // break;
        case VM_CAM_PREVIEW_START_ABORT:
            // break;
        case VM_CAM_PREVIEW_FRAME_RECEIVED:
            if (vm_camera_get_frame(handle_ptr, &my_frame_data) == VM_CAM_SUCCESS) {
                // VMUINT my_app_frame_data_size = 0;
                // VMUINT8* my_app_frame_data = NULL;
                // my_app_frame_data = my_frame_data.pixtel_data;

                VMUINT row_pixel = my_frame_data.row_pixel;
                VMUINT col_pixel = my_frame_data.col_pixel;

                VMUINT grey_app_frame_data_size = 0;
                VMUINT uyvy_app_frame_data_size = 0;
                VMUINT8 *grey_app_frame_data = NULL;
                VMUINT8 *uyvy_app_frame_data = NULL;
                // VMUINT row_pixel = frame.row_pixel;
                // VMUINT col_pixel = frame.col_pixel;

                if (my_frame_data.pixtel_format == PIXTEL_UYUV422) {
                    //vm_vibrator_once();

                    // uyvy_app_frame_data = frame.pixtel_data;

                    uyvy_app_frame_data = my_frame_data.pixtel_data;
                    uyvy_app_frame_data_size = row_pixel * col_pixel * 4;
                    grey_app_frame_data_size = row_pixel * col_pixel * 2;
                    grey_app_frame_data = vm_malloc(grey_app_frame_data_size);
                    uyvyToGrey(grey_app_frame_data, uyvy_app_frame_data, grey_app_frame_data_size);
                    tmp_qr_res = decode_qr(grey_app_frame_data, grey_app_frame_data_size, col_pixel, row_pixel);

                    if (tmp_qr_res) {
                        stop_preview();
                        //vm_audio_play_beep();
                        draw_hello1();
                        vm_vibrator_once();
                    }
                }

                // break;
            }
            draw_hello1();
            break;
        case VM_CAM_CAPTURE_DONE:
            if (vm_camera_get_capture_data(handle_ptr, &my_capture_data) == VM_CAM_SUCCESS) {
                // vm_vibrator_once();
            }
            if (vm_camera_get_capture_data(handle_ptr, &my_capture_data) == VM_CAM_ERR_UNSPPUORT_CAMERA_FEATURE) {
                // vm_vibrator_once();
            }
            if (vm_camera_get_capture_file(myCaptureFile) == VM_CAM_SUCCESS) {
                // myCaptureFile = vm_malloc(200);
                // vm_vibrator_once();
                // vm_audio_play_beep();
                //vm_create_timer(100, tt);
            }
            // vm_camera_preview_stop(handle_ptr);
            //vm_camera_preview_start(handle_ptr);
            break;
        default:
            break;
    }
}

void app_set_current_preview_size(VM_CAMERA_HANDLE camera_handle) {
    const vm_cam_size_t *ptr = NULL;

    VMUINT size = 0, i = 0;
    if (vm_camera_get_support_capture_size(camera_handle, &ptr, &size) == VM_CAM_SUCCESS) {
        vm_cam_size_t my_cam_size;
        for (i = 0; i < size; i++) {
            my_cam_size.width = (ptr + i)->width;
            my_cam_size.height = (ptr + i)->height;
            vm_camera_set_capture_size(camera_handle, &my_cam_size);
        }
    }
    if (vm_camera_get_support_preview_size(camera_handle, &ptr, &size) == VM_CAM_SUCCESS) {
        vm_cam_size_t my_cam_size;
        for (i = 0; i < size; i++) {
            my_cam_size.width = (ptr + i)->width;
            my_cam_size.height = (ptr + i)->height;
            vm_camera_set_preview_size(camera_handle, &my_cam_size);
        }
    }
}

static void scene_1(void) {
    vm_graphic_color color;
    color.vm_color_565 = VM_COLOR_GREEN;
    vm_graphic_setcolor(&color);

    if (trigeris == 0) {
        vm_graphic_line_ex(layer_hdl[0], 10, 10, 50, 50);
        vm_graphic_line_ex(layer_hdl[0], 50, 10, 10, 50);
        trigeris = 1;
    } else {
        vm_graphic_line_ex(layer_hdl[0], 230, 310, 190, 270);
        vm_graphic_line_ex(layer_hdl[0], 190, 310, 230, 270);
        trigeris = 0;
    }
    vm_graphic_flush_layer(layer_hdl, 1);
}

static void scene_2(void) {
    vm_graphic_color color;
    color.vm_color_565 = VM_COLOR_BLACK;
    vm_graphic_setcolor(&color);

    if (trigeris1 == 0) {
        vm_graphic_line_ex(layer_hdl[0], 10, 10, 50, 50);
        vm_graphic_line_ex(layer_hdl[0], 50, 10, 10, 50);
        trigeris1 = 1;
    } else {
        vm_graphic_line_ex(layer_hdl[0], 230, 310, 190, 270);
        vm_graphic_line_ex(layer_hdl[0], 190, 310, 230, 270);
        trigeris1 = 0;
    }

    vm_graphic_flush_layer(layer_hdl, 1);
}

void tt(VMINT tid) {
    VMINT sizey;
    VMCHAR file_name1[100];

    vm_get_time(&curr_time);
    sprintf(file_name1, "e:\\%d%d%d%d%d.jpg", curr_time.mon, curr_time.day, curr_time.hour, curr_time.min, curr_time.sec);
    sizey = (strlen(file_name1) + 1) * 2;
    myRenameFile = vm_realloc(myRenameFile, sizey);
    vm_ascii_to_ucs2(myRenameFile, sizey, file_name1);

    // vm_camera_preview_stop(handle_ptr);
    // vm_release_camera_instance(handle_ptr);
    // vm_graphic_draw_image_from_file(layer_hdl[0], 0, 0, myFilePath);
    // vm_graphic_draw_image_from_file(layer_hdl[0], 0, 0, myTempFile);
    // vm_graphic_flush_layer(layer_hdl, 1);
    vm_file_rename(myTempFile, myRenameFile);
    vm_delete_timer(tid);
}

unsigned char claim(int x) { return x > 255 ? 255 : (x < 0 ? 0 : x); }

void uyvyToGrey(unsigned char *dst, unsigned char *src, unsigned int numberPixels) {
    while (numberPixels > 0) {
        uint8_t y1;
        uint8_t y2;
        // VMINT y1;
        // VMINT y2;
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
    /* create a reader */
    scanner = zbar_image_scanner_create();

    /* configure the reader */
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);

    /* wrap image data */
    image = zbar_image_create();
    zbar_image_set_format(image, *(int *)"GREY");
    zbar_image_set_size(image, width, height);
    zbar_image_set_data(image, raw, width * height, zbar_image_free_data);

    /* scan the image for barcodes */
    n = zbar_scan_image(scanner, image);
    // dbg("[+] zbar_scan_image: %d\n", n);

    /* extract results */
    symbol = zbar_image_first_symbol(image);
    for (; symbol; symbol = zbar_symbol_next(symbol)) {
        /* do something useful with results */
        zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
        const char *data = zbar_symbol_get_data(symbol);
        // unsigned int data_len = zbar_symbol_get_data_length(symbol);
        // unsigned int decoded_str_len = strlen(data);
        // dbg("[+] decoded %s symbol \"%s\"\n", zbar_get_symbol_name(typ), data);

        decoded_str = vm_malloc(strlen(data) + 1);
        strcpy(decoded_str, data);
        //strcpy(myText, data);
        //sprintf(myText, "%d", strlen(data) + 1);
        break;
    }

    /* clean up */
    // memset(raw, 0, raw_sz);
    // raw is freed by zbar_image_destroy(image);
    zbar_image_destroy(image);
    zbar_image_scanner_destroy(scanner);

    return decoded_str;
}

void stop_preview() { int res = vm_camera_preview_stop(handle_ptr); }