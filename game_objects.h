#ifndef GAME_OBJECTS_H
#define GAME_OBJECTS_H

#include <gba_types.h>

// Fixed Point Math Macros (16.8 fixed point format)
#define FP_SHIFT 8
#define INT_TO_FP(x) ((x) << FP_SHIFT)
#define FP_TO_INT(x) ((x) >> FP_SHIFT)
#define FLOAT_TO_FP(x) ((int)((x) * (1 << FP_SHIFT)))

// --- Pre-calculated Fixed-Point Math for 8-Way Rotation (45-degree steps) ---
// Values are fixed-point 16.8 (256 == 1.0)
#define FP_ONE        INT_TO_FP(1)  // 256
#define FP_ZERO       0
// 0.707 * 256 approx 181. Used for sin/cos of 45, 135, 225, 315 degrees.
#define FP_COS_45_SIN_45 181

// The indices correspond to the angle in degrees: 0, 45, 90, 135, 180, 225, 270, 315
// Declared here, defined in game_logic.c
extern const int ROTATION_COS[8];
extern const int ROTATION_SIN[8];

// --- Game Object Sizes & Speed ---
#define PLAYER_SIZE 8
#define PLAYER_MAX_VELOCITY 10
#define PLAYER_FRONT_EXTEND 2
// How much to inset the rear vertices toward the center (makes the back point inward)
#define PLAYER_BACK_INSET 2
#define BULLET_SIZE 2
#define BULLET_SPEED 6
#define ASTEROID_SIZE_L 16
#define ASTEROID_SIZE_M 12
#define ASTEROID_SIZE_S 8

// --- Max Counts ---
#define MAX_BULLETS 10
#define MAX_ASTEROIDS 16

// --- Structures ---

typedef struct {
    int width;
    int height;
    int x; // Fixed-point position
    int y; // Fixed-point position
    int prevX; // Integer previous position (for clearing)
    int prevY; // Integer previous position (for clearing)
    int velocityX; // Fixed-point velocity
    int velocityY; // Fixed-point velocity
    int angle; // Angle in degrees (will be one of 0, 45, 90...315)
    int isAlive;
    int oam_index; // OAM sprite index (for asteroids/bullets), -1 if not using OAM
    int colorIdx; // Per-object color cycle index (used for bullets, etc.)
} GameObject;

typedef struct {
    GameObject obj;
    int sizeType; // ASTEROID_SIZE_L, _M, or _S
    int oam_index; // OAM sprite index
} Asteroid;

void manageAsteroidSpawning(GameObject *ship, Asteroid asteroids[]);

#endif // GAME_OBJECTS_H