#include "fixed_trig.h"
#include <stdint.h>
#include <stdbool.h>

// Pi in Q8 fixed-point (3.14159265 * 256 ~= 804)
#define PI_FP 804

// Bhaskara I sine approximation on [0, pi]
// sin(x) ≈ (16 x (π - x)) / (5 π^2 - 4 x (π - x))
// All values are in Q8; intermediate math uses 64-bit.
int sin_fp_deg(int deg) {
    // Normalize angle to [0,359]
    deg %= 360;
    if (deg < 0) deg += 360;

    bool neg = false;
    int d = deg;
    if (d > 180) { d = 360 - d; neg = true; }

    // x in radians, Q8: x_fp = d * PI / 180
    long long x_fp = ((long long)d * PI_FP) / 180; // Q8
    long long pi = PI_FP; // Q8

    long long x_pi_x = x_fp * (pi - x_fp); // Q8 * Q8 => Q16
    long long num = 16LL * x_pi_x; // Q16
    long long pi_sq = pi * pi; // Q8 * Q8 => Q16
    long long den = 5LL * pi_sq - 4LL * x_pi_x; // Q16

    // sin_fp = (num * S) / den, where S = 1<<FP_SHIFT (to get Q8 result)
    long long S = 1LL << FP_SHIFT;
    long long sin_fp = 0;
    if (den != 0) {
        sin_fp = (num * S) / den; // result in Q8
    }

    int res = (int)sin_fp;
    if (neg) res = -res;
    return res;
}

int cos_fp_deg(int deg) {
    // cos(theta) = sin(theta + 90)
    int a = (deg + 90) % 360;
    return sin_fp_deg(a);
}
