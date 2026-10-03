#pragma once
#include <stdint.h>
#include <stdbool.h>

bool scrib_perf_controls(unsigned frame);
void scrib_perf_sample(uint64_t start, uint64_t input, uint64_t game,
                       uint64_t debug, uint64_t swap, uint64_t finish,
                       unsigned active, unsigned engaged);
