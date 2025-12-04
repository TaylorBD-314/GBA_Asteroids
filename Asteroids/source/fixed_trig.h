#ifndef FIXED_TRIG_H
#define FIXED_TRIG_H

#include "game_objects.h" // For FP_SHIFT and fixed-point macros

// Returns sine of angle (degrees) as fixed-point 16.8 (Q8)
int sin_fp_deg(int deg);

// Returns cosine of angle (degrees) as fixed-point 16.8 (Q8)
int cos_fp_deg(int deg);

#endif // FIXED_TRIG_H
