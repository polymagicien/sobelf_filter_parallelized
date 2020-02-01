/*
 * INF560
 *
 * Image Filtering Project
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <mpi.h>

#include "gif_lib.h"

/* Set this macro to 1 to enable debugging information */
#define SOBELF_DEBUG 0
#define SIZE_STENCIL 5


// rm test/result.txt; mpicc -o test/test test/main.c; mpirun -n 1 ./test/test

typedef struct img_info
{
    int width;
    int height; 
    int order;
    int order_sub_img;
    int blur_up_from;
    int blur_up_to;
    int blur_down_from;
    int blur_down_to;
} img_info;

typedef struct pixel
{
    int r;
    int g;
    int b;
} pixel;


void cast_flat_img(int* flat, img_info **infos, pixel **pixel_array){
    *infos = (img_info *)flat;
    *pixel_array = (pixel*)(flat + sizeof(img_info)/sizeof(int));
}

int conv(int i, int j, int width){
    return i*width + j;
}

// Main entry point
int main( int argc, char ** argv )
{
    // Time 
    struct timeval t1, t2;
    double duration ;
    
    // Temporary variables
    int i, j, k;
    MPI_Request req;
    MPI_Status status;

    // Init MPI
    int n_process, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &n_process);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int height, width;
    srand48(6);

    if(rank == 0){
        char *saving_filename = "test/result.txt";
        FILE *fp;
        
        int size = 4;        
        int height_array[] = {10, 100, 1000, 10000};
        int width_array[] = {10, 100, 1000, 10000};
        int height, width;
        pixel *fake_pixels, *pixels_rotated;

        for(k=0; k<size; k++){
            height = height_array[k];
            width = width_array[k];
            printf("h: %d    w: %d\n", height, width);
            gettimeofday(&t1, NULL);
            fake_pixels = (pixel *)malloc(height*width*sizeof(pixel)); 
            gettimeofday(&t2, NULL);
            duration = (t2.tv_sec-t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
            printf("Malloc done in %lf s\n", duration);

            gettimeofday(&t1, NULL);
            for(i=0; i<height; i++){
                for(j=0; j<width; j++){
                    fake_pixels[conv(i, j, width)].r = lrand48()%255;
                    fake_pixels[conv(i, j, width)].g = lrand48()%255;
                    fake_pixels[conv(i, j, width)].b = lrand48()%255;
                }
            }
            gettimeofday(&t2, NULL);
            duration = (t2.tv_sec-t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
            printf("Random assignement done in %lf s\n", duration);


            gettimeofday(&t1, NULL);
            pixels_rotated = (pixel *)malloc(height*width*sizeof(pixel)); 
            int new_i, new_j;
            for(i=0; i<height; i++){
                for(j=0; j<width; j++){
                    new_i = width - 1 - j;
                    new_j = i;
                    pixels_rotated[conv(i, j, width)].r = fake_pixels[conv(new_i, new_j, height)].r;
                    pixels_rotated[conv(i, j, width)].g = fake_pixels[conv(new_i, new_j, height)].g;
                    pixels_rotated[conv(i, j, width)].b = fake_pixels[conv(new_i, new_j, height)].b;
                }
            }
            gettimeofday(&t2, NULL);
            duration = (t2.tv_sec-t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
            printf("Rotation done in %lf s\n", duration);
            fp = fopen(saving_filename, "a");
            fprintf(fp, "%lf,", duration);
            fclose(fp);
            free(fake_pixels);
            free(pixels_rotated);
        }
    }
    MPI_Finalize();
    return 0;
}
