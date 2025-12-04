#ifndef SAVE_H
#define SAVE_H

#include <gba_types.h>
#include "game_objects.h"

// Initialize/load high score from cartridge SRAM into runtime.
void loadHighScore(void);

// Persist current high score to cartridge SRAM.
void saveHighScore(void);

// Accessors for runtime high score value
int getHighScore(void);
void setHighScore(int v);

// Returns 1 if the last save operation was verified OK, 0 otherwise
int wasLastSaveOK(void);

// Game state save/load functions
void saveGameState(int score, int lives, GameObject *ship, Asteroid asteroids[], GameObject bullets[]);
int loadGameState(int *score, int *lives, GameObject *ship, Asteroid asteroids[], GameObject bullets[]);
int hasSavedGame(void);

#endif // SAVE_H
