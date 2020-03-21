/* Represent one pixel from the image */
#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED
#include "gif_lib.h"

typedef struct pixel
{
    int r ; /* Red */
    int g ; /* Green */
    int b ; /* Blue */
} pixel ;

/* Represent one GIF image (animated or not */
typedef struct animated_gif
{
    int n_images ; /* Number of images */
    int * width ; /* Width of each image */
    int * height ; /* Height of each image */
    pixel ** p ; /* Pixels of each image */
    GifFileType * g ; /* Internal representation. DO NOT MODIFY */
} animated_gif ;

animated_gif *load_pixels( char * filename );
int output_modified_read_gif( char * filename, GifFileType * g ) ;
int store_pixels( char * filename, animated_gif * image );
int load_image_from_file(animated_gif **image , int *n_images, char *input_filename);
void print_heuristics(int n_images, int n_process, int n_rounds, int n_parts_per_img[]);
void printf_time(char* string, struct timeval t1, struct timeval t2);
void print_how_to(int rank);
void INV_CONV(int *n_row, int *n_col, int i, int width);
void apply_gray_filter_initial( animated_gif * image );
void apply_blur_filter_initial( animated_gif * image, int size, int threshold );
void apply_sobel_filter_initial( animated_gif * image );
void apply_gray_filter_initial_one_image( animated_gif * image, int i );

void apply_blur_filter_initial_one_image( animated_gif * image, int size, int threshold, int i );
void apply_sobel_filter_initial_one_image( animated_gif * image, int i );
void apply_gray_filter_one_img(int width, int height, pixel *p);
void apply_sobel_filter_one_img_col(int width, int height, pixel *p, pixel *sobel);
void apply_blur_filter_one_iter_col( int width, int height, pixel *p, int size, int threshold, pixel *new_, int *end );


#endif