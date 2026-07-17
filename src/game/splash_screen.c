#include "splash_screen.h"
#include "breakout.h"
#include "space_invaders.h"
#include "touch_report.h"
#include "game_common.h"
#include "../../../../third_party/lvgl/src/lvgl.h"

static lv_obj_t *splash_scr;

static void breakout_btn_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_t *old_scr = splash_scr;
    splash_scr = NULL;
    breakout_create();
    lv_obj_del(old_scr);
}

static void invaders_btn_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_t *old_scr = splash_scr;
    splash_scr = NULL;
    space_invaders_create();
    lv_obj_del(old_scr);
}

static void settings_btn_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_t *old_scr = splash_scr;
    splash_scr = NULL;
    touch_report_create();
    lv_obj_del(old_scr);
}

void splash_screen_create(void)
{
    splash_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(splash_scr, lv_color_make(0x00, 0x10, 0x30), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(splash_scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(splash_scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(splash_scr);
    lv_label_set_text(title, "ARCADE");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -60);

    /* Breakout button */
    lv_obj_t *btn1 = lv_btn_create(splash_scr);
    lv_obj_set_size(btn1, 160, 40);
    lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(btn1, lv_color_make(0x00, 0x60, 0xA0), LV_PART_MAIN);
    lv_obj_add_event_cb(btn1, breakout_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl1 = lv_label_create(btn1);
    lv_label_set_text(lbl1, "Breakout");
    lv_obj_set_style_text_font(lbl1, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(lbl1);

    /* Space Invaders button */
    lv_obj_t *btn2 = lv_btn_create(splash_scr);
    lv_obj_set_size(btn2, 160, 40);
    lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_bg_color(btn2, lv_color_make(0x00, 0x60, 0xA0), LV_PART_MAIN);
    lv_obj_add_event_cb(btn2, invaders_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl2 = lv_label_create(btn2);
    lv_label_set_text(lbl2, "Space Invaders");
    lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(lbl2);

    /* Settings icon (top-right) */
    lv_obj_t *settings_btn = lv_btn_create(splash_scr);
    lv_obj_set_size(settings_btn, 36, 36);
    lv_obj_align(settings_btn, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_set_style_bg_color(settings_btn, lv_color_make(0x30, 0x30, 0x50), LV_PART_MAIN);
    lv_obj_set_style_radius(settings_btn, 18, LV_PART_MAIN);
    lv_obj_add_event_cb(settings_btn, settings_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *gear_lbl = lv_label_create(settings_btn);
    lv_label_set_text(gear_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_center(gear_lbl);

    lv_scr_load(splash_scr);
}
