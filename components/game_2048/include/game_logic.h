#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define GRID_SIZE 4

typedef enum {
    DIR_UP = 0,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT,
} direction_t;

typedef enum {
    STATE_PLAYING = 0,
    STATE_WON,
    STATE_LOST,
} game_state_t;

typedef struct {
    int grid[GRID_SIZE][GRID_SIZE];
    int score;
    int best_score;
    game_state_t state;
    bool keep_playing;  /* Continue after reaching 2048 */
    bool merged[GRID_SIZE][GRID_SIZE];  /* Track merges within a single move */
} game_t;

void game_init(game_t *game);
void game_reset(game_t *game);
bool game_move(game_t *game, direction_t dir);
void game_add_random_tile(game_t *game);
bool game_has_moves(game_t *game);
void game_check_state(game_t *game);

#endif /* GAME_LOGIC_H */
