#include "utils.h"
#include <stdlib.h>
#include "platform.h"

void random_seed() {
    srand(platform_time());
}

f64 random_ndc() {
    return rand() / (f64)RAND_MAX;
}

u32 random_int(u32 min, u32 max) {
    return (u32)(random_ndc() * (max - min) + min);
}

f32 random_float(f32 min, f32 max) {
    return (f32)(random_ndc() * (max - min) + min);
}

f64 random_double(f64 min, f64 max) {
    return (f64)(random_ndc() * (max - min) + min);
}