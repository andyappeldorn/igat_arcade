#ifndef GAME_COMMON_H
#define GAME_COMMON_H

#include <stdint.h>
#include <stdbool.h>

#define SCREEN_WIDTH    480
#define SCREEN_HEIGHT   320

#define BRICK_COLS      10
#define BRICK_ROWS      5
#define BRICK_COUNT     (BRICK_COLS * BRICK_ROWS)

#define BRICK_WIDTH     40
#define BRICK_HEIGHT    14
#define BRICK_GAP_H     4
#define BRICK_GAP_V     4
#define BRICK_MARGIN_TOP    30
#define BRICK_MARGIN_LEFT   22

#define PADDLE_WIDTH    80
#define PADDLE_HEIGHT   12
#define PADDLE_Y        290

#define BALL_SIZE       10

#define GAME_TIMER_MS   20

#define MAX_LIVES       3

#define BALL_SPEED_Y    48
#define BALL_SPEED_X_INIT 16

typedef enum {
    GAME_STATE_IDLE,
    GAME_STATE_PLAYING,
    GAME_STATE_GAME_OVER,
    GAME_STATE_WIN
} game_state_t;

#endif
