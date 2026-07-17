#include "space_invaders.h"
#include "splash_screen.h"
#include "game_common.h"
#include "../app.h"
#include "../../../../third_party/lvgl/src/lvgl.h"

#define SI_ALIEN_COLS           8
#define SI_ALIEN_ROWS           4
#define SI_ALIEN_COUNT          (SI_ALIEN_COLS * SI_ALIEN_ROWS)

#define SI_ALIEN_WIDTH          28
#define SI_ALIEN_HEIGHT         20
#define SI_ALIEN_GAP_H          6
#define SI_ALIEN_GAP_V          6
#define SI_ALIEN_MARGIN_TOP     35
#define SI_ALIEN_MARGIN_LEFT    34

#define SI_SHIP_WIDTH           40
#define SI_SHIP_HEIGHT          20
#define SI_SHIP_Y               285

#define SI_BULLET_WIDTH         4
#define SI_BULLET_HEIGHT        10
#define SI_PLAYER_BULLET_SPEED  6
#define SI_ALIEN_BULLET_SPEED   4
#define SI_MAX_ALIEN_BULLETS    3

#define SI_ALIEN_STEP_X         4
#define SI_ALIEN_DROP_Y         12
#define SI_BASE_MOVE_TICKS      30
#define SI_MIN_MOVE_TICKS       4

#define SI_FIRE_INTERVAL_TICKS  40
#define SI_FIRE_INTERVAL_MIN    15

#define SI_INVASION_Y           (SI_SHIP_Y - SI_ALIEN_HEIGHT)

static const uint16_t si_row_scores[SI_ALIEN_ROWS] = { 40, 30, 20, 10 };

static lv_color_t si_alien_colors[SI_ALIEN_ROWS];

static void si_init_colors(void)
{
    si_alien_colors[0] = lv_color_make(0xFF, 0x00, 0x40);
    si_alien_colors[1] = lv_color_make(0xFF, 0x80, 0x00);
    si_alien_colors[2] = lv_color_make(0x00, 0xFF, 0x80);
    si_alien_colors[3] = lv_color_make(0x00, 0x80, 0xFF);
}

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *aliens[SI_ALIEN_COUNT];
    lv_obj_t *ship;
    lv_obj_t *player_bullet;
    lv_obj_t *alien_bullets[SI_MAX_ALIEN_BULLETS];
    lv_obj_t *score_label;
    lv_obj_t *lives_label;
    lv_obj_t *message_label;

    lv_timer_t *game_timer;

    game_state_t state;
    uint16_t score;
    uint8_t lives;
    uint8_t aliens_alive;

    bool alien_alive[SI_ALIEN_COUNT];
    int16_t grid_x;
    int16_t grid_y;
    int8_t grid_dir;
    uint16_t move_tick_counter;
    uint16_t move_interval;

    int16_t ship_x;

    int16_t pbullet_x;
    int16_t pbullet_y;
    bool pbullet_active;

    int16_t abullet_x[SI_MAX_ALIEN_BULLETS];
    int16_t abullet_y[SI_MAX_ALIEN_BULLETS];
    bool abullet_active[SI_MAX_ALIEN_BULLETS];

    uint16_t fire_tick_counter;
    uint16_t fire_interval;

    uint16_t rng_state;
} space_invaders_state_t;

static space_invaders_state_t g_si;

static uint16_t si_rand(space_invaders_state_t *g)
{
    g->rng_state ^= g->rng_state << 7;
    g->rng_state ^= g->rng_state >> 9;
    g->rng_state ^= g->rng_state << 8;
    return g->rng_state;
}

static void si_update_score_display(space_invaders_state_t *g)
{
    lv_label_set_text_fmt(g->score_label, "Score: %d", g->score);
}

static void si_update_lives_display(space_invaders_state_t *g)
{
    lv_label_set_text_fmt(g->lives_label, "Lives: %d", g->lives);
}

static void si_show_message(space_invaders_state_t *g, const char *msg)
{
    lv_label_set_text(g->message_label, msg);
    lv_obj_clear_flag(g->message_label, LV_OBJ_FLAG_HIDDEN);
}

static void si_hide_message(space_invaders_state_t *g)
{
    lv_obj_add_flag(g->message_label, LV_OBJ_FLAG_HIDDEN);
}

static void si_recalc_speed(space_invaders_state_t *g)
{
    int16_t killed = SI_ALIEN_COUNT - g->aliens_alive;
    g->move_interval = SI_BASE_MOVE_TICKS - killed;
    if (g->move_interval < SI_MIN_MOVE_TICKS) g->move_interval = SI_MIN_MOVE_TICKS;

    g->fire_interval = SI_FIRE_INTERVAL_TICKS - killed / 2;
    if (g->fire_interval < SI_FIRE_INTERVAL_MIN) g->fire_interval = SI_FIRE_INTERVAL_MIN;
}

static void si_find_grid_bounds(space_invaders_state_t *g, int16_t *left_col, int16_t *right_col)
{
    *left_col = SI_ALIEN_COLS;
    *right_col = -1;
    for (int i = 0; i < SI_ALIEN_COUNT; i++) {
        if (!g->alien_alive[i]) continue;
        int col = i % SI_ALIEN_COLS;
        if (col < *left_col) *left_col = col;
        if (col > *right_col) *right_col = col;
    }
}

static void si_update_ship(space_invaders_state_t *g)
{
    if (touchDown) {
        int16_t target_x = (int16_t)touchX - SI_SHIP_WIDTH / 2;
        if (target_x < 0) target_x = 0;
        if (target_x > SCREEN_WIDTH - SI_SHIP_WIDTH) target_x = SCREEN_WIDTH - SI_SHIP_WIDTH;
        g->ship_x = target_x;
    }
}

static void si_update_player_bullet(space_invaders_state_t *g)
{
    if (!g->pbullet_active) return;
    g->pbullet_y -= SI_PLAYER_BULLET_SPEED;
    if (g->pbullet_y + SI_BULLET_HEIGHT < 0) {
        g->pbullet_active = false;
        lv_obj_add_flag(g->player_bullet, LV_OBJ_FLAG_HIDDEN);
    }
}

static void si_update_alien_bullets(space_invaders_state_t *g)
{
    for (int i = 0; i < SI_MAX_ALIEN_BULLETS; i++) {
        if (!g->abullet_active[i]) continue;
        g->abullet_y[i] += SI_ALIEN_BULLET_SPEED;
        if (g->abullet_y[i] > SCREEN_HEIGHT) {
            g->abullet_active[i] = false;
            lv_obj_add_flag(g->alien_bullets[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void si_move_aliens(space_invaders_state_t *g)
{
    int16_t left_col, right_col;
    si_find_grid_bounds(g, &left_col, &right_col);

    if (right_col < 0) return;

    int16_t left_edge = g->grid_x + left_col * (SI_ALIEN_WIDTH + SI_ALIEN_GAP_H);
    int16_t right_edge = g->grid_x + right_col * (SI_ALIEN_WIDTH + SI_ALIEN_GAP_H) + SI_ALIEN_WIDTH;

    bool hit_edge = false;
    if (g->grid_dir > 0 && right_edge + SI_ALIEN_STEP_X > SCREEN_WIDTH) {
        hit_edge = true;
    } else if (g->grid_dir < 0 && left_edge - SI_ALIEN_STEP_X < 0) {
        hit_edge = true;
    }

    if (hit_edge) {
        g->grid_dir = -g->grid_dir;
        g->grid_y += SI_ALIEN_DROP_Y;
    } else {
        g->grid_x += g->grid_dir * SI_ALIEN_STEP_X;
    }
}

static void si_alien_fire(space_invaders_state_t *g)
{
    int slot = -1;
    for (int i = 0; i < SI_MAX_ALIEN_BULLETS; i++) {
        if (!g->abullet_active[i]) { slot = i; break; }
    }
    if (slot < 0) return;

    int attempts = 0;
    while (attempts < SI_ALIEN_COLS * 2) {
        int col = si_rand(g) % SI_ALIEN_COLS;
        int bottom_alien = -1;
        for (int row = SI_ALIEN_ROWS - 1; row >= 0; row--) {
            int idx = row * SI_ALIEN_COLS + col;
            if (g->alien_alive[idx]) { bottom_alien = idx; break; }
        }
        if (bottom_alien >= 0) {
            int ac = bottom_alien % SI_ALIEN_COLS;
            int ar = bottom_alien / SI_ALIEN_COLS;
            g->abullet_x[slot] = g->grid_x + ac * (SI_ALIEN_WIDTH + SI_ALIEN_GAP_H) + SI_ALIEN_WIDTH / 2 - SI_BULLET_WIDTH / 2;
            g->abullet_y[slot] = g->grid_y + ar * (SI_ALIEN_HEIGHT + SI_ALIEN_GAP_V) + SI_ALIEN_HEIGHT;
            g->abullet_active[slot] = true;
            lv_obj_clear_flag(g->alien_bullets[slot], LV_OBJ_FLAG_HIDDEN);
            return;
        }
        attempts++;
    }
}

static void si_check_player_bullet_hits(space_invaders_state_t *g)
{
    if (!g->pbullet_active) return;

    int16_t bx1 = g->pbullet_x;
    int16_t by1 = g->pbullet_y;
    int16_t bx2 = bx1 + SI_BULLET_WIDTH;
    int16_t by2 = by1 + SI_BULLET_HEIGHT;

    for (int i = 0; i < SI_ALIEN_COUNT; i++) {
        if (!g->alien_alive[i]) continue;

        int col = i % SI_ALIEN_COLS;
        int row = i / SI_ALIEN_COLS;
        int16_t ax = g->grid_x + col * (SI_ALIEN_WIDTH + SI_ALIEN_GAP_H);
        int16_t ay = g->grid_y + row * (SI_ALIEN_HEIGHT + SI_ALIEN_GAP_V);

        if (bx2 >= ax && bx1 <= ax + SI_ALIEN_WIDTH &&
            by2 >= ay && by1 <= ay + SI_ALIEN_HEIGHT) {
            g->alien_alive[i] = false;
            lv_obj_add_flag(g->aliens[i], LV_OBJ_FLAG_HIDDEN);
            g->aliens_alive--;
            g->score += si_row_scores[row];
            si_update_score_display(g);
            si_recalc_speed(g);

            g->pbullet_active = false;
            lv_obj_add_flag(g->player_bullet, LV_OBJ_FLAG_HIDDEN);

            if (g->aliens_alive == 0) {
                g->state = GAME_STATE_WIN;
                si_show_message(g, "YOU WIN!\nTap to continue");
            }
            return;
        }
    }
}

static void si_check_alien_bullet_hits(space_invaders_state_t *g)
{
    int16_t sx1 = g->ship_x;
    int16_t sy1 = SI_SHIP_Y;
    int16_t sx2 = sx1 + SI_SHIP_WIDTH;
    int16_t sy2 = sy1 + SI_SHIP_HEIGHT;

    for (int i = 0; i < SI_MAX_ALIEN_BULLETS; i++) {
        if (!g->abullet_active[i]) continue;

        int16_t bx1 = g->abullet_x[i];
        int16_t by1 = g->abullet_y[i];
        int16_t bx2 = bx1 + SI_BULLET_WIDTH;
        int16_t by2 = by1 + SI_BULLET_HEIGHT;

        if (bx2 >= sx1 && bx1 <= sx2 && by2 >= sy1 && by1 <= sy2) {
            g->abullet_active[i] = false;
            lv_obj_add_flag(g->alien_bullets[i], LV_OBJ_FLAG_HIDDEN);
            g->lives--;
            si_update_lives_display(g);
            if (g->lives == 0) {
                g->state = GAME_STATE_GAME_OVER;
                si_show_message(g, "GAME OVER\nTap to continue");
            }
            return;
        }
    }
}

static void si_check_invasion(space_invaders_state_t *g)
{
    for (int i = 0; i < SI_ALIEN_COUNT; i++) {
        if (!g->alien_alive[i]) continue;
        int row = i / SI_ALIEN_COLS;
        int16_t ay = g->grid_y + row * (SI_ALIEN_HEIGHT + SI_ALIEN_GAP_V) + SI_ALIEN_HEIGHT;
        if (ay >= SI_INVASION_Y) {
            g->state = GAME_STATE_GAME_OVER;
            si_show_message(g, "INVADED!\nTap to continue");
            return;
        }
    }
}

static void si_update_positions(space_invaders_state_t *g)
{
    lv_obj_set_pos(g->ship, g->ship_x, SI_SHIP_Y);

    if (g->pbullet_active) {
        lv_obj_set_pos(g->player_bullet, g->pbullet_x, g->pbullet_y);
    }

    for (int i = 0; i < SI_MAX_ALIEN_BULLETS; i++) {
        if (g->abullet_active[i]) {
            lv_obj_set_pos(g->alien_bullets[i], g->abullet_x[i], g->abullet_y[i]);
        }
    }

    for (int i = 0; i < SI_ALIEN_COUNT; i++) {
        if (!g->alien_alive[i]) continue;
        int col = i % SI_ALIEN_COLS;
        int row = i / SI_ALIEN_COLS;
        int16_t ax = g->grid_x + col * (SI_ALIEN_WIDTH + SI_ALIEN_GAP_H);
        int16_t ay = g->grid_y + row * (SI_ALIEN_HEIGHT + SI_ALIEN_GAP_V);
        lv_obj_set_pos(g->aliens[i], ax, ay);
    }
}

static void si_game_timer_cb(lv_timer_t *timer)
{
    space_invaders_state_t *g = &g_si;
    (void)timer;

    if (g->state != GAME_STATE_PLAYING) return;

    si_update_ship(g);
    si_update_player_bullet(g);
    si_update_alien_bullets(g);

    g->move_tick_counter++;
    if (g->move_tick_counter >= g->move_interval) {
        g->move_tick_counter = 0;
        si_move_aliens(g);
    }

    g->fire_tick_counter++;
    if (g->fire_tick_counter >= g->fire_interval) {
        g->fire_tick_counter = 0;
        si_alien_fire(g);
    }

    si_check_player_bullet_hits(g);
    if (g->state != GAME_STATE_PLAYING) return;
    si_check_alien_bullet_hits(g);
    if (g->state != GAME_STATE_PLAYING) return;
    si_check_invasion(g);

    si_update_positions(g);
}

static void si_screen_click_cb(lv_event_t *e)
{
    space_invaders_state_t *g = &g_si;
    (void)e;

    switch (g->state) {
        case GAME_STATE_IDLE:
            g->state = GAME_STATE_PLAYING;
            si_hide_message(g);
            break;
        case GAME_STATE_PLAYING:
            if (!g->pbullet_active) {
                g->pbullet_x = g->ship_x + SI_SHIP_WIDTH / 2 - SI_BULLET_WIDTH / 2;
                g->pbullet_y = SI_SHIP_Y - SI_BULLET_HEIGHT;
                g->pbullet_active = true;
                lv_obj_clear_flag(g->player_bullet, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case GAME_STATE_GAME_OVER:
        case GAME_STATE_WIN:
            space_invaders_destroy();
            splash_screen_create();
            break;
    }
}

static void si_create_aliens(space_invaders_state_t *g)
{
    for (int row = 0; row < SI_ALIEN_ROWS; row++) {
        for (int col = 0; col < SI_ALIEN_COLS; col++) {
            int idx = row * SI_ALIEN_COLS + col;
            int16_t x = g->grid_x + col * (SI_ALIEN_WIDTH + SI_ALIEN_GAP_H);
            int16_t y = g->grid_y + row * (SI_ALIEN_HEIGHT + SI_ALIEN_GAP_V);

            g->aliens[idx] = lv_obj_create(g->screen);
            lv_obj_set_size(g->aliens[idx], SI_ALIEN_WIDTH, SI_ALIEN_HEIGHT);
            lv_obj_set_pos(g->aliens[idx], x, y);
            lv_obj_set_style_bg_color(g->aliens[idx], si_alien_colors[row], LV_PART_MAIN);
            lv_obj_set_style_bg_opa(g->aliens[idx], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(g->aliens[idx], 3, LV_PART_MAIN);
            lv_obj_set_style_border_width(g->aliens[idx], 0, LV_PART_MAIN);
            lv_obj_clear_flag(g->aliens[idx], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

            g->alien_alive[idx] = true;
        }
    }
    g->aliens_alive = SI_ALIEN_COUNT;
}

void space_invaders_create(void)
{
    space_invaders_state_t *g = &g_si;

    si_init_colors();

    g->screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g->screen, lv_color_make(0x00, 0x08, 0x18), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g->screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(g->screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g->screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g->screen, si_screen_click_cb, LV_EVENT_CLICKED, NULL);

    g->grid_x = SI_ALIEN_MARGIN_LEFT;
    g->grid_y = SI_ALIEN_MARGIN_TOP;
    g->grid_dir = 1;
    g->move_tick_counter = 0;
    g->move_interval = SI_BASE_MOVE_TICKS;
    g->fire_tick_counter = 0;
    g->fire_interval = SI_FIRE_INTERVAL_TICKS;
    g->rng_state = 0xACE1;

    si_create_aliens(g);

    /* Ship */
    g->ship_x = (SCREEN_WIDTH - SI_SHIP_WIDTH) / 2;
    g->ship = lv_obj_create(g->screen);
    lv_obj_set_size(g->ship, SI_SHIP_WIDTH, SI_SHIP_HEIGHT);
    lv_obj_set_pos(g->ship, g->ship_x, SI_SHIP_Y);
    lv_obj_set_style_bg_color(g->ship, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g->ship, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g->ship, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(g->ship, 0, LV_PART_MAIN);
    lv_obj_clear_flag(g->ship, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* Player bullet */
    g->player_bullet = lv_obj_create(g->screen);
    lv_obj_set_size(g->player_bullet, SI_BULLET_WIDTH, SI_BULLET_HEIGHT);
    lv_obj_set_style_bg_color(g->player_bullet, lv_color_make(0xC0, 0xFF, 0x00), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g->player_bullet, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g->player_bullet, 1, LV_PART_MAIN);
    lv_obj_set_style_border_width(g->player_bullet, 0, LV_PART_MAIN);
    lv_obj_clear_flag(g->player_bullet, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g->player_bullet, LV_OBJ_FLAG_HIDDEN);
    g->pbullet_active = false;

    /* Alien bullets */
    for (int i = 0; i < SI_MAX_ALIEN_BULLETS; i++) {
        g->alien_bullets[i] = lv_obj_create(g->screen);
        lv_obj_set_size(g->alien_bullets[i], SI_BULLET_WIDTH, SI_BULLET_HEIGHT);
        lv_obj_set_style_bg_color(g->alien_bullets[i], lv_color_make(0xFF, 0x40, 0x40), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g->alien_bullets[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(g->alien_bullets[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_width(g->alien_bullets[i], 0, LV_PART_MAIN);
        lv_obj_clear_flag(g->alien_bullets[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(g->alien_bullets[i], LV_OBJ_FLAG_HIDDEN);
        g->abullet_active[i] = false;
    }

    /* Score label */
    g->score_label = lv_label_create(g->screen);
    lv_obj_set_style_text_color(g->score_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g->score_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(g->score_label, 5, 5);
    g->score = 0;
    si_update_score_display(g);

    /* Lives label */
    g->lives_label = lv_label_create(g->screen);
    lv_obj_set_style_text_color(g->lives_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g->lives_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(g->lives_label, LV_ALIGN_TOP_RIGHT, -5, 5);
    g->lives = MAX_LIVES;
    si_update_lives_display(g);

    /* Message label */
    g->message_label = lv_label_create(g->screen);
    lv_obj_set_style_text_color(g->message_label, lv_color_make(0xFF, 0xFF, 0x00), LV_PART_MAIN);
    lv_obj_set_style_text_font(g->message_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(g->message_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g->message_label, LV_ALIGN_CENTER, 0, 40);

    /* Initial state */
    g->state = GAME_STATE_IDLE;
    si_show_message(g, "Tap to start");

    /* Game loop timer */
    g->game_timer = lv_timer_create(si_game_timer_cb, GAME_TIMER_MS, NULL);

    lv_scr_load(g->screen);
}

void space_invaders_destroy(void)
{
    space_invaders_state_t *g = &g_si;

    if (g->game_timer) {
        lv_timer_del(g->game_timer);
        g->game_timer = NULL;
    }

    if (g->screen) {
        lv_obj_del(g->screen);
        g->screen = NULL;
    }
}
