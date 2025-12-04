#ifndef CHARACTERS_H
#define CHARACTERS_H

#include <gba_types.h>
#include <stdbool.h>

/* NUMBERS 0 - 10 */
extern const bool score[11][64];

/* LETTERS A - Z */
extern const bool alphabet[26][64];

/* PERIOD, EXCLAMATION POINT, COLON */
extern const bool punctuation[3][64];

/* SELECTOR ARROW, PADDLE */
extern const bool selector[2][64];

#endif // CHARACTERS_H