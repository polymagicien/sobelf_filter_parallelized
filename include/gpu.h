#ifndef GPU_H_INCLUDED
#define GPU_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include "utils.h"

// __global__ void apply_blur_filter_one_iter_col_gpu( pixel * res, pixel * p, int * end, int width, int height, int size_stencil, int threshold);
void gpu_part(int width, int height, pixel *p, int size, int threshold, pixel *res, int *end);

#endif