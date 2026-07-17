#include "touch_report.h"
#include "splash_screen.h"
#include "game_common.h"
#include "../app.h"
#include "../../../../third_party/lvgl/src/lvgl.h"

#define TR_CROSSHAIR_SIZE   20
#define TR_LINE_THICKNESS   2
#define TR_DOT_SIZE         6
#define TR_UPDATE_MS        20

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *coord_label;
    lv_obj_t *h_line;
    lv_obj_t *v_line;
    lv_obj_t *dot;
    lv_obj_t *back_label;
    lv_timer_t *timer;
} touch_report_state_t;

static touch_report_state_t g_tr;

static void tr_timer_cb(lv_timer_t *timer)
{
    touch_report_state_t *t = &g_tr;
    (void)timer;

    int16_t x = (int16_t)touchX;
    int16_t y = (int16_t)touchY;

    lv_label_set_text_fmt(t->coord_label, "X: %d  Y: %d  %s",
                          x, y, touchDown ? "DOWN" : "UP");

    if (touchDown) {
        lv_obj_set_pos(t->h_line, x - TR_CROSSHAIR_SIZE, y - TR_LINE_THICKNESS / 2);
        lv_obj_set_pos(t->v_line, x - TR_LINE_THICKNESS / 2, y - TR_CROSSHAIR_SIZE);
        lv_obj_set_pos(t->dot, x - TR_DOT_SIZE / 2, y - TR_DOT_SIZE / 2);
        lv_obj_clear_flag(t->h_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(t->v_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(t->dot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(t->h_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(t->v_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(t->dot, LV_OBJ_FLAG_HIDDEN);
    }
}

static void back_btn_cb(lv_event_t *e)
{
    (void)e;
    touch_report_destroy();
    splash_screen_create();
}

void touch_report_create(void)
{
    touch_report_state_t *t = &g_tr;

    t->screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(t->screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(t->screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(t->screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Coordinate readout at top center */
    t->coord_label = lv_label_create(t->screen);
    lv_obj_set_style_text_color(t->coord_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(t->coord_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(t->coord_label, LV_ALIGN_TOP_MID, 0, 5);
    lv_label_set_text(t->coord_label, "X: 0  Y: 0  UP");

    /* Back button (top-left) */
    lv_obj_t *back_btn = lv_btn_create(t->screen);
    lv_obj_set_size(back_btn, 60, 28);
    lv_obj_set_pos(back_btn, 5, 5);
    lv_obj_set_style_bg_color(back_btn, lv_color_make(0x40, 0x40, 0x40), LV_PART_MAIN);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    t->back_label = lv_label_create(back_btn);
    lv_label_set_text(t->back_label, "Back");
    lv_obj_set_style_text_font(t->back_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_center(t->back_label);

    /* Crosshair horizontal line */
    t->h_line = lv_obj_create(t->screen);
    lv_obj_set_size(t->h_line, TR_CROSSHAIR_SIZE * 2, TR_LINE_THICKNESS);
    lv_obj_set_style_bg_color(t->h_line, lv_color_make(0x00, 0xFF, 0x00), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(t->h_line, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(t->h_line, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(t->h_line, 0, LV_PART_MAIN);
    lv_obj_clear_flag(t->h_line, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(t->h_line, LV_OBJ_FLAG_HIDDEN);

    /* Crosshair vertical line */
    t->v_line = lv_obj_create(t->screen);
    lv_obj_set_size(t->v_line, TR_LINE_THICKNESS, TR_CROSSHAIR_SIZE * 2);
    lv_obj_set_style_bg_color(t->v_line, lv_color_make(0x00, 0xFF, 0x00), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(t->v_line, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(t->v_line, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(t->v_line, 0, LV_PART_MAIN);
    lv_obj_clear_flag(t->v_line, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(t->v_line, LV_OBJ_FLAG_HIDDEN);

    /* Center dot */
    t->dot = lv_obj_create(t->screen);
    lv_obj_set_size(t->dot, TR_DOT_SIZE, TR_DOT_SIZE);
    lv_obj_set_style_bg_color(t->dot, lv_color_make(0xFF, 0xFF, 0x00), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(t->dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(t->dot, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(t->dot, TR_DOT_SIZE / 2, LV_PART_MAIN);
    lv_obj_clear_flag(t->dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(t->dot, LV_OBJ_FLAG_HIDDEN);

    /* Update timer */
    t->timer = lv_timer_create(tr_timer_cb, TR_UPDATE_MS, NULL);

    lv_scr_load(t->screen);
}

void touch_report_destroy(void)
{
    touch_report_state_t *t = &g_tr;

    if (t->timer) {
        lv_timer_del(t->timer);
        t->timer = NULL;
    }

    if (t->screen) {
        lv_obj_del(t->screen);
        t->screen = NULL;
    }
}
