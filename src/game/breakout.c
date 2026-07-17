#include "breakout.h"
#include "splash_screen.h"
#include "game_common.h"
#include "../app.h"
#include "../../../../third_party/lvgl/src/lvgl.h"

static lv_color_t brick_colors[BRICK_ROWS];

static void init_brick_colors(void)
{
    brick_colors[0] = lv_color_make(0xFF, 0x00, 0x00);
    brick_colors[1] = lv_color_make(0xFF, 0xA5, 0x00);
    brick_colors[2] = lv_color_make(0xFF, 0xFF, 0x00);
    brick_colors[3] = lv_color_make(0x00, 0xFF, 0x00);
    brick_colors[4] = lv_color_make(0x00, 0xBF, 0xFF);
}

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *paddle;
    lv_obj_t *ball;
    lv_obj_t *bricks[BRICK_COUNT];
    lv_obj_t *score_label;
    lv_obj_t *lives_label;
    lv_obj_t *message_label;

    lv_timer_t *game_timer;

    int16_t ball_x;
    int16_t ball_y;
    int16_t ball_vx;
    int16_t ball_vy;
    int16_t ball_x_frac;
    int16_t ball_y_frac;

    int16_t paddle_x;

    game_state_t state;
    uint8_t lives;
    uint16_t score;
    uint8_t bricks_remaining;
    bool brick_alive[BRICK_COUNT];
} breakout_state_t;

static breakout_state_t g_game;

static void reset_ball_to_paddle(breakout_state_t *g)
{
    g->ball_x = g->paddle_x + PADDLE_WIDTH / 2;
    g->ball_y = PADDLE_Y - BALL_SIZE / 2 - 1;
    g->ball_vx = 0;
    g->ball_vy = 0;
    g->ball_x_frac = 0;
    g->ball_y_frac = 0;
    lv_obj_set_pos(g->ball, g->ball_x - BALL_SIZE / 2, g->ball_y - BALL_SIZE / 2);
}

static void update_score_display(breakout_state_t *g)
{
    lv_label_set_text_fmt(g->score_label, "Score: %d", g->score);
}

static void update_lives_display(breakout_state_t *g)
{
    lv_label_set_text_fmt(g->lives_label, "Lives: %d", g->lives);
}

static void show_message(breakout_state_t *g, const char *msg)
{
    lv_label_set_text(g->message_label, msg);
    lv_obj_clear_flag(g->message_label, LV_OBJ_FLAG_HIDDEN);
}

static void hide_message(breakout_state_t *g)
{
    lv_obj_add_flag(g->message_label, LV_OBJ_FLAG_HIDDEN);
}

static void update_paddle(breakout_state_t *g)
{
    if (touchDown) {
        int16_t target_x = (int16_t)touchX - PADDLE_WIDTH / 2;
        if (target_x < 0) target_x = 0;
        if (target_x > SCREEN_WIDTH - PADDLE_WIDTH) target_x = SCREEN_WIDTH - PADDLE_WIDTH;
        g->paddle_x = target_x;
    }
}

static void check_wall_collision(breakout_state_t *g)
{
    if (g->ball_x - BALL_SIZE / 2 <= 0) {
        g->ball_x = BALL_SIZE / 2;
        g->ball_vx = -g->ball_vx;
    }
    if (g->ball_x + BALL_SIZE / 2 >= SCREEN_WIDTH) {
        g->ball_x = SCREEN_WIDTH - BALL_SIZE / 2;
        g->ball_vx = -g->ball_vx;
    }
    if (g->ball_y - BALL_SIZE / 2 <= 0) {
        g->ball_y = BALL_SIZE / 2;
        g->ball_vy = -g->ball_vy;
    }
}

static void check_paddle_collision(breakout_state_t *g)
{
    if (g->ball_vy <= 0) return;

    int16_t ball_bottom = g->ball_y + BALL_SIZE / 2;
    int16_t ball_left = g->ball_x - BALL_SIZE / 2;
    int16_t ball_right = g->ball_x + BALL_SIZE / 2;

    if (ball_bottom >= PADDLE_Y &&
        ball_bottom <= PADDLE_Y + PADDLE_HEIGHT &&
        ball_right >= g->paddle_x &&
        ball_left <= g->paddle_x + PADDLE_WIDTH) {

        g->ball_vy = -g->ball_vy;
        g->ball_y = PADDLE_Y - BALL_SIZE / 2;

        int16_t hit_pos = g->ball_x - (g->paddle_x + PADDLE_WIDTH / 2);
        g->ball_vx = (hit_pos * 48) / (PADDLE_WIDTH / 2);
    }
}

static void check_brick_collision(breakout_state_t *g)
{
    int16_t ball_left = g->ball_x - BALL_SIZE / 2;
    int16_t ball_right = g->ball_x + BALL_SIZE / 2;
    int16_t ball_top = g->ball_y - BALL_SIZE / 2;
    int16_t ball_bottom = g->ball_y + BALL_SIZE / 2;

    for (int i = 0; i < BRICK_COUNT; i++) {
        if (!g->brick_alive[i]) continue;

        int col = i % BRICK_COLS;
        int row = i / BRICK_COLS;
        int16_t bx = BRICK_MARGIN_LEFT + col * (BRICK_WIDTH + BRICK_GAP_H);
        int16_t by = BRICK_MARGIN_TOP + row * (BRICK_HEIGHT + BRICK_GAP_V);

        if (ball_right >= bx && ball_left <= bx + BRICK_WIDTH &&
            ball_bottom >= by && ball_top <= by + BRICK_HEIGHT) {

            g->brick_alive[i] = false;
            lv_obj_add_flag(g->bricks[i], LV_OBJ_FLAG_HIDDEN);
            g->bricks_remaining--;
            g->score += 10;
            update_score_display(g);

            int16_t overlap_left = ball_right - bx;
            int16_t overlap_right = (bx + BRICK_WIDTH) - ball_left;
            int16_t overlap_top = ball_bottom - by;
            int16_t overlap_bottom = (by + BRICK_HEIGHT) - ball_top;
            int16_t min_overlap_x = (overlap_left < overlap_right) ? overlap_left : overlap_right;
            int16_t min_overlap_y = (overlap_top < overlap_bottom) ? overlap_top : overlap_bottom;

            if (min_overlap_x < min_overlap_y) {
                g->ball_vx = -g->ball_vx;
            } else {
                g->ball_vy = -g->ball_vy;
            }

            break;
        }
    }
}

static void check_ball_lost(breakout_state_t *g)
{
    if (g->ball_y - BALL_SIZE / 2 > SCREEN_HEIGHT) {
        g->lives--;
        update_lives_display(g);
        if (g->lives == 0) {
            g->state = GAME_STATE_GAME_OVER;
            show_message(g, "GAME OVER\nTap to continue");
        } else {
            g->state = GAME_STATE_IDLE;
            reset_ball_to_paddle(g);
            show_message(g, "Tap to launch");
        }
    }
}

static void game_timer_cb(lv_timer_t *timer)
{
    breakout_state_t *g = &g_game;
    (void)timer;

    if (g->state != GAME_STATE_PLAYING) return;

    update_paddle(g);

    g->ball_x_frac += g->ball_vx;
    g->ball_y_frac += g->ball_vy;
    int16_t dx = g->ball_x_frac / 16;
    int16_t dy = g->ball_y_frac / 16;
    g->ball_x_frac -= dx * 16;
    g->ball_y_frac -= dy * 16;
    g->ball_x += dx;
    g->ball_y += dy;

    check_wall_collision(g);
    check_paddle_collision(g);
    check_brick_collision(g);
    check_ball_lost(g);

    lv_obj_set_pos(g->ball, g->ball_x - BALL_SIZE / 2, g->ball_y - BALL_SIZE / 2);
    lv_obj_set_pos(g->paddle, g->paddle_x, PADDLE_Y);

    if (g->bricks_remaining == 0) {
        g->state = GAME_STATE_WIN;
        show_message(g, "YOU WIN!\nTap to continue");
    }
}

static void launch_ball(breakout_state_t *g)
{
    g->ball_vx = BALL_SPEED_X_INIT;
    g->ball_vy = -BALL_SPEED_Y;
    g->state = GAME_STATE_PLAYING;
    hide_message(g);
}

static void game_screen_click_cb(lv_event_t *e)
{
    breakout_state_t *g = &g_game;
    (void)e;

    switch (g->state) {
        case GAME_STATE_IDLE:
            update_paddle(g);
            reset_ball_to_paddle(g);
            launch_ball(g);
            break;
        case GAME_STATE_GAME_OVER:
        case GAME_STATE_WIN:
            breakout_destroy();
            splash_screen_create();
            break;
        default:
            break;
    }
}

static void create_bricks(breakout_state_t *g)
{
    for (int row = 0; row < BRICK_ROWS; row++) {
        for (int col = 0; col < BRICK_COLS; col++) {
            int idx = row * BRICK_COLS + col;
            int16_t x = BRICK_MARGIN_LEFT + col * (BRICK_WIDTH + BRICK_GAP_H);
            int16_t y = BRICK_MARGIN_TOP + row * (BRICK_HEIGHT + BRICK_GAP_V);

            g->bricks[idx] = lv_obj_create(g->screen);
            lv_obj_set_size(g->bricks[idx], BRICK_WIDTH, BRICK_HEIGHT);
            lv_obj_set_pos(g->bricks[idx], x, y);
            lv_obj_set_style_bg_color(g->bricks[idx], brick_colors[row], LV_PART_MAIN);
            lv_obj_set_style_bg_opa(g->bricks[idx], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(g->bricks[idx], 2, LV_PART_MAIN);
            lv_obj_set_style_border_width(g->bricks[idx], 0, LV_PART_MAIN);
            lv_obj_clear_flag(g->bricks[idx], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

            g->brick_alive[idx] = true;
        }
    }
    g->bricks_remaining = BRICK_COUNT;
}

void breakout_create(void)
{
    breakout_state_t *g = &g_game;

    init_brick_colors();

    g->screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g->screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g->screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(g->screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g->screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g->screen, game_screen_click_cb, LV_EVENT_CLICKED, NULL);

    create_bricks(g);

    /* Paddle */
    g->paddle_x = (SCREEN_WIDTH - PADDLE_WIDTH) / 2;
    g->paddle = lv_obj_create(g->screen);
    lv_obj_set_size(g->paddle, PADDLE_WIDTH, PADDLE_HEIGHT);
    lv_obj_set_pos(g->paddle, g->paddle_x, PADDLE_Y);
    lv_obj_set_style_bg_color(g->paddle, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g->paddle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g->paddle, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(g->paddle, 0, LV_PART_MAIN);
    lv_obj_clear_flag(g->paddle, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* Ball */
    g->ball = lv_obj_create(g->screen);
    lv_obj_set_size(g->ball, BALL_SIZE, BALL_SIZE);
    lv_obj_set_style_bg_color(g->ball, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g->ball, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g->ball, BALL_SIZE / 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(g->ball, 0, LV_PART_MAIN);
    lv_obj_clear_flag(g->ball, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* Score label */
    g->score_label = lv_label_create(g->screen);
    lv_obj_set_style_text_color(g->score_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g->score_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_pos(g->score_label, 5, 5);
    g->score = 0;
    update_score_display(g);

    /* Lives label */
    g->lives_label = lv_label_create(g->screen);
    lv_obj_set_style_text_color(g->lives_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(g->lives_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(g->lives_label, LV_ALIGN_TOP_RIGHT, -5, 5);
    g->lives = MAX_LIVES;
    update_lives_display(g);

    /* Message label */
    g->message_label = lv_label_create(g->screen);
    lv_obj_set_style_text_color(g->message_label, lv_color_make(0xFF, 0xFF, 0x00), LV_PART_MAIN);
    lv_obj_set_style_text_font(g->message_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(g->message_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g->message_label, LV_ALIGN_CENTER, 0, 40);

    /* Initial state */
    g->state = GAME_STATE_IDLE;
    reset_ball_to_paddle(g);
    show_message(g, "Tap to launch");

    /* Game loop timer */
    g->game_timer = lv_timer_create(game_timer_cb, GAME_TIMER_MS, NULL);

    lv_scr_load(g->screen);
}

void breakout_destroy(void)
{
    breakout_state_t *g = &g_game;

    if (g->game_timer) {
        lv_timer_del(g->game_timer);
        g->game_timer = NULL;
    }

    if (g->screen) {
        lv_obj_del(g->screen);
        g->screen = NULL;
    }
}
