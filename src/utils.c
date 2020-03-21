#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <mpi.h>
#include <omp.h>

#include "utils.h"

/*
 * Load a GIF image from a file and return a
 * structure of type animated_gif.
 */
animated_gif *load_pixels( char * filename ) 
{
    GifFileType * g ;
    ColorMapObject * colmap ;
    int error ;
    int n_images ;
    int * width ;
    int * height ;
    pixel ** p ;
    int i ;
    animated_gif * image ;

    /* Open the GIF image (read mode) */
    g = DGifOpenFileName( filename, &error ) ;
    if ( g == NULL ) 
    {
        fprintf( stderr, "Error DGifOpenFileName %s\n", filename ) ;
        return NULL ;
    }

    /* Read the GIF image */
    error = DGifSlurp( g ) ;
    if ( error != GIF_OK )
    {
        fprintf( stderr, 
                "Error DGifSlurp: %d <%s>\n", error, GifErrorString(g->Error) ) ;
        return NULL ;
    }

    /* Grab the number of images and the size of each image */
    n_images = g->ImageCount ;

    width = (int *)malloc( n_images * sizeof( int ) ) ;
    if ( width == NULL )
    {
        fprintf( stderr, "Unable to allocate width of size %d\n",
                n_images ) ;
        return 0 ;
    }

    height = (int *)malloc( n_images * sizeof( int ) ) ;
    if ( height == NULL )
    {
        fprintf( stderr, "Unable to allocate height of size %d\n",
                n_images ) ;
        return 0 ;
    }

    /* Fill the width and height */
    for ( i = 0 ; i < n_images ; i++ ) 
    {
        width[i] = g->SavedImages[i].ImageDesc.Width ;
        height[i] = g->SavedImages[i].ImageDesc.Height ;

#if SOBELF_DEBUG
        printf( "Image %d: l:%d t:%d w:%d h:%d interlace:%d localCM:%p\n",
                i, 
                g->SavedImages[i].ImageDesc.Left,
                g->SavedImages[i].ImageDesc.Top,
                g->SavedImages[i].ImageDesc.Width,
                g->SavedImages[i].ImageDesc.Height,
                g->SavedImages[i].ImageDesc.Interlace,
                g->SavedImages[i].ImageDesc.ColorMap
                ) ;
#endif
    }


    /* Get the global colormap */
    colmap = g->SColorMap ;
    if ( colmap == NULL ) 
    {
        fprintf( stderr, "Error global colormap is NULL\n" ) ;
        return NULL ;
    }

#if SOBELF_DEBUG
    printf( "Global color map: count:%d bpp:%d sort:%d\n",
            g->SColorMap->ColorCount,
            g->SColorMap->BitsPerPixel,
            g->SColorMap->SortFlag
            ) ;
#endif

    /* Allocate the array of pixels to be returned */
    p = (pixel **)malloc( n_images * sizeof( pixel * ) ) ;
    if ( p == NULL )
    {
        fprintf( stderr, "Unable to allocate array of %d images\n",
                n_images ) ;
        return NULL ;
    }

    for ( i = 0 ; i < n_images ; i++ ) 
    {
        p[i] = (pixel *)malloc( width[i] * height[i] * sizeof( pixel ) ) ;
        if ( p[i] == NULL )
        {
        fprintf( stderr, "Unable to allocate %d-th array of %d pixels\n",
                i, width[i] * height[i] ) ;
        return NULL ;
        }
    }
    
    /* Fill pixels */

    /* For each image */
    for ( i = 0 ; i < n_images ; i++ )
    {
        int j ;

        /* Get the local colormap if needed */
        if ( g->SavedImages[i].ImageDesc.ColorMap )
        {

            /* TODO No support for local color map */
            fprintf( stderr, "Error: application does not support local colormap\n" ) ;
            return NULL ;
            colmap = g->SavedImages[i].ImageDesc.ColorMap ;
        }

        /* Traverse the image and fill pixels */
        for ( j = 0 ; j < width[i] * height[i] ; j++ ) 
        {
            int c ;

            c = g->SavedImages[i].RasterBits[j] ;

            p[i][j].r = colmap->Colors[c].Red ;
            p[i][j].g = colmap->Colors[c].Green ;
            p[i][j].b = colmap->Colors[c].Blue ;
        }
    }

    /* Allocate image info */
    image = (animated_gif *)malloc( sizeof(animated_gif) ) ;
    if ( image == NULL ) 
    {
        fprintf( stderr, "Unable to allocate memory for animated_gif\n" ) ;
        return NULL ;
    }

    /* Fill image fields */
    image->n_images = n_images ;
    image->width = width ;
    image->height = height ;
    image->p = p ;
    image->g = g ;

#if SOBELF_DEBUG
    printf( "-> GIF w/ %d image(s) with first image of size %d x %d\n",
            image->n_images, image->width[0], image->height[0] ) ;
#endif

    return image ;
}

int output_modified_read_gif( char * filename, GifFileType * g ) 
{
    GifFileType * g2 ;
    int error2 ;

#if SOBELF_DEBUG
    printf( "Starting output to file %s\n", filename ) ;
#endif

    g2 = EGifOpenFileName( filename, false, &error2 ) ;
    if ( g2 == NULL )
    {
        fprintf( stderr, "Error EGifOpenFileName %s\n",
                filename ) ;
        return 0 ;
    }

    g2->SWidth = g->SWidth ;
    g2->SHeight = g->SHeight ;
    g2->SColorResolution = g->SColorResolution ;
    g2->SBackGroundColor = g->SBackGroundColor ;
    g2->AspectByte = g->AspectByte ;
    g2->SColorMap = g->SColorMap ;
    g2->ImageCount = g->ImageCount ;
    g2->SavedImages = g->SavedImages ;
    g2->ExtensionBlockCount = g->ExtensionBlockCount ;
    g2->ExtensionBlocks = g->ExtensionBlocks ;

    error2 = EGifSpew( g2 ) ;
    if ( error2 != GIF_OK ) 
    {
        fprintf( stderr, "Error after writing g2: %d <%s>\n", 
                error2, GifErrorString(g2->Error) ) ;
        return 0 ;
    }

    return 1 ;
}

int store_pixels( char * filename, animated_gif * image )
{
    int n_colors = 0 ;
    pixel ** p ;
    int i, j, k ;
    GifColorType * colormap ;

    /* Initialize the new set of colors */
    colormap = (GifColorType *)malloc( 256 * sizeof( GifColorType ) ) ;
    if ( colormap == NULL ) 
    {
        fprintf( stderr,
                "Unable to allocate 256 colors\n" ) ;
        return 0 ;
    }

    /* Everything is white by default */
    for ( i = 0 ; i < 256 ; i++ ) 
    {
        colormap[i].Red = 255 ;
        colormap[i].Green = 255 ;
        colormap[i].Blue = 255 ;
    }

    /* Change the background color and store it */
    int moy ;
    moy = (
            image->g->SColorMap->Colors[ image->g->SBackGroundColor ].Red
            +
            image->g->SColorMap->Colors[ image->g->SBackGroundColor ].Green
            +
            image->g->SColorMap->Colors[ image->g->SBackGroundColor ].Blue
            )/3 ;
    if ( moy < 0 ) moy = 0 ;
    if ( moy > 255 ) moy = 255 ;

#if SOBELF_DEBUG
    printf( "[DEBUG] Background color (%d,%d,%d) -> (%d,%d,%d)\n",
            image->g->SColorMap->Colors[ image->g->SBackGroundColor ].Red,
            image->g->SColorMap->Colors[ image->g->SBackGroundColor ].Green,
            image->g->SColorMap->Colors[ image->g->SBackGroundColor ].Blue,
            moy, moy, moy ) ;
#endif

    colormap[0].Red = moy ;
    colormap[0].Green = moy ;
    colormap[0].Blue = moy ;

    image->g->SBackGroundColor = 0 ;

    n_colors++ ;

    /* Process extension blocks in main structure */
    for ( j = 0 ; j < image->g->ExtensionBlockCount ; j++ )
    {
        int f ;

        f = image->g->ExtensionBlocks[j].Function ;
        if ( f == GRAPHICS_EXT_FUNC_CODE )
        {
            int tr_color = image->g->ExtensionBlocks[j].Bytes[3] ;

            if ( tr_color >= 0 &&
                    tr_color < 255 )
            {

                int found = -1 ;

                moy = 
                    (
                     image->g->SColorMap->Colors[ tr_color ].Red
                     +
                     image->g->SColorMap->Colors[ tr_color ].Green
                     +
                     image->g->SColorMap->Colors[ tr_color ].Blue
                    ) / 3 ;
                if ( moy < 0 ) moy = 0 ;
                if ( moy > 255 ) moy = 255 ;

#if SOBELF_DEBUG
                printf( "[DEBUG] Transparency color image %d (%d,%d,%d) -> (%d,%d,%d)\n",
                        i,
                        image->g->SColorMap->Colors[ tr_color ].Red,
                        image->g->SColorMap->Colors[ tr_color ].Green,
                        image->g->SColorMap->Colors[ tr_color ].Blue,
                        moy, moy, moy ) ;
#endif

                for ( k = 0 ; k < n_colors ; k++ )
                {
                    if ( 
                            moy == colormap[k].Red
                            &&
                            moy == colormap[k].Green
                            &&
                            moy == colormap[k].Blue
                       )
                    {
                        found = k ;
                    }
                }
                if ( found == -1  ) 
                {
                    if ( n_colors >= 256 ) 
                    {
                        fprintf( stderr, 
                                "Error: Found too many colors inside the image\n"
                               ) ;
                        return 0 ;
                    }

#if SOBELF_DEBUG
                    printf( "[DEBUG]\tNew color %d\n",
                            n_colors ) ;
#endif

                    colormap[n_colors].Red = moy ;
                    colormap[n_colors].Green = moy ;
                    colormap[n_colors].Blue = moy ;


                    image->g->ExtensionBlocks[j].Bytes[3] = n_colors ;

                    n_colors++ ;
                } else
                {
#if SOBELF_DEBUG
                    printf( "[DEBUG]\tFound existing color %d\n",
                            found ) ;
#endif
                    image->g->ExtensionBlocks[j].Bytes[3] = found ;
                }
            }
        }
    }

    for ( i = 0 ; i < image->n_images ; i++ )
    {
        for ( j = 0 ; j < image->g->SavedImages[i].ExtensionBlockCount ; j++ )
        {
            int f ;

            f = image->g->SavedImages[i].ExtensionBlocks[j].Function ;
            if ( f == GRAPHICS_EXT_FUNC_CODE )
            {
                int tr_color = image->g->SavedImages[i].ExtensionBlocks[j].Bytes[3] ;

                if ( tr_color >= 0 &&
                        tr_color < 255 )
                {

                    int found = -1 ;

                    moy = 
                        (
                         image->g->SColorMap->Colors[ tr_color ].Red
                         +
                         image->g->SColorMap->Colors[ tr_color ].Green
                         +
                         image->g->SColorMap->Colors[ tr_color ].Blue
                        ) / 3 ;
                    if ( moy < 0 ) moy = 0 ;
                    if ( moy > 255 ) moy = 255 ;

#if SOBELF_DEBUG
                    printf( "[DEBUG] Transparency color image %d (%d,%d,%d) -> (%d,%d,%d)\n",
                            i,
                            image->g->SColorMap->Colors[ tr_color ].Red,
                            image->g->SColorMap->Colors[ tr_color ].Green,
                            image->g->SColorMap->Colors[ tr_color ].Blue,
                            moy, moy, moy ) ;
#endif

                    for ( k = 0 ; k < n_colors ; k++ )
                    {
                        if ( 
                                moy == colormap[k].Red
                                &&
                                moy == colormap[k].Green
                                &&
                                moy == colormap[k].Blue
                           )
                        {
                            found = k ;
                        }
                    }
                    if ( found == -1  ) 
                    {
                        if ( n_colors >= 256 ) 
                        {
                            fprintf( stderr, 
                                    "Error: Found too many colors inside the image\n"
                                   ) ;
                            return 0 ;
                        }

#if SOBELF_DEBUG
                        printf( "[DEBUG]\tNew color %d\n",
                                n_colors ) ;
#endif

                        colormap[n_colors].Red = moy ;
                        colormap[n_colors].Green = moy ;
                        colormap[n_colors].Blue = moy ;


                        image->g->SavedImages[i].ExtensionBlocks[j].Bytes[3] = n_colors ;

                        n_colors++ ;
                    } else
                    {
#if SOBELF_DEBUG
                        printf( "[DEBUG]\tFound existing color %d\n",
                                found ) ;
#endif
                        image->g->SavedImages[i].ExtensionBlocks[j].Bytes[3] = found ;
                    }
                }
            }
        }
    }

#if SOBELF_DEBUG
    printf( "[DEBUG] Number of colors after background and transparency: %d\n",
            n_colors ) ;
#endif

    p = image->p ;

    /* Find the number of colors inside the image */
    for ( i = 0 ; i < image->n_images ; i++ )
    {

#if SOBELF_DEBUG
        printf( "OUTPUT: Processing image %d (total of %d images) -> %d x %d\n",
                i, image->n_images, image->width[i], image->height[i] ) ;
#endif

        for ( j = 0 ; j < image->width[i] * image->height[i] ; j++ ) 
        {
            int found = 0 ;
            for ( k = 0 ; k < n_colors ; k++ )
            {
                if ( p[i][j].r == colormap[k].Red &&
                        p[i][j].g == colormap[k].Green &&
                        p[i][j].b == colormap[k].Blue )
                {
                    found = 1 ;
                }
            }

            if ( found == 0 ) 
            {
                if ( n_colors >= 256 ) 
                {
                    fprintf( stderr, 
                            "Error: Found too many colors inside the image\n"
                           ) ;
                    return 0 ;
                }

#if SOBELF_DEBUG
                printf( "[DEBUG] Found new %d color (%d,%d,%d)\n",
                        n_colors, p[i][j].r, p[i][j].g, p[i][j].b ) ;
#endif

                colormap[n_colors].Red = p[i][j].r ;
                colormap[n_colors].Green = p[i][j].g ;
                colormap[n_colors].Blue = p[i][j].b ;
                n_colors++ ;
            }
        }
    }

#if SOBELF_DEBUG
    printf( "OUTPUT: found %d color(s)\n", n_colors ) ;
#endif


    /* Round up to a power of 2 */
    if ( n_colors != (1 << GifBitSize(n_colors) ) )
    {
        n_colors = (1 << GifBitSize(n_colors) ) ;
    }

#if SOBELF_DEBUG
    printf( "OUTPUT: Rounding up to %d color(s)\n", n_colors ) ;
#endif

    /* Change the color map inside the animated gif */
    ColorMapObject * cmo ;

    cmo = GifMakeMapObject( n_colors, colormap ) ;
    if ( cmo == NULL )
    {
        fprintf( stderr, "Error while creating a ColorMapObject w/ %d color(s)\n",
                n_colors ) ;
        return 0 ;
    }

    image->g->SColorMap = cmo ;

    /* Update the raster bits according to color map */
    for ( i = 0 ; i < image->n_images ; i++ )
    {
        for ( j = 0 ; j < image->width[i] * image->height[i] ; j++ ) 
        {
            int found_index = -1 ;
            for ( k = 0 ; k < n_colors ; k++ ) 
            {
                if ( p[i][j].r == image->g->SColorMap->Colors[k].Red &&
                        p[i][j].g == image->g->SColorMap->Colors[k].Green &&
                        p[i][j].b == image->g->SColorMap->Colors[k].Blue )
                {
                    found_index = k ;
                }
            }

            if ( found_index == -1 ) 
            {
                fprintf( stderr,
                        "Error: Unable to find a pixel in the color map\n" ) ;
                return 0 ;
            }

            image->g->SavedImages[i].RasterBits[j] = found_index ;
        }
    }


    /* Write the final image */
    if ( !output_modified_read_gif( filename, image->g ) ) { return 0 ; }

    return 1 ;
}


int load_image_from_file(animated_gif **image , int *n_images, char *input_filename){
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    *image = load_pixels( input_filename );
    *n_images = (*image)->n_images;
    if ( image == NULL ) { printf("IMAGE NULL"); return 1 ; }
    gettimeofday(&t2, NULL);
    // double duration = (t2.tv_sec -t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
    // printf( "GIF loaded from file %s with %d image(s) in %lf s\n", input_filename, image->n_images, duration);
    return 0; 
}

void print_heuristics(int n_images, int n_process, int n_rounds, int n_parts_per_img[]){
    printf(" ----> For %d images and %d process, the heuristics found %d rounds and repartition of parts: ", n_images, n_process, n_rounds);
    int counter = 0, i;
    for (i=0; i<n_images; i++){
        printf(" %i", n_parts_per_img[i]);
        counter += n_parts_per_img[i];
        if (counter % n_process == 0)
            printf(" ||");
    }
    printf("\n");
}

void printf_time(char* string, struct timeval t1, struct timeval t2) {

    double duration = (t2.tv_sec-t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
    char dest[200] = "";
	strcat(dest, "Duration of ");
    strcat(dest, string);
    strcat(dest, " %lf\n");

    printf( dest, duration);
}

void print_how_to(rank){
    if (rank == 0) {
        printf("INVALID ARGUMENT\n----------------------------------------------------------------------------------------------------------\n");
        printf("USAGE: ./sobelf input_filename output_filename [-option value]\n");
        printf("OPTIONS: \n    -file : writing result in a file \n    -beta : 1 if you want to limit the number of parts, 0 if not (default 1)\n");
        printf("    -verifgif : 1 if you want to verify the result (default 0)\n");
        printf("EXAMPLE:  ./sobelf input_filename output_filename -file output.txt -beta 1 -rootwork 0 -verifgif 1");
        printf("\n----------------------------------------------------------------------------------------------------------\n\n\n");
    }
}

#define CONV(l,c,nb_c) \
    (l)*(nb_c)+(c)


#define CONV_COL(l,c,nb_l) \
    (c)*(nb_l)+(l) 

void INV_CONV(int *n_row, int *n_col, int i, int width){
    *n_row = i / width;
    *n_col = i % width;
}

// BASIC FUNCTIONS

void apply_gray_filter_initial( animated_gif * image )
{
    int i, j ;
    pixel ** p ;

    p = image->p ;

    for ( i = 0 ; i < image->n_images ; i++ )
    {
        for ( j = 0 ; j < image->width[i] * image->height[i] ; j++ )
        {
            int moy ;

            moy = (p[i][j].r + p[i][j].g + p[i][j].b)/3 ;
            if ( moy < 0 ) moy = 0 ;
            if ( moy > 255 ) moy = 255 ;

            p[i][j].r = moy ;
            p[i][j].g = moy ;
            p[i][j].b = moy ;
        }
    }
}

void apply_blur_filter_initial( animated_gif * image, int size, int threshold )
{
    int i, j, k ;
    int width, height ;
    int end = 0 ;
    int n_iter = 0 ;

    pixel ** p ;
    pixel * new ;

    /* Get the pixels of all images */
    p = image->p ;


    /* Process all images */
    for ( i = 0 ; i < image->n_images ; i++ )
    {
        n_iter = 0 ;
        width = image->width[i] ;
        height = image->height[i] ;

        /* Allocate array of new pixels */
        new = (pixel *)malloc(width * height * sizeof( pixel ) ) ;

        /* Perform at least one blur iteration */
        do
        {
            end = 1 ;
            n_iter++ ;

            // Copy pixels of images in ew 
            for(j=0; j<height-1; j++)
            {
                for(k=0; k<width-1; k++)
                {
                    new[CONV(j,k,width)].r = p[i][CONV(j,k,width)].r ;
                    new[CONV(j,k,width)].g = p[i][CONV(j,k,width)].g ;
                    new[CONV(j,k,width)].b = p[i][CONV(j,k,width)].b ;
                }
            }

            /* Apply blur on top part of image (10%) */
            for(j=size; j<height/10-size; j++)
            {
                for(k=size; k<width-size; k++)
                {
                    int stencil_j, stencil_k ;
                    int t_r = 0 ;
                    int t_g = 0 ;
                    int t_b = 0 ;

                    for ( stencil_j = -size ; stencil_j <= size ; stencil_j++ )
                    {
                        for ( stencil_k = -size ; stencil_k <= size ; stencil_k++ )
                        {
                            t_r += p[i][CONV(j+stencil_j,k+stencil_k,width)].r ;
                            t_g += p[i][CONV(j+stencil_j,k+stencil_k,width)].g ;
                            t_b += p[i][CONV(j+stencil_j,k+stencil_k,width)].b ;
                        }
                    }

                    new[CONV(j,k,width)].r = t_r / ( (2*size+1)*(2*size+1) ) ;
                    new[CONV(j,k,width)].g = t_g / ( (2*size+1)*(2*size+1) ) ;
                    new[CONV(j,k,width)].b = t_b / ( (2*size+1)*(2*size+1) ) ;
                }
            }

            /* Copy the middle part of the image */
            for(j=height/10-size; j<height*0.9+size; j++)
            {
                for(k=size; k<width-size; k++)
                {
                    new[CONV(j,k,width)].r = p[i][CONV(j,k,width)].r ; 
                    new[CONV(j,k,width)].g = p[i][CONV(j,k,width)].g ; 
                    new[CONV(j,k,width)].b = p[i][CONV(j,k,width)].b ; 
                }
            }

            /* Apply blur on the bottom part of the image (10%) */
            for(j=height*0.9+size; j<height-size; j++)
            {
                for(k=size; k<width-size; k++)
                {
                    int stencil_j, stencil_k ;
                    int t_r = 0 ;
                    int t_g = 0 ;
                    int t_b = 0 ;

                    for ( stencil_j = -size ; stencil_j <= size ; stencil_j++ )
                    {
                        for ( stencil_k = -size ; stencil_k <= size ; stencil_k++ )
                        {
                            t_r += p[i][CONV(j+stencil_j,k+stencil_k,width)].r ;
                            t_g += p[i][CONV(j+stencil_j,k+stencil_k,width)].g ;
                            t_b += p[i][CONV(j+stencil_j,k+stencil_k,width)].b ;
                        }
                    }

                    new[CONV(j,k,width)].r = t_r / ( (2*size+1)*(2*size+1) ) ;
                    new[CONV(j,k,width)].g = t_g / ( (2*size+1)*(2*size+1) ) ;
                    new[CONV(j,k,width)].b = t_b / ( (2*size+1)*(2*size+1) ) ;
                }
            }

            for(j=1; j<height-1; j++)
            {
                for(k=1; k<width-1; k++)
                {

                    float diff_r ;
                    float diff_g ;
                    float diff_b ;

                    diff_r = (new[CONV(j  ,k  ,width)].r - p[i][CONV(j  ,k  ,width)].r) ;
                    diff_g = (new[CONV(j  ,k  ,width)].g - p[i][CONV(j  ,k  ,width)].g) ;
                    diff_b = (new[CONV(j  ,k  ,width)].b - p[i][CONV(j  ,k  ,width)].b) ;

                    if ( diff_r > threshold || -diff_r > threshold 
                            ||
                             diff_g > threshold || -diff_g > threshold
                             ||
                              diff_b > threshold || -diff_b > threshold
                       ) {
                        end = 0 ;
                    }

                    p[i][CONV(j  ,k  ,width)].r = new[CONV(j  ,k  ,width)].r ;
                    p[i][CONV(j  ,k  ,width)].g = new[CONV(j  ,k  ,width)].g ;
                    p[i][CONV(j  ,k  ,width)].b = new[CONV(j  ,k  ,width)].b ;
                }
            }

        } while ( threshold > 0 && !end ) ;

        // printf("niter = %i\n", n_iter);

        free (new) ;
    }

}

void apply_sobel_filter_initial( animated_gif * image )
{
    int i, j, k ;
    int width, height ;

    pixel ** p ;

    p = image->p ;

    for ( i = 0 ; i < image->n_images ; i++ )
    {
        width = image->width[i] ;
        height = image->height[i] ;

        pixel * sobel ;

        sobel = (pixel *)malloc(width * height * sizeof( pixel ) ) ;

        for(j=1; j<height-1; j++)
        {
            for(k=1; k<width-1; k++)
            {
                int pixel_blue_no, pixel_blue_n, pixel_blue_ne;
                int pixel_blue_so, pixel_blue_s, pixel_blue_se;
                int pixel_blue_o , pixel_blue  , pixel_blue_e ;

                float deltaX_blue ;
                float deltaY_blue ;
                float val_blue;

                pixel_blue_no = p[i][CONV(j-1,k-1,width)].b ;
                pixel_blue_n  = p[i][CONV(j-1,k  ,width)].b ;
                pixel_blue_ne = p[i][CONV(j-1,k+1,width)].b ;
                pixel_blue_so = p[i][CONV(j+1,k-1,width)].b ;
                pixel_blue_s  = p[i][CONV(j+1,k  ,width)].b ;
                pixel_blue_se = p[i][CONV(j+1,k+1,width)].b ;
                pixel_blue_o  = p[i][CONV(j  ,k-1,width)].b ;
                pixel_blue    = p[i][CONV(j  ,k  ,width)].b ;
                pixel_blue_e  = p[i][CONV(j  ,k+1,width)].b ;

                deltaX_blue = -pixel_blue_no + pixel_blue_ne - 2*pixel_blue_o + 2*pixel_blue_e - pixel_blue_so + pixel_blue_se;             

                deltaY_blue = pixel_blue_se + 2*pixel_blue_s + pixel_blue_so - pixel_blue_ne - 2*pixel_blue_n - pixel_blue_no;

                val_blue = sqrt(deltaX_blue * deltaX_blue + deltaY_blue * deltaY_blue)/4;


                if ( val_blue > 50 ) 
                {
                    sobel[CONV(j  ,k  ,width)].r = 255 ;
                    sobel[CONV(j  ,k  ,width)].g = 255 ;
                    sobel[CONV(j  ,k  ,width)].b = 255 ;
                } else
                {
                    sobel[CONV(j  ,k  ,width)].r = 0 ;
                    sobel[CONV(j  ,k  ,width)].g = 0 ;
                    sobel[CONV(j  ,k  ,width)].b = 0 ;
                }
            }
        }

        for(j=1; j<height-1; j++)
        {
            for(k=1; k<width-1; k++)
            {
                p[i][CONV(j  ,k  ,width)].r = sobel[CONV(j  ,k  ,width)].r ;
                p[i][CONV(j  ,k  ,width)].g = sobel[CONV(j  ,k  ,width)].g ;
                p[i][CONV(j  ,k  ,width)].b = sobel[CONV(j  ,k  ,width)].b ;
            }
        }

        free (sobel) ;
    }

}

// BASIC FUNCTIONS FOR ONE IMAGE


void apply_gray_filter_initial_one_image( animated_gif * image, int i )
{
    int j ;
    pixel ** p ;

    p = image->p ;

    for ( j = 0 ; j < image->width[i] * image->height[i] ; j++ )
    {
        int moy ;

        moy = (p[i][j].r + p[i][j].g + p[i][j].b)/3 ;
        if ( moy < 0 ) moy = 0 ;
        if ( moy > 255 ) moy = 255 ;

        p[i][j].r = moy ;
        p[i][j].g = moy ;
        p[i][j].b = moy ;
    }
}

void apply_blur_filter_initial_one_image( animated_gif * image, int size, int threshold, int i )
{
    int j, k ;
    int width, height ;
    int end = 0 ;
    int n_iter = 0 ;

    pixel ** p ;
    pixel * new ;

    /* Get the pixels of all images */
    p = image->p ;

    n_iter = 0 ;
    width = image->width[i] ;
    height = image->height[i] ;

    /* Allocate array of new pixels */
    new = (pixel *)malloc(width * height * sizeof( pixel ) ) ;

    /* Perform at least one blur iteration */
    do
    {
        end = 1 ;
        n_iter++ ;

        // Copy pixels of images in ew 
        for(j=0; j<height-1; j++)
        {
            for(k=0; k<width-1; k++)
            {
                new[CONV(j,k,width)].r = p[i][CONV(j,k,width)].r ;
                new[CONV(j,k,width)].g = p[i][CONV(j,k,width)].g ;
                new[CONV(j,k,width)].b = p[i][CONV(j,k,width)].b ;
            }
        }

        /* Apply blur on top part of image (10%) */
        for(j=size; j<height/10-size; j++)
        {
            for(k=size; k<width-size; k++)
            {
                int stencil_j, stencil_k ;
                int t_r = 0 ;
                int t_g = 0 ;
                int t_b = 0 ;

                for ( stencil_j = -size ; stencil_j <= size ; stencil_j++ )
                {
                    for ( stencil_k = -size ; stencil_k <= size ; stencil_k++ )
                    {
                        t_r += p[i][CONV(j+stencil_j,k+stencil_k,width)].r ;
                        t_g += p[i][CONV(j+stencil_j,k+stencil_k,width)].g ;
                        t_b += p[i][CONV(j+stencil_j,k+stencil_k,width)].b ;
                    }
                }

                new[CONV(j,k,width)].r = t_r / ( (2*size+1)*(2*size+1) ) ;
                new[CONV(j,k,width)].g = t_g / ( (2*size+1)*(2*size+1) ) ;
                new[CONV(j,k,width)].b = t_b / ( (2*size+1)*(2*size+1) ) ;
            }
        }

        /* Copy the middle part of the image */
        for(j=height/10-size; j<height*0.9+size; j++)
        {
            for(k=size; k<width-size; k++)
            {
                new[CONV(j,k,width)].r = p[i][CONV(j,k,width)].r ; 
                new[CONV(j,k,width)].g = p[i][CONV(j,k,width)].g ; 
                new[CONV(j,k,width)].b = p[i][CONV(j,k,width)].b ; 
            }
        }

        /* Apply blur on the bottom part of the image (10%) */
        for(j=height*0.9+size; j<height-size; j++)
        {
            for(k=size; k<width-size; k++)
            {
                int stencil_j, stencil_k ;
                int t_r = 0 ;
                int t_g = 0 ;
                int t_b = 0 ;

                for ( stencil_j = -size ; stencil_j <= size ; stencil_j++ )
                {
                    for ( stencil_k = -size ; stencil_k <= size ; stencil_k++ )
                    {
                        t_r += p[i][CONV(j+stencil_j,k+stencil_k,width)].r ;
                        t_g += p[i][CONV(j+stencil_j,k+stencil_k,width)].g ;
                        t_b += p[i][CONV(j+stencil_j,k+stencil_k,width)].b ;
                    }
                }

                new[CONV(j,k,width)].r = t_r / ( (2*size+1)*(2*size+1) ) ;
                new[CONV(j,k,width)].g = t_g / ( (2*size+1)*(2*size+1) ) ;
                new[CONV(j,k,width)].b = t_b / ( (2*size+1)*(2*size+1) ) ;
            }
        }

        for(j=1; j<height-1; j++)
        {
            for(k=1; k<width-1; k++)
            {

                float diff_r ;
                float diff_g ;
                float diff_b ;

                diff_r = (new[CONV(j  ,k  ,width)].r - p[i][CONV(j  ,k  ,width)].r) ;
                diff_g = (new[CONV(j  ,k  ,width)].g - p[i][CONV(j  ,k  ,width)].g) ;
                diff_b = (new[CONV(j  ,k  ,width)].b - p[i][CONV(j  ,k  ,width)].b) ;

                if ( diff_r > threshold || -diff_r > threshold 
                        ||
                            diff_g > threshold || -diff_g > threshold
                            ||
                            diff_b > threshold || -diff_b > threshold
                    ) {
                    end = 0 ;
                }

                p[i][CONV(j  ,k  ,width)].r = new[CONV(j  ,k  ,width)].r ;
                p[i][CONV(j  ,k  ,width)].g = new[CONV(j  ,k  ,width)].g ;
                p[i][CONV(j  ,k  ,width)].b = new[CONV(j  ,k  ,width)].b ;
            }
        }

    } while ( threshold > 0 && !end ) ;

    // printf("niter = %i\n", n_iter);

    free (new) ;
}

void apply_sobel_filter_initial_one_image( animated_gif * image, int i )
{
    int j, k ;
    int width, height ;

    pixel ** p ;

    p = image->p ;

    width = image->width[i] ;
    height = image->height[i] ;

    pixel * sobel ;

    sobel = (pixel *)malloc(width * height * sizeof( pixel ) ) ;

    for(j=1; j<height-1; j++)
    {
        for(k=1; k<width-1; k++)
        {
            int pixel_blue_no, pixel_blue_n, pixel_blue_ne;
            int pixel_blue_so, pixel_blue_s, pixel_blue_se;
            int pixel_blue_o , pixel_blue  , pixel_blue_e ;

            float deltaX_blue ;
            float deltaY_blue ;
            float val_blue;

            pixel_blue_no = p[i][CONV(j-1,k-1,width)].b ;
            pixel_blue_n  = p[i][CONV(j-1,k  ,width)].b ;
            pixel_blue_ne = p[i][CONV(j-1,k+1,width)].b ;
            pixel_blue_so = p[i][CONV(j+1,k-1,width)].b ;
            pixel_blue_s  = p[i][CONV(j+1,k  ,width)].b ;
            pixel_blue_se = p[i][CONV(j+1,k+1,width)].b ;
            pixel_blue_o  = p[i][CONV(j  ,k-1,width)].b ;
            pixel_blue    = p[i][CONV(j  ,k  ,width)].b ;
            pixel_blue_e  = p[i][CONV(j  ,k+1,width)].b ;

            deltaX_blue = -pixel_blue_no + pixel_blue_ne - 2*pixel_blue_o + 2*pixel_blue_e - pixel_blue_so + pixel_blue_se;             

            deltaY_blue = pixel_blue_se + 2*pixel_blue_s + pixel_blue_so - pixel_blue_ne - 2*pixel_blue_n - pixel_blue_no;

            val_blue = sqrt(deltaX_blue * deltaX_blue + deltaY_blue * deltaY_blue)/4;


            if ( val_blue > 50 ) 
            {
                sobel[CONV(j  ,k  ,width)].r = 255 ;
                sobel[CONV(j  ,k  ,width)].g = 255 ;
                sobel[CONV(j  ,k  ,width)].b = 255 ;
            } else
            {
                sobel[CONV(j  ,k  ,width)].r = 0 ;
                sobel[CONV(j  ,k  ,width)].g = 0 ;
                sobel[CONV(j  ,k  ,width)].b = 0 ;
            }
        }
    }

    for(j=1; j<height-1; j++)
    {
        for(k=1; k<width-1; k++)
        {
            p[i][CONV(j  ,k  ,width)].r = sobel[CONV(j  ,k  ,width)].r ;
            p[i][CONV(j  ,k  ,width)].g = sobel[CONV(j  ,k  ,width)].g ;
            p[i][CONV(j  ,k  ,width)].b = sobel[CONV(j  ,k  ,width)].b ;
        }
    }

    free (sobel) ;
}

// OUR FUNCTIONS FOR MPI AND OPENMP

void apply_gray_filter_one_img(int width, int height, pixel *p)
{
    int j ;
    int end_loop = width * height;

    #pragma omp for
        for ( j = 0 ; j < end_loop ; j++ )
        {
            int moy ;
            moy = (p[j].r + p[j].g + p[j].b)/3 ;
            if ( moy < 0 ) moy = 0 ;
            if ( moy > 255 ) moy = 255 ;

            p[j].r = moy ;
            p[j].g = moy ;
            p[j].b = moy ;
        }
}

void apply_sobel_filter_one_img_col(int width, int height, pixel *p, pixel *sobel)
{
    int j, k ;
    int hmu = height - 1;
    int wmu = width - 1;

    #pragma omp for
        for(k=1; k<wmu; k++)
        {
            for(j=1; j<hmu; j++)
            {
                int pixel_blue_no, pixel_blue_n, pixel_blue_ne;
                int pixel_blue_so, pixel_blue_s, pixel_blue_se;
                int pixel_blue_o /*, pixel_blue*/  , pixel_blue_e ;

                float deltaX_blue ;
                float deltaY_blue ;
                float val_blue;

                pixel_blue_no = p[CONV_COL(j-1,k-1,height)].b ;
                pixel_blue_n  = p[CONV_COL(j-1,k  ,height)].b ;
                pixel_blue_ne = p[CONV_COL(j-1,k+1,height)].b ;
                pixel_blue_so = p[CONV_COL(j+1,k-1,height)].b ;
                pixel_blue_s  = p[CONV_COL(j+1,k  ,height)].b ;
                pixel_blue_se = p[CONV_COL(j+1,k+1,height)].b ;
                pixel_blue_o  = p[CONV_COL(j  ,k-1,height)].b ;
                // pixel_blue    = p[CONV_COL(j  ,k  ,height)].b ;
                
                pixel_blue_e  = p[CONV_COL(j  ,k+1,height)].b ;
                deltaX_blue = -pixel_blue_no + pixel_blue_ne - 2*pixel_blue_o + 2*pixel_blue_e - pixel_blue_so + pixel_blue_se;             
                deltaY_blue = pixel_blue_se + 2*pixel_blue_s + pixel_blue_so - pixel_blue_ne - 2*pixel_blue_n - pixel_blue_no;

                val_blue = sqrt(deltaX_blue * deltaX_blue + deltaY_blue * deltaY_blue)/4;


                if ( val_blue > 50 ) 
                {
                    sobel[CONV_COL(j  ,k  ,height)].r = 255 ;
                    sobel[CONV_COL(j  ,k  ,height)].g = 255 ;
                    sobel[CONV_COL(j  ,k  ,height)].b = 255 ;
                } else
                {
                    sobel[CONV_COL(j  ,k  ,height)].r = 0 ;
                    sobel[CONV_COL(j  ,k  ,height)].g = 0 ;
                    sobel[CONV_COL(j  ,k  ,height)].b = 0 ;
                }
            }
        }

    #pragma omp for
        for(k=1; k<wmu; k++)
        {
            for(j=1; j<hmu; j++)
            {
                p[CONV_COL(j  ,k  ,height)].r = sobel[CONV_COL(j  ,k  ,height)].r ;
                p[CONV_COL(j  ,k  ,height)].g = sobel[CONV_COL(j  ,k  ,height)].g ;
                p[CONV_COL(j  ,k  ,height)].b = sobel[CONV_COL(j  ,k  ,height)].b ;
            }
        }

}

void apply_blur_filter_one_iter_col( int width, int height, pixel *p, int size, int threshold, pixel *new_, int *end )
{
    
    int j, k ;

    // Limits of for loops
    int end_loop = height*0.9+size;
    int begin_loop = height/10-size;
    int end_last_loop = height-size;
    int end_mid_loop = width-size;
    int hmu = height - 1;
    int wmu = width - 1;
    int msize = -size;

    // Copy pixels of images in ew 
    #pragma omp for
        for(k=0; k<wmu; k++)
        {
            for(j=0; j<hmu; j++)
            {
                new_[CONV_COL(j,k,height)].r = p[CONV_COL(j,k,height)].r ;
                new_[CONV_COL(j,k,height)].g = p[CONV_COL(j,k,height)].g ;
                new_[CONV_COL(j,k,height)].b = p[CONV_COL(j,k,height)].b ;
            }
        }

        /* Apply blur on top part of image (10%) */
    #pragma omp for
        for(k=size; k<end_mid_loop; k++)
        {
            for(j=size; j<begin_loop; j++)
            {
                int stencil_j, stencil_k ;
                int t_r = 0 ;
                int t_g = 0 ;
                int t_b = 0 ;

                for ( stencil_j = -size ; stencil_j <= size ; stencil_j++ )
                {
                    for ( stencil_k = -size ; stencil_k <= size ; stencil_k++ )
                    {
                        t_r += p[CONV_COL(j+stencil_j,k+stencil_k,height)].r ;
                        t_g += p[CONV_COL(j+stencil_j,k+stencil_k,height)].g ;
                        t_b += p[CONV_COL(j+stencil_j,k+stencil_k,height)].b ;
                    }
                }

                new_[CONV_COL(j,k,height)].r = t_r / ( (2*size+1)*(2*size+1) ) ;
                new_[CONV_COL(j,k,height)].g = t_g / ( (2*size+1)*(2*size+1) ) ;
                new_[CONV_COL(j,k,height)].b = t_b / ( (2*size+1)*(2*size+1) ) ;
            }
        }

    
        /* Copy the middle part of the image */
    #pragma omp for 
        for(k=size; k<end_mid_loop; k++)
        {
            for(j= begin_loop; j< end_loop; j++)
            {
                new_[CONV_COL(j,k,height)].r = p[CONV_COL(j,k,height)].r ; 
                new_[CONV_COL(j,k,height)].g = p[CONV_COL(j,k,height)].g ; 
                new_[CONV_COL(j,k,height)].b = p[CONV_COL(j,k,height)].b ; 
            }
        }

   
        /* Apply blur on the bottom part of the image (10%) */
    #pragma omp for
        for(k=size; k<end_mid_loop; k++)
        {
            for(j=end_loop; j<end_last_loop; j++)
            {
                int stencil_j, stencil_k ;
                int t_r = 0 ;
                int t_g = 0 ;
                int t_b = 0 ;

                for ( stencil_j = msize ; stencil_j <= size ; stencil_j++ )
                {
                    for ( stencil_k = -size ; stencil_k <= size ; stencil_k++ )
                    {
                        t_r += p[CONV_COL(j+stencil_j,k+stencil_k,height)].r ;
                        t_g += p[CONV_COL(j+stencil_j,k+stencil_k,height)].g ;
                        t_b += p[CONV_COL(j+stencil_j,k+stencil_k,height)].b ;
                    }
                }

                new_[CONV_COL(j,k,height)].r = t_r / ( (2*size+1)*(2*size+1) ) ;
                new_[CONV_COL(j,k,height)].g = t_g / ( (2*size+1)*(2*size+1) ) ;
                new_[CONV_COL(j,k,height)].b = t_b / ( (2*size+1)*(2*size+1) ) ;
            }
        }

   
    #pragma omp for
            
        for(k=1; k<wmu; k++)
        {
            for(j=1; j<hmu; j++)
            {
                float diff_r ;
                float diff_g ;
                float diff_b ;

                diff_r = (new_[CONV_COL(j  ,k  ,height)].r - p[CONV_COL(j  ,k  ,height)].r) ;
                diff_g = (new_[CONV_COL(j  ,k  ,height)].g - p[CONV_COL(j  ,k  ,height)].g) ;
                diff_b = (new_[CONV_COL(j  ,k  ,height)].b - p[CONV_COL(j  ,k  ,height)].b) ;

                if ( diff_r > threshold || -diff_r > threshold 
                        ||
                            diff_g > threshold || -diff_g > threshold
                            ||
                            diff_b > threshold || -diff_b > threshold
                    ) {
                    *end = 0 ;
                }

                p[CONV_COL(j  ,k  ,height)].r = new_[CONV_COL(j  ,k  ,height)].r ;
                p[CONV_COL(j  ,k  ,height)].g = new_[CONV_COL(j  ,k  ,height)].g ;
                p[CONV_COL(j  ,k  ,height)].b = new_[CONV_COL(j  ,k  ,height)].b ;
            }
        }
}