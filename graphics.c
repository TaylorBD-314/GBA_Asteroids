#include <gba_video.h>
#include <gba_types.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fixed_trig.h"

#include "graphics.h"
#include "game_objects.h" // For GameObject structure and lookup tables
#include "characters.h"
#include <gba_input.h> // ADDED: Needed for keysHeld() and KEY_UP

// NOTE: EWRAM_DATA is for initialized data (like the char arrays).
// EWRAM_BSS is for uninitialized data (like the video buffer).

// --- Character Data Definitions (SINGLE DEFINITION POINT) ---

/* NUMBERS 0 - 10 */
const bool score[11][64] EWRAM_DATA = {
    // 0
    {
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
    },
    // 1
    {
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 1, 0, 0,
    },
    // 2
    {
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 1, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0,
        0, 0, 1, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 0,
    },
    // 3
    {
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
    },
    // 4
    {
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 1, 0, 1, 0, 0, 0,
        0, 1, 0, 0, 1, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 1, 0, 0,
    },
    // 5
    {
        0, 1, 1, 1, 1, 1, 1, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
    },
    // 6
    {
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
    },
    // 7
    {
        0, 1, 1, 1, 1, 1, 1, 0,
        0, 0, 0, 0, 0, 1, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0,
        0, 0, 1, 0, 0, 0, 0, 0,
        0, 0, 1, 0, 0, 0, 0, 0,
    },
    // 8
    {
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
    },
    // 9
    {
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 1, 1, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    },
    // 10 (Used for space or placeholder)
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    },
};

/* LETTERS A - Z */
const bool alphabet[26][64] EWRAM_DATA = {
    // A
    {
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 1, 0, 0, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 1, 1, 1, 1, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    },
    // B
    {
        0, 1, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 1, 1, 1, 1, 0, 0,
    },
    // C
    {
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
    },
    // D
    {
        0, 1, 1, 1, 1, 0, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 1, 0, 0,
        0, 1, 1, 1, 1, 0, 0, 0,
    },
    // E
    {
        0, 1, 1, 1, 1, 1, 1, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 0,
    },
    // F
    {
        0, 1, 1, 1, 1, 1, 1, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
    },
    // G
    {
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 1, 1, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
    },
    // H
    {
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 1, 1, 1, 1, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
    },
    // I
    {
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
    },
    // J
    {
        0, 1, 1, 1, 1, 1, 1, 0,
        0, 0, 0, 0, 0, 1, 0, 0,
        0, 0, 0, 0, 0, 1, 0, 0,
        0, 0, 0, 0, 0, 1, 0, 0,
        0, 0, 0, 0, 0, 1, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0,
        0, 0, 1, 1, 1, 0, 0, 0,
    },
    // K
    {
        0, 1, 0, 0, 0, 1, 0, 0,
        0, 1, 0, 0, 1, 0, 0, 0,
        0, 1, 0, 1, 0, 0, 0, 0,
        0, 1, 1, 0, 0, 0, 0, 0,
        0, 1, 0, 1, 0, 0, 0, 0,
        0, 1, 0, 0, 1, 0, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
    },
    // L
    {
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 0,
    },
    // M
    {
        0, 1, 1, 0, 0, 1, 1, 0,
        0, 1, 0, 1, 1, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
    },
    // N
    {
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 1, 0, 0, 0, 1, 0,
        0, 1, 0, 1, 0, 0, 1, 0,
        0, 1, 0, 0, 1, 0, 1, 0,
        0, 1, 0, 0, 0, 1, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
    },
    // O
    {
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
    },
    // P
    {
        0, 1, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0,
        0, 1, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
    },
    // Q
    {
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 1, 0, 1, 0,
        0, 1, 0, 0, 0, 1, 0, 0,
        0, 0, 1, 1, 1, 1, 0, 1,
    },
    // R
    {
        0, 1, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0,
        0, 1, 1, 1, 1, 1, 0, 0,
        0, 1, 0, 0, 1, 0, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
    },
    // S
    {
        0, 0, 1, 1, 1, 1, 1, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
    },
    // T
    {
        0, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
    },
    // U
    {
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 1, 1, 1, 0, 0,
    },
    // V
    {
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 0, 0, 1, 0, 0,
        0, 0, 1, 0, 0, 1, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
    },
    // W
    {
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 1, 0, 1, 1, 0,
        0, 1, 1, 0, 1, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
    },
    // X
    {
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 0, 0, 1, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 1, 0, 0, 1, 0, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
    },
    // Y
    {
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 0,
        0, 0, 1, 0, 1, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0,
    },
    // Z
    {
        0, 1, 1, 1, 1, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 1, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0,
        0, 0, 1, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 0,
    },
};

/* PERIOD, EXCLAMATION POINT, COLON */
const bool punctuation[3][64] EWRAM_DATA = {
    // Period
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
    },
    // Exclamation Point (index 1)
    {
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
    },
    // Colon (index 2)
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    },
};

/* SELECTOR ARROW, PADDLE */
const bool selector[2][64] EWRAM_DATA = {
    // Arrow (index 0)
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0,
        0, 0, 1, 1, 0, 0, 0, 0,
        0, 1, 1, 1, 0, 0, 0, 0,
        0, 0, 1, 1, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    },
    // Paddle (index 1) - used as a marker for the cursor in this context
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 0, 0,
        0, 1, 1, 1, 1, 1, 0, 0,
        0, 1, 1, 1, 1, 1, 0, 0,
        0, 1, 1, 1, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    },
};

// --- Double Buffering Implementation ---

// 1. Define and allocate the back buffer memory in the .c file.
// FIX: Use EWRAM_BSS for the uninitialized buffer to avoid section conflict.
M3LINE back_buffer[SCREEN_HEIGHT] EWRAM_BSS; 

/**
 * Copies the completed frame from the RAM back buffer to the visible VRAM.
 * Optimized: Use memcpy for faster bulk copy (faster than pixel-by-pixel loop).
 */
void flipBuffer() {
    // This is the actual VRAM address (visible buffer)
    volatile u32 *vram_ptr = (volatile u32*)MEM_VRAM; 
    // This is the RAM buffer pointer (hidden buffer)
    u32 *back_ptr = (u32*)back_buffer;
    
    // Use u32 copies (4 bytes at a time, 2 pixels per write) for maximum efficiency
    // Total pixels: 240 * 160 = 38400; in u32: 38400 / 2 = 19200
    for (int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT / 2); i++) {
        vram_ptr[i] = back_ptr[i];
    }
}

// --- Graphics Implementation ---

// Get a pointer to the DRAWING buffer (now the RAM back_buffer)
volatile u16* videoBuffer = (u16*)back_buffer;

// Sets the color of a single pixel
void setPixel(int x, int y, u16 color) {
    // Check boundaries
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        // videoBuffer now points to the RAM buffer.
        videoBuffer[y * SCREEN_WIDTH + x] = color; 
    }
}

// Draws a line between two points (a simple DDA-like implementation)
// This is required for drawing the ship's triangle outline.
void drawLine(int x0, int y0, int x1, int y1, u16 color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int e2;
    volatile u16* vram = (u16*)back_buffer;
    
    for (;;) {
        if (x0 >= 0 && x0 < SCREEN_WIDTH && y0 >= 0 && y0 < SCREEN_HEIGHT) {
            vram[y0 * SCREEN_WIDTH + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Clears the entire screen (optimized: direct buffer writes)
void clearScreen() {
    volatile u16* vram = (u16*)back_buffer;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        vram[i] = CLR_BLACK;
    }
}

// Clears a rectangular region to black (optimized with u32 chunk writes where aligned)
void clearRegion(int x, int y, int width, int height) {
    volatile u16* vram = (u16*)back_buffer;
    for (int j = 0; j < height; j++) {
        int py = y + j;
        if (py >= 0 && py < SCREEN_HEIGHT) {
            int start_x = (x >= 0) ? x : 0;
            int end_x = (x + width <= SCREEN_WIDTH) ? (x + width) : SCREEN_WIDTH;
            
            // Write u32 chunks (2 pixels per write) where properly aligned for speed
            if ((start_x & 1) == 0 && ((end_x - start_x) & 1) == 0) {
                volatile u32* vram32 = (volatile u32*)back_buffer;
                int pairs = (end_x - start_x) / 2;
                int offset = (py * SCREEN_WIDTH + start_x) / 2;
                for (int p = 0; p < pairs; p++) {
                    vram32[offset + p] = 0; // 0 = two u16 black pixels
                }
            } else {
                // Fallback for misaligned regions
                for (int i = start_x; i < end_x; i++) {
                    vram[py * SCREEN_WIDTH + i] = CLR_BLACK;
                }
            }
        }
    }
}

// Clears the previous position of a game object
void clearPrevious(GameObject *obj) {
    // The previous size is based on the current size
    clearRegion(obj->prevX, obj->prevY, obj->width, obj->height);
}

// Draws an 8x8 character from a boolean array (optimized: skip black pixels)
void printChar(const bool charData[64], int x, int y) {
    for (int j = 0; j < CHAR_PIX_SIZE; j++) {
        for (int i = 0; i < CHAR_PIX_SIZE; i++) {
            // Only draw white pixels; skip black for performance
            if (charData[j * CHAR_PIX_SIZE + i]) {
                setPixel(x + i, y + j, CLR_WHITE);
            }
        }
    }
}

// Draw an 8x8 character in an arbitrary color
void printCharColor(const bool charData[64], int x, int y, u16 color) {
    for (int j = 0; j < CHAR_PIX_SIZE; j++) {
        for (int i = 0; i < CHAR_PIX_SIZE; i++) {
            if (charData[j * CHAR_PIX_SIZE + i]) {
                setPixel(x + i, y + j, color);
            }
        }
    }
}

// Draws a string of text in an arbitrary color
void displayTextColor(const char* text, int x, int y, u16 color) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        char c = text[i];
        if (c == '!') {
            printCharColor(punctuation[1], x + i * CHAR_PIX_SIZE, y, color);
        } else if (c == ':') {
            printCharColor(punctuation[2], x + i * CHAR_PIX_SIZE, y, color);
        } else if (c == '.') {
            printCharColor(punctuation[0], x + i * CHAR_PIX_SIZE, y, color);
        } else if (c >= '0' && c <= '9') {
            printCharColor(score[c - '0'], x + i * CHAR_PIX_SIZE, y, color);
        } else if (c == ' ') {
            printCharColor(score[10], x + i * CHAR_PIX_SIZE, y, color);
        } else if (c >= 'A' && c <= 'Z') {
            printCharColor(alphabet[c - 'A'], x + i * CHAR_PIX_SIZE, y, color);
        }
    }
}

// Draws a string of text
void displayText(const char* text, int x, int y) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        char c = text[i];
        
        // Exclamation Point
        if (c == '!') { 	
            printChar(punctuation[1], x + i * CHAR_PIX_SIZE, y);
        } else if (c == ':') { 
            printChar(punctuation[2], x + i * CHAR_PIX_SIZE, y);
        // Period
        } else if (c == '.') { 	
            printChar(punctuation[0], x + i * CHAR_PIX_SIZE, y);
        // Numbers 0-9
        } else if (c >= '0' && c <= '9') { 	
            printChar(score[c - '0'], x + i * CHAR_PIX_SIZE, y);
        // Space
        } else if (c == ' ') {
            printChar(score[10], x + i * CHAR_PIX_SIZE, y); // score[10] is the blank char
        // Letters A-Z (Capitalize all)
        } else if (c >= 'A' && c <= 'Z') {
            printChar(alphabet[c - 'A'], x + i * CHAR_PIX_SIZE, y);
        }
    }
}

// Clears the main menu area
void clearMenu() {
    clearRegion(MENU_TEXT_X - CHAR_PIX_SIZE, MENU_TEXT_Y, 
                CHAR_PIX_SIZE * 20, LINE_HEIGHT * 4);
}

// Draws the player ship (RESTORED TRIANGLE ROTATION LOGIC)
void drawPlayerShip(GameObject *ship) {
    // 1. Clear the previous drawing area
    clearPrevious(ship);
    
    // 2. Center of the ship (in screen coordinates)
    int centerX = FP_TO_INT(ship->x) + (ship->width / 2); // Corrected to int from float
    int centerY = FP_TO_INT(ship->y) + (ship->height / 2); // Corrected to int from float

    // 3. Define the vertices of the ship's triangle relative to its center (at angle 0/East)
    int offset = ship->width / 2; // = 4 for an 8x8 ship
    int front_offset = offset + PLAYER_FRONT_EXTEND; // extend the front tip for visibility
    // Vertices for an 8x8 ship (PLAYER_SIZE is 8):
    // Front point: (+front_offset, 0)
    // Left back:   (-4, +4)
    // Right back:  (-4, -4)
    // Move the rear vertices inward to form an arrow-like tail
    int back_inset = PLAYER_BACK_INSET;
    int initial_vertices[3][2] = {
        { front_offset, 0 },
        { -offset + back_inset, offset - back_inset },
        { -offset + back_inset, -offset + back_inset }
    };

    // 4. Calculate the rotation parameters (compute integer fixed-point sin/cos from angle)
    int cosA_fp = cos_fp_deg(ship->angle);
    int sinA_fp = sin_fp_deg(ship->angle);

    int rotated_vertices[3][2];

    // Rotation calculation: x' = x*cos(A) - y*sin(A), y' = x*sin(A) + y*cos(A)
    // All calculations are fixed-point multiplication, resulting in an integer coordinate after shifting.
    for (int i = 0; i < 3; i++) {
        int x_rel = initial_vertices[i][0]; // Integer relative X
        int y_rel = initial_vertices[i][1]; // Integer relative Y

        // x' (integer result) = (x_rel * cosA_fp - y_rel * sinA_fp) >> FP_SHIFT
        rotated_vertices[i][0] = centerX + 
            (((x_rel * cosA_fp) - (y_rel * sinA_fp)) >> FP_SHIFT);

        // y' (integer result) = (x_rel * sinA_fp + y_rel * cosA_fp) >> FP_SHIFT
        rotated_vertices[i][1] = centerY + 
            (((x_rel * sinA_fp) + (y_rel * cosA_fp)) >> FP_SHIFT);
    }

    u16 color = CLR_LIME;

    // 5. Draw the three lines of the triangle
    drawLine(rotated_vertices[0][0], rotated_vertices[0][1], 
             rotated_vertices[1][0], rotated_vertices[1][1], color);
    drawLine(rotated_vertices[1][0], rotated_vertices[1][1], 
             rotated_vertices[2][0], rotated_vertices[2][1], color);
    drawLine(rotated_vertices[2][0], rotated_vertices[2][1], 
             rotated_vertices[0][0], rotated_vertices[0][1], color);

    // Also draw the 'engine flare' when thrusting (KEY_UP is pressed)
    u16 keys_held = keysHeld();
    if (keys_held & KEY_UP || keys_held & KEY_B) {
        // Calculate the back center: (centerX + (-offset * cosA) >> 8, centerY + (-offset * sinA) >> 8)
        int flareX = centerX + (((-offset * cosA_fp)) >> FP_SHIFT);
        int flareY = centerY + (((-offset * sinA_fp)) >> FP_SHIFT);
        
        // Draw a small square for the flare
        setPixel(flareX, flareY, CLR_RED);
        setPixel(flareX + 1, flareY, CLR_RED);
        setPixel(flareX, flareY + 1, CLR_RED);
        setPixel(flareX + 1, flareY + 1, CLR_RED);
    }
}

// Draws a bullet as a small cyan circle
void drawBullet(GameObject *bullet) {
    int x = FP_TO_INT(bullet->x);
    int y = FP_TO_INT(bullet->y);
    
    // Color cycle for bullets: match the bouncing circles' rainbow palette
    static const u16 bullet_colors[] = { CLR_RED, CLR_YELLOW, CLR_LIME, CLR_CYAN, CLR_BLUE, CLR_MAG };
    const int BULLET_COLOR_COUNT = sizeof(bullet_colors) / sizeof(u16);
    u16 color = bullet_colors[bullet->colorIdx % BULLET_COLOR_COUNT];
    drawCircle(x, y, 2, color);
}

// Draws an asteroid as a filled circle
void drawAsteroid(Asteroid *asteroid) {
    // Get position
    GameObject *obj = &asteroid->obj;
    int x = FP_TO_INT(obj->x);
    int y = FP_TO_INT(obj->y);
    
    // Determine color and radius based on size
    u16 color;
    int radius;
    if (asteroid->sizeType == ASTEROID_SIZE_L) {
        color = RGB5(31, 0, 0);  // Red for large
        radius = 10;
    } else if (asteroid->sizeType == ASTEROID_SIZE_M) {
        color = RGB5(31, 0, 31); // Magenta for medium
        radius = 6;
    } else {
        color = RGB5(31, 31, 0); // Yellow for small
        radius = 3;
    }
    
    // Draw filled circle
    drawCircle(x, y, radius, color);
}

// Draws the score and lives
void drawScoreboard(int score, int lives, int highScore) {
    // Clear Score Area
    clearRegion(0, SCORE_Y, SCREEN_WIDTH, LINE_HEIGHT);
    clearRegion(0, SCREEN_HEIGHT - LINE_HEIGHT - SCORE_Y, SCREEN_WIDTH, LINE_HEIGHT);

    // Draw Score
    char scoreText[16];
    sprintf(scoreText, "SCORE: %d", score);
    displayText(scoreText, 10, SCORE_Y);

    // Draw High Score (right of the score area)
    char hiText[20];
    sprintf(hiText, "HI: %d", highScore);
    // place high-score in the bottom-left corner in red
    displayTextColor(hiText, 10, PLAYER_SYM_Y, CLR_RED);

    // Draw Lives
    char livesText[16];
    sprintf(livesText, "LIVES: %d", lives);
    displayText(livesText, SCREEN_WIDTH - (8 * CHAR_PIX_SIZE + 10), SCORE_Y);
}

// Draw the perimeter of a circle using an integer midpoint algorithm (optimized: skip duplicate octants)
void drawCircle(int cx, int cy, int radius, u16 color) {
    int x = radius;
    int y = 0;
    int err = 0;
    volatile u16* vram = (u16*)back_buffer;

    while (x >= y) {
        // Draw all 8 octants efficiently with bounds check
        int pts[8][2] = {
            {cx + x, cy + y}, {cx + y, cy + x}, {cx - y, cy + x}, {cx - x, cy + y},
            {cx - x, cy - y}, {cx - y, cy - x}, {cx + y, cy - x}, {cx + x, cy - y}
        };
        for (int p = 0; p < 8; p++) {
            int px = pts[p][0], py = pts[p][1];
            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                vram[py * SCREEN_WIDTH + px] = color;
            }
        }

        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err -= 2 * x + 1;
        }
    }
}

// Helper function to get asteroid visual radius
static int getAsteroidRadiusDebug(int sizeType) {
    if (sizeType == ASTEROID_SIZE_L) return 10;
    if (sizeType == ASTEROID_SIZE_M) return 6;
    if (sizeType == ASTEROID_SIZE_S) return 3;
    return 5;
}

// Draw collision circles for debugging
void drawCollisionCircles(GameObject *ship, Asteroid asteroids[], GameObject bullets[]) {
    // Draw ship collision circle (white)
    if (ship->isAlive) {
        int shipX = FP_TO_INT(ship->x) + (ship->width / 2);  // Center of ship
        int shipY = FP_TO_INT(ship->y) + (ship->height / 2);
        drawCircle(shipX, shipY, 4, CLR_WHITE);  // Collision radius = 4
    }
    
    // Draw bullet collision circles (yellow)
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].isAlive) {
            int bulletX = FP_TO_INT(bullets[i].x);
            int bulletY = FP_TO_INT(bullets[i].y);
            drawCircle(bulletX, bulletY, bullets[i].width, CLR_YELLOW);
        }
    }
    
    // Draw asteroid collision circles (green for large, cyan for medium, magenta for small)
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (asteroids[i].obj.isAlive) {
            int asteroidX = FP_TO_INT(asteroids[i].obj.x);
            int asteroidY = FP_TO_INT(asteroids[i].obj.y);
            int radius = getAsteroidRadiusDebug(asteroids[i].sizeType);
            u16 color;
            if (asteroids[i].sizeType == ASTEROID_SIZE_L) color = CLR_LIME;
            else if (asteroids[i].sizeType == ASTEROID_SIZE_M) color = CLR_CYAN;
            else color = CLR_MAG;
            drawCircle(asteroidX, asteroidY, radius, color);
        }
    }
}
