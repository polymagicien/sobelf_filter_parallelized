
#include <cuda.h>

#define CONV_COL(l,c,nb_l) \
    (c)*(nb_l)+(l) 

typedef struct pixel
{
    int r ; /* Red */
    int g ; /* Green */
    int b ; /* Blue */
} pixel ;


__global__ void apply_blur_filter_one_iter_col( pixel * res, pixel * p, int * end, int width, int height, int size_stencil){

    int n_block = blockIdx.x + gridDim.x * blockIdx.y + gridDim.x * gridDim.y * blockIdx.z;
    int n_thread_in_block = threadIdx.x + blockDim.x * threadIdx.y + blockDim.x * blockDim.y * threadIdx.z;
    
    int n = n_block * blockDim.x * blockDim.y * blockDim.z + n_thread_in_block;

    int n_pixels = width * height;

    int i = n % height;
    int i = n / height;
    int blurred_top = ( i >= size_stencil && i < height/10 && i >= size_stencil && i < width-size_stencil);
    int blurred_bottom = ( i < height-size_stencil && i >= height*9/10 && i >= size_stencil && i <n_blocks

    if( n < n_pixels){
        // 1. copy
        res[n].r = p[n].r;
        res[n].g = p[n].g;
        res[n].b = p[n].b;

        // 2. blur on top and bottom
        if ( blurred ){
            int stencil_i, stencil_j ;
            int t_r = 0 ;
            int t_g = 0 ;
            int t_b = 0 ;

            for ( stencil_i = -size_stencil ; stencil_i <= size_stencil ; stencil_i++ )
            {
                for ( stencil_j = -size_stencil ; stencil_j <= size_stencil ; stencil_j++ )
                {
                    t_r += p[CONV_COL(i+stencil_i, i+stencil_j,height)].r ;
                    t_g += p[CONV_COL(i+stencil_i, i+stencil_j,height)].g ;
                    t_b += p[CONV_COL(i+stencil_i, i+stencil_j,height)].b ;
                }
            }

            res[n].r = t_r / ( (2*size_stencil+1)*(2*size_stencil+1) ) ;
            res[n].g = t_g / ( (2*size_stencil+1)*(2*size_stencil+1) ) ;
            res[n].b = t_b / ( (2*size_stencil+1)*(2*size_stencil+1) ) ;
        } else {
            // Copy middle part fo image
            res[n].r = p[n].r;
            res[n].g = p[n].g;
            res[n].b = p[n].b;
        }

        float diff_r ;
        float diff_g ;
        float diff_b ;

        diff_r = res[n].r - p[n].r ;
        diff_g = res[n].g - p[n].g ;
        diff_b = res[n].b - p[n].b ;

        if ( diff_r > threshold || -diff_r > threshold 
                ||
                    diff_g > threshold || -diff_g > threshold
                    ||
                    diff_b > threshold || -diff_b > threshold
            ) {
            *end = 0 ;
        }

        p[n].r = res[n].r ;
        p[n].g = res[n].g ;
        p[n].b = res[n].b ;
    }
}

// img in COLUMNS
int gpu_part(int width, int height, pixel *p, int size, int threshold, pixel *res, int *end)
{
    int i, j;
    int length = width * height ;

    pixel *d_p, *d_res;
    int *d_end;
    cudaMalloc((void **)&d_p, length * sizeof(pixel)) ;
    cudaMalloc((void **)&d_res, length * sizeof(pixel)) ;
    cudaMalloc((void **)&d_end, sizeof(int)) ;
    cudaMemcpy(d_p, p, length * sizeof(pixel), cudaMemcpyHostToDevice);
    cudaMemcpy(d_end, end, length * sizeof(pixel), cudaMemcpyHostToDevice);

    dim3 bl, t;
    int n_threads = 1000;
    int n_blocks = length / n_threads;
    bl.x = n_blocks ;
    bl.y = 1 ;
    bl.z = 1 ;

    t.x = n_threads ;
    t.y = 1 ;

    apply_blur_filter_one_iter_col<<<bl,t>>>( d_res, d_p, end, width, height, size_stencil ) ;
    cudaDeviceSynchronize();
    cudaMemcpy(end, d_end, length * sizeof(pixel), cudaMemcpyDeviceToHost);
    // cudaMemcpy(res, d_res, length * sizeof(pixel), cudaMemcpyDeviceToHost);
}