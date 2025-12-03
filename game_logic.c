#include <gba_video.h>
#include <gba_input.h>
#include <gba_types.h>
#include <stdlib.h>
#include "fixed_trig.h"
#include "game_objects.h"
#include "graphics.h"
#include "sound.h"

// Constants
// #define RAD_PER_DEG (3.14159f / 180.0f) // REMOVED
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160

// Rotation speed in degrees per frame when holding left/right
#define ROTATION_SPEED_DEG 18

// Fixed-point acceleration factor: FLOAT_TO_FP(0.1f) is approx 26
#define ACCEL_FACTOR_FP 26 

// --- DYNAMIC ASTEROID SPAWN LOGIC VARIABLES (RESTORED TO ORIGINAL VALUES) ---
// Initial spawn interval: 60 frames (assuming 60 FPS)
#define INITIAL_SPAWN_INTERVAL 60
// Interval to decrease the spawn time
#define DECREASE_INTERVAL (INITIAL_SPAWN_INTERVAL * 2)
// The amount the spawn interval decreases by (e.g., 5 frames)
#define DECREASE_AMOUNT 5
// Minimum spawn interval to prevent overwhelming the player (e.g., 20 frames)
#define MIN_SPAWN_INTERVAL 20

// Static variables to manage spawn timing
static int s_spawnTimer = INITIAL_SPAWN_INTERVAL; // Countdown to next spawn
static int s_currentSpawnInterval = INITIAL_SPAWN_INTERVAL; // Current delay between spawns
static int s_decreaseTimer = DECREASE_INTERVAL; // Countdown until spawn interval decreases

// --- FIXED-POINT LOOKUP TABLES FOR 8-WAY ROTATION ---
    // The index corresponds to the angle (angle / 45) for 0, 45, 90, 135, 180, 225, 270, 315 degrees.
// Values are fixed-point 16.8 (256 == 1.0)

// Cosine (X-component)
const int ROTATION_COS[8] = {
    FP_ONE,            // 0 deg (1.0)
    FP_COS_45_SIN_45,  // 45 deg (0.707)
    FP_ZERO,           // 90 deg (0.0)
    -FP_COS_45_SIN_45, // 135 deg (-0.707)
    -FP_ONE,           // 180 deg (-1.0)
    -FP_COS_45_SIN_45, // 225 deg (-0.707)
    FP_ZERO,           // 270 deg (0.0)
    FP_COS_45_SIN_45   // 315 deg (0.707)
};

// Sine (Y-component)
const int ROTATION_SIN[8] = {
    FP_ZERO,           // 0 deg (0.0)
    FP_COS_45_SIN_45,  // 45 deg (0.707)
    FP_ONE,            // 90 deg (1.0)
    FP_COS_45_SIN_45,  // 135 deg (0.707)
    FP_ZERO,           // 180 deg (0.0)
    -FP_COS_45_SIN_45, // 225 deg (-0.707)
    -FP_ONE,           // 270 deg (-1.0)
    -FP_COS_45_SIN_45  // 315 deg (-0.707)
};

// --- Game Logic Implementations (Externally declared in main.c) ---

// Helper: Get the visual collision radius for an asteroid
static int getAsteroidRadius(int sizeType) {
    if (sizeType == ASTEROID_SIZE_L) return 10;  // Large asteroid drawn with radius 10
    if (sizeType == ASTEROID_SIZE_M) return 6;   // Medium asteroid drawn with radius 6
    if (sizeType == ASTEROID_SIZE_S) return 3;   // Small asteroid drawn with radius 3
    return 5; // Fallback
}

// Helper: Distance squared from point (px, py) to line segment (x1,y1)-(x2,y2)
static int distanceToSegmentSq(int px, int py, int x1, int y1, int x2, int y2) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    int lenSq = dx * dx + dy * dy;
    
    if (lenSq == 0) {
        // Degenerate segment, just point distance
        int dpx = px - x1;
        int dpy = py - y1;
        return dpx * dpx + dpy * dpy;
    }
    
    // Parameter t for closest point on segment
    int dot = (px - x1) * dx + (py - y1) * dy;
    int t = (dot * 256) / lenSq; // scaled by 256 for precision
    
    if (t < 0) t = 0;
    if (t > 256) t = 256;
    
    // Closest point on segment
    int closestX = x1 + ((dx * t) >> 8);
    int closestY = y1 + ((dy * t) >> 8);
    
    int dpx = px - closestX;
    int dpy = py - closestY;
    return dpx * dpx + dpy * dpy;
}

// Helper: Check if point is inside or near ship triangle
static bool pointNearShipTriangle(int px, int py, GameObject *ship, int threshold) {
    // Compute ship vertices (same as drawPlayerShip)
    int centerX = FP_TO_INT(ship->x) + (ship->width / 2);
    int centerY = FP_TO_INT(ship->y) + (ship->height / 2);
    
    int offset = ship->width / 2;
    int front_offset = offset + 2; // PLAYER_FRONT_EXTEND
    int back_inset = 1; // PLAYER_BACK_INSET
    
    int initial_vertices[3][2] = {
        { front_offset, 0 },
        { -offset + back_inset, offset - back_inset },
        { -offset + back_inset, -offset + back_inset }
    };
    
    int cosA_fp = cos_fp_deg(ship->angle);
    int sinA_fp = sin_fp_deg(ship->angle);
    
    int vertices[3][2];
    for (int i = 0; i < 3; i++) {
        int x_rel = initial_vertices[i][0];
        int y_rel = initial_vertices[i][1];
        vertices[i][0] = centerX + (((x_rel * cosA_fp) - (y_rel * sinA_fp)) >> FP_SHIFT);
        vertices[i][1] = centerY + (((x_rel * sinA_fp) + (y_rel * cosA_fp)) >> FP_SHIFT);
    }
    
    // Check distance to each edge of the triangle
    int thresholdSq = threshold * threshold;
    for (int i = 0; i < 3; i++) {
        int next = (i + 1) % 3;
        int distSq = distanceToSegmentSq(px, py, vertices[i][0], vertices[i][1],
                                          vertices[next][0], vertices[next][1]);
        if (distSq <= thresholdSq) return true;
    }
    
    return false;
}

// Collision Detection for GameObject vs Asteroid
// Uses triangle edges for ship, circle for bullets
bool collisionWithAsteroid(GameObject *obj, Asteroid *asteroid) {
    int x2 = FP_TO_INT(asteroid->obj.x);
    int y2 = FP_TO_INT(asteroid->obj.y);
    int baseRadius = getAsteroidRadius(asteroid->sizeType);
    
    // Only reduce collision radius for medium and large asteroids
    // Keep small asteroids at full radius for better hit detection
    int r2 = (baseRadius <= 3) ? baseRadius : baseRadius - 1;
    
    if (obj->width == 8) {
        // Ship: use triangle collision
        return pointNearShipTriangle(x2, y2, obj, r2);
    } else {
        // Bullet: use circle collision
        int x1 = FP_TO_INT(obj->x);
        int y1 = FP_TO_INT(obj->y);
        int r1 = obj->width;  // Bullet width = 2 (don't reduce for small asteroids)
        
        int dx = x1 - x2;
        int dy = y1 - y2;
        int distSq = dx * dx + dy * dy;
        int radiusSumSq = (r1 + r2) * (r1 + r2);
        
        return distSq <= radiusSumSq;
    }
}

// Legacy collision function for other uses
bool collision(GameObject *obj1, GameObject *obj2) {
    int x1 = FP_TO_INT(obj1->x);
    int y1 = FP_TO_INT(obj1->y);
    int x2 = FP_TO_INT(obj2->x);
    int y2 = FP_TO_INT(obj2->y);
    
    int r1 = obj1->width;
    int r2 = obj2->width;
    
    int dx = x1 - x2;
    int dy = y1 - y2;
    int distSq = dx * dx + dy * dy;
    int radiusSumSq = (r1 + r2) * (r1 + r2);
    
    return distSq <= radiusSumSq;
}

void initGameObject(GameObject *obj, int width, int height, int x, int y) {
    obj->width = width;
    obj->height = height;
    obj->x = INT_TO_FP(x);
    obj->y = INT_TO_FP(y);
    obj->prevX = x;
    obj->prevY = y;
    obj->velocityX = 0;
    obj->velocityY = 0;
    obj->angle = 270; // Facing up initially (270 degrees)
    obj->isAlive = 1; // Always set alive upon initialization
    // Initialize color index to a small random offset so bullets/objects
    // don't all share the same starting color phase.
    obj->colorIdx = rand() & 0xFF;
}

void setupMatch(GameObject *ship, Asteroid asteroids[], GameObject bullets[], 
    int *score, int *lives) {
    
    // Player Ship Setup
    initGameObject(ship, PLAYER_SIZE, PLAYER_SIZE, 
                   SCREEN_WIDTH/2 - PLAYER_SIZE/2, 
                   SCREEN_HEIGHT/2 - PLAYER_SIZE/2);
    *lives = 3;
    *score = 0;

    // Reset spawn timers for a new match
    s_currentSpawnInterval = INITIAL_SPAWN_INTERVAL;
    s_spawnTimer = s_currentSpawnInterval;
    s_decreaseTimer = DECREASE_INTERVAL;

    // Asteroids Setup (Start with 4 large asteroids)
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        // CRITICAL: Ensure ALL slots start DEAD
        asteroids[i].obj.isAlive = 0; 

        if (i < 4) {
            initGameObject(&asteroids[i].obj, ASTEROID_SIZE_L, ASTEROID_SIZE_L, 
                           (i % 2) ? 10 : SCREEN_WIDTH - ASTEROID_SIZE_L - 10, 
                           (i / 2) * 40 + 20);
            asteroids[i].sizeType = ASTEROID_SIZE_L;
            // Asteroid velocity is fixed-point, but set from integer values for simplicity
            asteroids[i].obj.velocityX = INT_TO_FP((i % 2) ? 1 : -1);
            asteroids[i].obj.velocityY = INT_TO_FP((i < 2) ? 1 : -1);
            // initGameObject set isAlive = 1, so no need to repeat
        }
    }

    // Bullets Setup
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].isAlive = 0;
    }
}

void updatePlayer(GameObject *ship, u16 keys) {
    static bool thrusterActive = false;
    
    // --- Continuous rotation (360-degree) ---
    if (keys & KEY_LEFT) {
        // Rotate left smoothly by ROTATION_SPEED_DEG
        ship->angle = (ship->angle - ROTATION_SPEED_DEG) % 360;
        if (ship->angle < 0) ship->angle += 360;
    }
    if (keys & KEY_RIGHT) {
        // Rotate right smoothly by ROTATION_SPEED_DEG
        ship->angle = (ship->angle + ROTATION_SPEED_DEG) % 360;
        if (ship->angle < 0) ship->angle += 360;
    }

    // Thrust
    if (keys & KEY_UP || keys & KEY_B) {
        // Start thruster sound only once
        if (!thrusterActive) {
            playThrusterSound();
            thrusterActive = true;
        }
        
        // Calculate exact angle using integer fixed-point trig
        int cosA_fp = cos_fp_deg(ship->angle);
        int sinA_fp = sin_fp_deg(ship->angle);

        // Apply acceleration using computed fixed-point sin/cos
        ship->velocityX += (ACCEL_FACTOR_FP * cosA_fp) >> FP_SHIFT;
        ship->velocityY += (ACCEL_FACTOR_FP * sinA_fp) >> FP_SHIFT;

        // Clamp velocity to max value
        if (ship->velocityX > INT_TO_FP(PLAYER_MAX_VELOCITY)) 
            ship->velocityX = INT_TO_FP(PLAYER_MAX_VELOCITY);
        if (ship->velocityX < INT_TO_FP(-PLAYER_MAX_VELOCITY)) 
            ship->velocityX = INT_TO_FP(-PLAYER_MAX_VELOCITY);
        if (ship->velocityY > INT_TO_FP(PLAYER_MAX_VELOCITY)) 
            ship->velocityY = INT_TO_FP(PLAYER_MAX_VELOCITY);
        if (ship->velocityY < INT_TO_FP(-PLAYER_MAX_VELOCITY)) 
            ship->velocityY = INT_TO_FP(-PLAYER_MAX_VELOCITY);
    } else {
        // Stop thruster sound when not accelerating
        if (thrusterActive) {
            stopThrusterSound();
            thrusterActive = false;
        }
    }

    // Update position based on velocity
    ship->prevX = FP_TO_INT(ship->x);
    ship->prevY = FP_TO_INT(ship->y);
    ship->x += ship->velocityX;
    ship->y += ship->velocityY;

    // Wrap-around screen bounds
    if (FP_TO_INT(ship->x) < -ship->width) 
        ship->x = INT_TO_FP(SCREEN_WIDTH);
    if (FP_TO_INT(ship->x) > SCREEN_WIDTH) 
        ship->x = INT_TO_FP(-ship->width);
    if (FP_TO_INT(ship->y) < -ship->height) 
        ship->y = INT_TO_FP(SCREEN_HEIGHT);
    if (FP_TO_INT(ship->y) > SCREEN_HEIGHT) 
        ship->y = INT_TO_FP(-ship->height);
    
    // Apply friction
    ship->velocityX = ship->velocityX * 253 / 256;
    ship->velocityY = ship->velocityY * 253 / 256;
}

void spawnBullet(GameObject bullets[], GameObject *ship) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].isAlive) {
            // Play laser sound
            playShootSound();
            
            // Compute exact direction using the ship's angle (integer fixed-point trig)
            int cosA_fp = cos_fp_deg(ship->angle);
            int sinA_fp = sin_fp_deg(ship->angle);

            int offset = ship->width / 2;
            int front_offset = offset + PLAYER_FRONT_EXTEND;
            int startX = FP_TO_INT(ship->x) + offset + ((front_offset * cosA_fp) >> FP_SHIFT);
            int startY = FP_TO_INT(ship->y) + offset + ((front_offset * sinA_fp) >> FP_SHIFT);
            
            initGameObject(&bullets[i], BULLET_SIZE, BULLET_SIZE, startX, startY);

            int speed_fp = INT_TO_FP(BULLET_SPEED);
            
            bullets[i].velocityX = (speed_fp * cosA_fp) >> FP_SHIFT;
            bullets[i].velocityY = (speed_fp * sinA_fp) >> FP_SHIFT;
            
            // initGameObject set isAlive = 1
            break;
        }
    }
}

void updateBullets(GameObject bullets[]) {
    // Tick used to slow bullet color cycling (advance color every N updates)
    #define BULLET_COLOR_TICK 3
    static int s_bullet_color_tick = 0;
    s_bullet_color_tick = (s_bullet_color_tick + 1) % BULLET_COLOR_TICK;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].isAlive) {
            bullets[i].prevX = FP_TO_INT(bullets[i].x);
            bullets[i].prevY = FP_TO_INT(bullets[i].y);
            bullets[i].x += bullets[i].velocityX;
            bullets[i].y += bullets[i].velocityY;
            // Advance color index only every BULLET_COLOR_TICK updates to slow cycling
            if (s_bullet_color_tick == 0) {
                bullets[i].colorIdx++;
            }
            
            // Deactivate bullets that go off-screen
            if (FP_TO_INT(bullets[i].x) < -bullets[i].width ||
                FP_TO_INT(bullets[i].x) > SCREEN_WIDTH ||
                FP_TO_INT(bullets[i].y) < -bullets[i].height ||
                FP_TO_INT(bullets[i].y) > SCREEN_HEIGHT) {
                bullets[i].isAlive = 0;
            }
        }
    }
}

void updateAsteroids(Asteroid asteroids[]) {
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (asteroids[i].obj.isAlive) {
            GameObject *obj = &asteroids[i].obj;
            obj->prevX = FP_TO_INT(obj->x);
            obj->prevY = FP_TO_INT(obj->y);
            obj->x += obj->velocityX;
            obj->y += obj->velocityY;

            // Wrap-around screen bounds
            if (FP_TO_INT(obj->x) < -obj->width) 
                obj->x = INT_TO_FP(SCREEN_WIDTH);
            if (FP_TO_INT(obj->x) > SCREEN_WIDTH) 
                obj->x = INT_TO_FP(-obj->width);
            if (FP_TO_INT(obj->y) < -obj->height) 
                obj->y = INT_TO_FP(SCREEN_HEIGHT);
            if (FP_TO_INT(obj->y) > SCREEN_HEIGHT) 
                obj->y = INT_TO_FP(-obj->height);
        }
    }
}

/**
 * Helper to spawn a new, smaller asteroid.
 */
void spawnNewAsteroid(Asteroid asteroids[], int size, int x, int y, int velX_int, int velY_int) {
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        // Find the first inactive slot
        if (!asteroids[i].obj.isAlive) { 
            initGameObject(&asteroids[i].obj, size, size, x, y);
            asteroids[i].sizeType = size;
            asteroids[i].obj.velocityX = INT_TO_FP(velX_int);
            asteroids[i].obj.velocityY = INT_TO_FP(velY_int);
            
            // initGameObject set isAlive = 1, so no need to repeat
            break;
        }
    }
}

/**
 * Manages the time-based spawning of new large asteroids.
 */
void manageAsteroidSpawning(GameObject *ship, Asteroid asteroids[]) {
    // Only spawn if the player is alive
    if (!ship->isAlive) return;

    // --- 1. Decrease Spawn Interval Timer (Time Difficulty) ---
    s_decreaseTimer--;
    if (s_decreaseTimer <= 0) {
        // Time to decrease the spawn interval
        if (s_currentSpawnInterval > MIN_SPAWN_INTERVAL) {
            s_currentSpawnInterval -= DECREASE_AMOUNT;
            // Ensure we don't drop below the minimum interval
            if (s_currentSpawnInterval < MIN_SPAWN_INTERVAL) {
                s_currentSpawnInterval = MIN_SPAWN_INTERVAL;
            }
        }
        // Reset the decrease timer
        s_decreaseTimer = DECREASE_INTERVAL;
    }

    // --- 2. Spawn Asteroid Timer (The 60-frame trigger) ---
    s_spawnTimer--;
    if (s_spawnTimer <= 0) {
        // Time to spawn a new asteroid
        
        // Randomly choose an edge to spawn from (0=Top, 1=Right, 2=Bottom, 3=Left)
        int edge = rand() % 4;
        int startX, startY, velX, velY;
        
        // Ensure starting position is outside the screen boundary
        int size = ASTEROID_SIZE_L;
        
        if (edge == 0) { // Top
            startX = rand() % SCREEN_WIDTH;
            startY = -size; // Start fully off-screen
            velX = (rand() % 3) - 1; // -1, 0, or 1
            velY = (rand() % 2) + 1; // 1 or 2 (must move down)
        } else if (edge == 1) { // Right
            startX = SCREEN_WIDTH; // Start fully off-screen
            startY = rand() % SCREEN_HEIGHT;
            velX = (rand() % 2) - 2; // -2 or -1 (must move left)
            velY = (rand() % 3) - 1; // -1, 0, or 1
        } else if (edge == 2) { // Bottom
            startX = rand() % SCREEN_WIDTH;
            startY = SCREEN_HEIGHT; // Start fully off-screen
            velX = (rand() % 3) - 1; // -1, 0, or 1
            velY = (rand() % 2) - 2; // -2 or -1 (must move up)
        } else { // Left
            startX = -size; // Start fully off-screen
            startY = rand() % SCREEN_HEIGHT;
            velX = (rand() % 2) + 1; // 1 or 2 (must move right)
            velY = (rand() % 3) - 1; // -1, 0, or 1
        }

        spawnNewAsteroid(asteroids, ASTEROID_SIZE_L, startX, startY, velX, velY);
        
        // Reset the spawn timer to the current interval (starts at 60 and decreases)
        s_spawnTimer = s_currentSpawnInterval;
    }
}

void handleCollisions(GameObject *ship, Asteroid asteroids[], GameObject bullets[], int *lives, int *score) {
    // Bullet-Asteroid Collisions
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].isAlive) {
            for (int j = 0; j < MAX_ASTEROIDS; j++) {
                Asteroid *a = &asteroids[j];
                if (a->obj.isAlive && collisionWithAsteroid(&bullets[i], a)) {
                    // Collision detected! Destroy both.
                    bullets[i].isAlive = 0;
                    playExplosionSound(); // Play explosion sound
                    
                    // Award points
                    if (a->sizeType == ASTEROID_SIZE_L) *score += 20;
                    else if (a->sizeType == ASTEROID_SIZE_M) *score += 50;
                    else *score += 100;

                    // If it was a small asteroid, simply destroy it.
                    if (a->sizeType == ASTEROID_SIZE_S) {
                        a->obj.isAlive = 0;
                    } else {
                        // Split into smaller asteroids (2 of next size down)
                        a->obj.isAlive = 0;
                        
                        int velX_base = FP_TO_INT(a->obj.velocityX);
                        int velY_base = FP_TO_INT(a->obj.velocityY);
                        
                        if (a->sizeType == ASTEROID_SIZE_L) {
                            spawnNewAsteroid(asteroids, ASTEROID_SIZE_M, FP_TO_INT(a->obj.x), FP_TO_INT(a->obj.y), 
                                             velX_base + 1, velY_base);
                            spawnNewAsteroid(asteroids, ASTEROID_SIZE_M, FP_TO_INT(a->obj.x), FP_TO_INT(a->obj.y), 
                                             velX_base - 1, velY_base);
                        } else if (a->sizeType == ASTEROID_SIZE_M) {
                            spawnNewAsteroid(asteroids, ASTEROID_SIZE_S, FP_TO_INT(a->obj.x), FP_TO_INT(a->obj.y), 
                                             velX_base, velY_base + 1);
                            spawnNewAsteroid(asteroids, ASTEROID_SIZE_S, FP_TO_INT(a->obj.x), FP_TO_INT(a->obj.y), 
                                             velX_base, velY_base - 1);
                        }
                    }
                    break; // Move to the next bullet after one successful collision
                }
            }
        }
    }
    
    // Ship-Asteroid Collisions
    for (int j = 0; j < MAX_ASTEROIDS; j++) {
        if (asteroids[j].obj.isAlive && ship->isAlive) {
            if (collisionWithAsteroid(ship, &asteroids[j])) {
                *lives -= 1;
                
                // Play hit sound (high-pitched sweep repeated 3 times)
                playPlayerHitSound();
                
                // Ship is destroyed, triggering the RESET_MODE transition in main.c.
                ship->isAlive = 0;
                
                // Reset position and velocity
                ship->x = INT_TO_FP(SCREEN_WIDTH/2 - PLAYER_SIZE/2);
                ship->y = INT_TO_FP(SCREEN_HEIGHT/2 - PLAYER_SIZE/2);
                ship->velocityX = 0;
                ship->velocityY = 0;
                ship->angle = 270;
            }
        }
    }
}