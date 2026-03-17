#include "game_logic.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef DESKTOP_BUILD
#include "esp_random.h"
#endif

static bool seeded = false;

static int random_int(int max)
{
#ifdef DESKTOP_BUILD
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = true;
    }
    return rand() % max;
#else
    return (int)(esp_random() % (uint32_t)max);
#endif
}

void game_init(game_t *game)
{
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = true;
    }
    game->best_score = 0;
    game_reset(game);
}

void game_reset(game_t *game)
{
    memset(game->grid, 0, sizeof(game->grid));
    memset(game->merged, 0, sizeof(game->merged));
    game->score = 0;
    game->state = STATE_PLAYING;
    game->keep_playing = false;
    game_add_random_tile(game);
    game_add_random_tile(game);
}

void game_add_random_tile(game_t *game)
{
    /* Count empty cells */
    int empty[GRID_SIZE * GRID_SIZE][2];
    int count = 0;

    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            if (game->grid[r][c] == 0) {
                empty[count][0] = r;
                empty[count][1] = c;
                count++;
            }
        }
    }

    if (count == 0) return;

    int idx = random_int(count);
    int r = empty[idx][0];
    int c = empty[idx][1];

    /* 90% chance of 2, 10% chance of 4 */
    game->grid[r][c] = (random_int(10) < 9) ? 2 : 4;
}

/*
 * Compress a single row/column toward index 0 (remove gaps).
 * Returns true if anything moved.
 */
static bool compress(int line[GRID_SIZE])
{
    bool moved = false;
    int pos = 0;
    for (int i = 0; i < GRID_SIZE; i++) {
        if (line[i] != 0) {
            if (i != pos) {
                line[pos] = line[i];
                line[i] = 0;
                moved = true;
            }
            pos++;
        }
    }
    return moved;
}

/*
 * Merge adjacent equal tiles in a compressed row/column.
 * Each tile can only merge once per move.
 * Returns points scored.
 */
static int merge(int line[GRID_SIZE], bool merged_flags[GRID_SIZE])
{
    int points = 0;
    for (int i = 0; i < GRID_SIZE - 1; i++) {
        if (line[i] != 0 && line[i] == line[i + 1]) {
            line[i] *= 2;
            line[i + 1] = 0;
            merged_flags[i] = true;
            points += line[i];
            i++; /* Skip next — already consumed */
        }
    }
    return points;
}

/*
 * Extract a row or column from the grid into a linear array.
 * For UP/DOWN we extract columns; for LEFT/RIGHT we extract rows.
 * For DOWN/RIGHT the line is reversed so we always merge toward index 0.
 */
static void extract_line(game_t *game, direction_t dir, int idx, int line[GRID_SIZE])
{
    for (int i = 0; i < GRID_SIZE; i++) {
        switch (dir) {
        case DIR_LEFT:  line[i] = game->grid[idx][i]; break;
        case DIR_RIGHT: line[i] = game->grid[idx][GRID_SIZE - 1 - i]; break;
        case DIR_UP:    line[i] = game->grid[i][idx]; break;
        case DIR_DOWN:  line[i] = game->grid[GRID_SIZE - 1 - i][idx]; break;
        }
    }
}

static void insert_line(game_t *game, direction_t dir, int idx, int line[GRID_SIZE])
{
    for (int i = 0; i < GRID_SIZE; i++) {
        switch (dir) {
        case DIR_LEFT:  game->grid[idx][i] = line[i]; break;
        case DIR_RIGHT: game->grid[idx][GRID_SIZE - 1 - i] = line[i]; break;
        case DIR_UP:    game->grid[i][idx] = line[i]; break;
        case DIR_DOWN:  game->grid[GRID_SIZE - 1 - i][idx] = line[i]; break;
        }
    }
}

static void set_merged_flag(game_t *game, direction_t dir, int line_idx, int pos)
{
    switch (dir) {
    case DIR_LEFT:  game->merged[line_idx][pos] = true; break;
    case DIR_RIGHT: game->merged[line_idx][GRID_SIZE - 1 - pos] = true; break;
    case DIR_UP:    game->merged[pos][line_idx] = true; break;
    case DIR_DOWN:  game->merged[GRID_SIZE - 1 - pos][line_idx] = true; break;
    }
}

bool game_move(game_t *game, direction_t dir)
{
    if (game->state == STATE_LOST) return false;
    if (game->state == STATE_WON && !game->keep_playing) return false;

    bool moved = false;
    memset(game->merged, 0, sizeof(game->merged));

    for (int idx = 0; idx < GRID_SIZE; idx++) {
        int line[GRID_SIZE];
        bool line_merged[GRID_SIZE] = {false};

        extract_line(game, dir, idx, line);

        bool m1 = compress(line);
        int points = merge(line, line_merged);
        bool m2 = compress(line);

        if (m1 || m2 || points > 0) {
            moved = true;
        }

        game->score += points;

        insert_line(game, dir, idx, line);

        /* Record which cells merged for animation */
        for (int i = 0; i < GRID_SIZE; i++) {
            if (line_merged[i]) {
                set_merged_flag(game, dir, idx, i);
            }
        }
    }

    if (game->score > game->best_score) {
        game->best_score = game->score;
    }

    return moved;
}

bool game_has_moves(game_t *game)
{
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            if (game->grid[r][c] == 0) return true;
            if (c < GRID_SIZE - 1 && game->grid[r][c] == game->grid[r][c + 1]) return true;
            if (r < GRID_SIZE - 1 && game->grid[r][c] == game->grid[r + 1][c]) return true;
        }
    }
    return false;
}

void game_check_state(game_t *game)
{
    if (game->state == STATE_LOST) return;

    /* Check for 2048 tile */
    if (!game->keep_playing) {
        for (int r = 0; r < GRID_SIZE; r++) {
            for (int c = 0; c < GRID_SIZE; c++) {
                if (game->grid[r][c] == 2048) {
                    game->state = STATE_WON;
                    return;
                }
            }
        }
    }

    /* Check for available moves */
    if (!game_has_moves(game)) {
        game->state = STATE_LOST;
        return;
    }

    game->state = STATE_PLAYING;
}
