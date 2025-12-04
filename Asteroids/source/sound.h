#ifndef SOUND_H
#define SOUND_H

#include <gba_types.h>

// Sound effect functions
void playShootSound(void);
void playExplosionSound(void);
void playMenuSelectSound(void);
void playThrusterSound(void);
void stopThrusterSound(void);
void playSirenSound(void);
void playPlayerHitSound(void);

// Procedural music functions (channel 2)
void initProceduralMusic(void);
void updateProceduralMusic(void);
void stopProceduralMusic(void);

#endif
