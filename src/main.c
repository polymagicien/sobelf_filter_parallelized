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

/* Represent one pixel from the image */
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

void apply_gray_filter_one_img(int width, int height, pixel *p)
{
    int j ;
    for ( j = 0 ; j < width * height; j++ )
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

#define CONV(l,c,nb_c) \
    (l)*(nb_c)+(c)

void apply_blur_filter_one_img( int width, int height, pixel *p, int size, int threshold )
{
    int j, k ;
    int end = 0 ;
    int n_iter = 0 ;

    pixel * new ;
    n_iter = 0 ;
    /* Allocate array of new pixels */
    new = (pixel *)malloc(width * height * sizeof( pixel ) ) ;

    /* Perform at least one blur iteration */
    do
    {
        end = 1 ;
        n_iter++ ;

        // Copy pixels of images in new 
        for(j=0; j<height-1; j++)
        {
            for(k=0; k<width-1; k++)
            {
                new[CONV(j,k,width)].r = p[CONV(j,k,width)].r ;
                new[CONV(j,k,width)].g = p[CONV(j,k,width)].g ;
                new[CONV(j,k,width)].b = p[CONV(j,k,width)].b ;
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
                        t_r += p[CONV(j+stencil_j,k+stencil_k,width)].r ;
                        t_g += p[CONV(j+stencil_j,k+stencil_k,width)].g ;
                        t_b += p[CONV(j+stencil_j,k+stencil_k,width)].b ;
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
                new[CONV(j,k,width)].r = p[CONV(j,k,width)].r ; 
                new[CONV(j,k,width)].g = p[CONV(j,k,width)].g ; 
                new[CONV(j,k,width)].b = p[CONV(j,k,width)].b ; 
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
                        t_r += p[CONV(j+stencil_j,k+stencil_k,width)].r ;
                        t_g += p[CONV(j+stencil_j,k+stencil_k,width)].g ;
                        t_b += p[CONV(j+stencil_j,k+stencil_k,width)].b ;
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

                diff_r = (new[CONV(j  ,k  ,width)].r - p[CONV(j  ,k  ,width)].r) ;
                diff_g = (new[CONV(j  ,k  ,width)].g - p[CONV(j  ,k  ,width)].g) ;
                diff_b = (new[CONV(j  ,k  ,width)].b - p[CONV(j  ,k  ,width)].b) ;

                if ( diff_r > threshold || -diff_r > threshold 
                        ||
                            diff_g > threshold || -diff_g > threshold
                            ||
                            diff_b > threshold || -diff_b > threshold
                    ) {
                    end = 0 ;
                }

                p[CONV(j  ,k  ,width)].r = new[CONV(j  ,k  ,width)].r ;
                p[CONV(j  ,k  ,width)].g = new[CONV(j  ,k  ,width)].g ;
                p[CONV(j  ,k  ,width)].b = new[CONV(j  ,k  ,width)].b ;
            }
        }

    } while ( threshold > 0 && !end ) ;

#if SOBELF_DEBUG
	printf( "BLUR: number of iterations for image %d\n", n_iter ) ;
#endif

    free (new) ;
}

void apply_sobel_filter_one_img(int width, int height, pixel *p)
{
    int j, k ;

    pixel * sobel ;
    sobel = (pixel *)malloc(width * height * sizeof( pixel ) ) ;

    for(j=1; j<height-1; j++)
    {
        for(k=1; k<width-1; k++)
        {
            int pixel_blue_no, pixel_blue_n, pixel_blue_ne;
            int pixel_blue_so, pixel_blue_s, pixel_blue_se;
            int pixel_blue_o /*, pixel_blue*/  , pixel_blue_e ;

            float deltaX_blue ;
            float deltaY_blue ;
            float val_blue;

            pixel_blue_no = p[CONV(j-1,k-1,width)].b ;
            pixel_blue_n  = p[CONV(j-1,k  ,width)].b ;
            pixel_blue_ne = p[CONV(j-1,k+1,width)].b ;
            pixel_blue_so = p[CONV(j+1,k-1,width)].b ;
            pixel_blue_s  = p[CONV(j+1,k  ,width)].b ;
            pixel_blue_se = p[CONV(j+1,k+1,width)].b ;
            pixel_blue_o  = p[CONV(j  ,k-1,width)].b ;
            // pixel_blue    = p[CONV(j  ,k  ,width)].b ;
            pixel_blue_e  = p[CONV(j  ,k+1,width)].b ;

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
            p[CONV(j  ,k  ,width)].r = sobel[CONV(j  ,k  ,width)].r ;
            p[CONV(j  ,k  ,width)].g = sobel[CONV(j  ,k  ,width)].g ;
            p[CONV(j  ,k  ,width)].b = sobel[CONV(j  ,k  ,width)].b ;
        }
    }

    free (sobel) ;

}

typedef struct img_info
{
    int width;
    int height; 
    int order;
} img_info;


void cast_flat_img(int* flat, img_info **infos, pixel **pixel_array){
    *infos = (img_info *)flat;
    *pixel_array = (pixel*)(flat + sizeof(img_info)/sizeof(int));
}

void print_flat_img(int *flat){
    img_info *infos;
    pixel *pixel_array;
    cast_flat_img(flat, &infos, &pixel_array);

    printf("*************** PRINT FLATTTEN IMG ***************\n");
    printf("order : %d\n", infos->order);
    printf("width : %d\n", infos->width);
    printf("height : %d\n", infos->height);
    printf("First pixel RGB : %d %d %d\n", pixel_array[0].r, pixel_array[0].g, pixel_array[0].b);
    int last = infos->width*infos->height - 1;
    printf("Last pixel RGB : %d %d %d\n", pixel_array[last].r, pixel_array[last].g, pixel_array[last].b);
    printf("*************** END PRINT ***************\n");
}

void print_img(animated_gif *image, int n_image){
    printf("*************** PRINT IMG ***************\n");
    printf("order : %d\n", n_image);
    printf("width : %d\n", image->width[n_image]);
    printf("height : %d\n", image->height[n_image]);
    printf("First pixel RGB : %d %d %d\n", image->p[n_image][0].r, image->p[n_image][0].g, image->p[n_image][0].b);
    int last = image->width[n_image]*image->height[n_image] - 1;
    printf("Last pixel RGB : %d %d %d\n", image->p[n_image][last].r, image->p[n_image][last].g, image->p[n_image][last].b);
    printf("*************** END PRINT ***************\n");
}

int is_grey(pixel *p, int size){
    int i;
    for(i=0; i<size; i++){
        if(p[i].b != p[i].g || p[i].b != p[i].r || p[i].r != p[i].g)
            return 0;
    }
    return 1;
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

void get_process_per_img(int *process_per_img, int* n_sub_imgs, int n_process, int n_images){
    int i;
    for(i=0; i<n_images; i++)   // Initialisation
        process_per_img[i] = 0;
    if(n_process <= n_images){
        *n_sub_imgs = n_images;
        for(i=0; i<n_images; i++)
            process_per_img[i] = 1;
    } else {
        *n_sub_imgs = n_process;
        for(i=0; i<n_process; i++)
            process_per_img[i%n_images] += 1;
    }
}

// Main entry point
int main( int argc, char ** argv )
{
    // Loading and saving
    char * input_filename ; 
    char * output_filename ;
    animated_gif * image ;

    // Measure performance
    FILE *fp; 
    int is_file_performance = 0;
    char * perf_filename ;

    // Time 
    struct timeval t1, t2;
    double duration ;
    
    // Temporary variables
    int i, j;
    MPI_Request req;
    MPI_Status status;
    int n_int_rcvd;
    int n_images;
    pixel *pixel_array;
    img_info *infos;

    // Check command-line arguments
    if ( argc < 3 )
    {
        fprintf( stderr, "Usage: %s input.gif output.gif \n", argv[0] ) ;
        return 1 ;
    }
    if(argc >= 4){
        is_file_performance = 1;
        perf_filename = argv[3];
    }
    input_filename = argv[1] ;
    output_filename = argv[2] ;

    // Init MPI
    int n_process, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &n_process);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if(rank == 0){
        // LOAD FILE INTO image
        if(load_image_from_file(&image, &n_images, input_filename) == 1)
            return 1;

        /****** OPTIMISATION BEGINS ******/
        gettimeofday(&t1, NULL);
        
        // Decide # processes / img 
        int process_per_img[n_images];
        int n_sub_img;
        get_process_per_img(process_per_img, &n_sub_img, n_process, n_images);
        
        // Divide imgs
        int *flat_imgs[n_sub_img];
        int n_bytes_msg;
        int index = 0;
        for(i=0; i<n_images; i++){
            if(process_per_img[i] == 1){
                // Compute addresses
                n_bytes_msg = sizeof(img_info) + (image->width[i] * image->height[i] * sizeof(pixel));
                flat_imgs[index] = (int *)malloc(n_bytes_msg);
                cast_flat_img(flat_imgs[index], &infos, &pixel_array);
                // Fill datas
                infos->width = image->width[i];
                infos->height = image->height[i];
                infos->order = i;
                memcpy(pixel_array, image->p[i], n_bytes_msg - sizeof(img_info));
                ++index;
            } else {
                int n_process_for_blur = process_per_img[i]/5
                for(j=0; j<)
                /* 
                    - Define the # of threads working of blurring
                    - Flatten them (MEF ghost cells)
                    - Find method to merge them (ghost cells)
                */
            }
        }

        // Flatten images
        int *flat_imgs_modified[n_images];

        for(i = 0; i < n_images; i++){
        }

        // Send all images
        for(i=0; i < n_images; i++){
            MPI_Isend(flat_imgs[i], sizeof(flat_imgs[i])/sizeof(int), MPI_INT, (i%(n_process - 1)) + 1, 0, MPI_COMM_WORLD, &req);
#ifdef DEBUG
    print_flat_img(flat_imgs[i]);
#endif
        }

        // Receive modified images
        MPI_Request req_array[n_images];
        MPI_Status status_array[n_images];
        for(i=0; i < n_images; i++){
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_INT, &n_int_rcvd);
            flat_imgs_modified[i] = (int *)malloc(n_int_rcvd*sizeof(int));
            MPI_Irecv(flat_imgs_modified[i], n_int_rcvd, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &(req_array[i]));
        }

        MPI_Waitall(n_images, req_array, status_array);

        // Send "end" signal to processes
        int buf = 0;
        for(i=1; i < n_process; i++)
            MPI_Isend(&buf, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &req);

        // Modify initial variable "image"
        for(i=0; i < n_images; i++){
            cast_flat_img(flat_imgs_modified[i], &infos, &pixel_array);
            image->p[infos->order] = pixel_array;
        }

        /****** OPTIMISATION ENDS ******/

        gettimeofday(&t2, NULL);
        duration = (t2.tv_sec-t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
        if(is_file_performance == 1){
            fp = fopen(perf_filename, "a");
            fprintf(fp, "%lf for %s\n", duration, input_filename) ;
            fclose(fp);
        }

        // EXPORTATION
        gettimeofday(&t1, NULL);
        if ( !store_pixels( output_filename, image ) ) { return 1 ; }
        gettimeofday(&t2, NULL);
        duration = (t2.tv_sec -t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
        // printf( "Export done in %lf s in file %s\n", duration, output_filename ) ;

        // free allocated
        for(i=0; i<n_images; i++){
            free(flat_imgs[i]);
            free(flat_imgs_modified[i]);
        }
    }
    else{
        int *raw_msg;

        while(1){
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_INT, &n_int_rcvd);

            if(n_int_rcvd == 1) // Signal for stop
                break;

            raw_msg = (int *)malloc(n_int_rcvd*sizeof(int));
            MPI_Recv(raw_msg, n_int_rcvd, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
            
            // Cast infos
            cast_flat_img(raw_msg, &infos, &pixel_array);

            apply_gray_filter_one_img(infos->width, infos->height, pixel_array);
            apply_blur_filter_one_img(infos->width, infos->height, pixel_array, 5, 20);
            apply_sobel_filter_one_img(infos->width, infos->height, pixel_array);

            MPI_Send(raw_msg, n_int_rcvd, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD);
            free(raw_msg);
        }
    }
    MPI_Finalize();
    return 0;
}
