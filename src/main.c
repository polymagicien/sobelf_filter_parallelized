/*
 * INF560
 *
 * Image Filtering Project
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <mpi.h>

#include "gif_lib.h"

// Make tester
// mpirun -n 1 ./test args

/* Set this macro to 1 to enable debugging information */
#define SOBELF_DEBUG 0
#define SIZE_STENCIL 5

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

#define CONV(l,c,nb_c) \
    (l)*(nb_c)+(c)


#define CONV_COL(l,c,nb_l) \
    (c)*(nb_l)+(l) 


void apply_gray_filter_one_img(int width, int height, pixel *p)
{
    int j ;
    int end_loop = width * height;
    #pragma omp parallel default(none) private(j) shared(end_loop,p)
    {

        #pragma omp for schedule(static,20)
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
}

void apply_sobel_filter_one_img_col(int width, int height, pixel *p)
{
    int j, k ;
    pixel * sobel ;
    sobel = (pixel *)malloc(width * height * sizeof( pixel ) ) ;
    int hmu = height - 1;
    int wmu = width - 1;

    #pragma omp parallel default(none) private(j,k) shared(hmu, wmu, p, sobel, height)
    {

        #pragma omp for schedule(static,20)
            for(j=1; j<hmu; j++)
            {
                for(k=1; k<wmu; k++)
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

        #pragma omp for schedule(static,20)
            for(j=1; j<hmu; j++)
            {
                for(k=1; k<wmu; k++)
                {
                    p[CONV_COL(j  ,k  ,height)].r = sobel[CONV_COL(j  ,k  ,height)].r ;
                    p[CONV_COL(j  ,k  ,height)].g = sobel[CONV_COL(j  ,k  ,height)].g ;
                    p[CONV_COL(j  ,k  ,height)].b = sobel[CONV_COL(j  ,k  ,height)].b ;
                }
            }

    }

    free(sobel);
}

int apply_blur_filter_one_iter_col( int width, int height, pixel *p, int size, int threshold )
{
    int j, k ;
    int end = 0 ;

    pixel * new ;
    int n_iter = 0 ;

    /* Allocate array of new pixels */
    new = (pixel *)malloc(width * height * sizeof( pixel ) ) ;

    end = 1 ;
    n_iter++ ;

    int end_loop = height*0.9+size;
    int begin_loop = height/10-size;
    int end_last_loop = height-size;
    int end_mid_loop = width-size;

    int hmu = height - 1;
    int wmu = width - 1;
    int msize = -size;


    #pragma omp parallel default(none) private(j,k) shared(size, begin_loop,p,height,new,end_mid_loop,end_loop,end_last_loop,msize,end, threshold, wmu,hmu)
    {

        // Copy pixels of images in ew 
        #pragma omp for schedule(static,20)
            for(j=0; j<hmu; j++)
            {
                for(k=0; k<wmu; k++)
                {
                    new[CONV_COL(j,k,height)].r = p[CONV_COL(j,k,height)].r ;
                    new[CONV_COL(j,k,height)].g = p[CONV_COL(j,k,height)].g ;
                    new[CONV_COL(j,k,height)].b = p[CONV_COL(j,k,height)].b ;
                }
            }


        #pragma omp for schedule(static,20) 
            /* Apply blur on top part of image (10%) */
            for(j=size; j<begin_loop; j++)
            {
                for(k=size; k<end_mid_loop; k++)
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

                    new[CONV_COL(j,k,height)].r = t_r / ( (2*size+1)*(2*size+1) ) ;
                    new[CONV_COL(j,k,height)].g = t_g / ( (2*size+1)*(2*size+1) ) ;
                    new[CONV_COL(j,k,height)].b = t_b / ( (2*size+1)*(2*size+1) ) ;
                }
            }


        #pragma omp for schedule(static,20)
            /* Copy the middle part of the image */
            for(j= begin_loop; j< end_loop; j++)
            {
                for(k=size; k<end_mid_loop; k++)
                {
                    new[CONV_COL(j,k,height)].r = p[CONV_COL(j,k,height)].r ; 
                    new[CONV_COL(j,k,height)].g = p[CONV_COL(j,k,height)].g ; 
                    new[CONV_COL(j,k,height)].b = p[CONV_COL(j,k,height)].b ; 
                }
            }

        #pragma omp for schedule(static,20) 
            /* Apply blur on the bottom part of the image (10%) */
            for(j=end_loop; j<end_last_loop; j++)
            {
                for(k=size; k<end_mid_loop; k++)
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

                    new[CONV_COL(j,k,height)].r = t_r / ( (2*size+1)*(2*size+1) ) ;
                    new[CONV_COL(j,k,height)].g = t_g / ( (2*size+1)*(2*size+1) ) ;
                    new[CONV_COL(j,k,height)].b = t_b / ( (2*size+1)*(2*size+1) ) ;
                }
            }

        #pragma omp for schedule(static,20) 
            for(j=1; j<hmu; j++)
            {
                for(k=1; k<wmu; k++)
                {

                    float diff_r ;
                    float diff_g ;
                    float diff_b ;

                    diff_r = (new[CONV_COL(j  ,k  ,height)].r - p[CONV_COL(j  ,k  ,height)].r) ;
                    diff_g = (new[CONV_COL(j  ,k  ,height)].g - p[CONV_COL(j  ,k  ,height)].g) ;
                    diff_b = (new[CONV_COL(j  ,k  ,height)].b - p[CONV_COL(j  ,k  ,height)].b) ;

                    if ( diff_r > threshold || -diff_r > threshold 
                            ||
                                diff_g > threshold || -diff_g > threshold
                                ||
                                diff_b > threshold || -diff_b > threshold
                        ) {
                        end = 0 ;
                    }

                    p[CONV_COL(j  ,k  ,height)].r = new[CONV_COL(j  ,k  ,height)].r ;
                    p[CONV_COL(j  ,k  ,height)].g = new[CONV_COL(j  ,k  ,height)].g ;
                    p[CONV_COL(j  ,k  ,height)].b = new[CONV_COL(j  ,k  ,height)].b ;
                }
            }
    }
    
    free (new) ;
    if ( threshold > 0 && !end )
        return 0;
    else
        return 1;
}

/****************************************************************************************************************************************************/

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


void print_array(pixel p[], int size, int width, char *title){
    int i;

    printf("---------- PRINT %s", title);
    for(i = 0; i < size; i++){
        if(i%width == 0)
            printf("\n");
        printf("%d ", p[i].b);
    }
    printf("\n");
}


/****************************************************************************************************************************************************/

// FUNCTIONS TO CREATE INFORMATION OF PARTS

typedef struct img_info
{
    int width, height; 
    int order, order_sub_img; // global # of part, # of part in image
    int image; // # of image 
    int ghost_cells_right, ghost_cells_left;
    int n_columns;
    int rank, rank_left, rank_right;
} img_info;

MPI_Datatype create_column(int width, int height){
    MPI_Datatype COLUMN;
    MPI_Type_vector(height, 3*1, width*3, MPI_INT, &COLUMN); // One column (width of 1 pixel)
    MPI_Type_create_resized(COLUMN, 0, sizeof(pixel), &COLUMN);
    MPI_Type_commit(&COLUMN);
    return COLUMN;
}

void fill_info_part_for_one_image(img_info info_array[],int n_parts, int img_n, int width, int height){

    int standard_n_columns = width / n_parts;
    int last_col = width - standard_n_columns * (n_parts-1);

    int k;
    for (k = 0; k < n_parts; k++){
        int part_global_num = img_n * n_parts + k;

        // GENERAL INFOS
        info_array[part_global_num].height = height;
        info_array[part_global_num].order = part_global_num;
        info_array[part_global_num].order_sub_img = k;
        info_array[part_global_num].image = img_n;

        // OTHER PROCESS INFO
        info_array[part_global_num].rank = k;
        info_array[part_global_num].rank_left = k-1;
        info_array[part_global_num].rank_right = k+1;

        // GHOST PROPERTIES
        int ghost_right = 5;
        int ghost_left = 5;
        int n_columns = standard_n_columns;
        if (k == n_parts - 1){
            ghost_right = 0;
            n_columns = last_col;
            info_array[part_global_num].rank_right = -1;
        } 
        if (k == 0){
            ghost_left = 0;
            info_array[part_global_num].rank_left = -1;
        }
        info_array[part_global_num].ghost_cells_left = ghost_left;
        info_array[part_global_num].ghost_cells_right = ghost_right;
        info_array[part_global_num].n_columns = n_columns;
        info_array[part_global_num].width = ghost_left + ghost_right + n_columns;
    }
}

void fill_pixel_column_pointers_for_one_image(pixel* pixel_array[], pixel *img_pixel, int n_parts, int img_n, img_info infos[]){
    int i; 
    pixel *head = img_pixel;
    for (i=0; i < n_parts; i++){
        int part_global_number = img_n * n_parts + i;
        pixel_array[part_global_number] = head;
        head += infos[part_global_number].n_columns;
    }
}

void fill_tables(img_info info_array[], pixel* pixel_array[], MPI_Datatype datatypes[], animated_gif * img, int n_parts_by_image, int n_images){

    animated_gif image = *img;
    int i;

    #pragma omp parallel default(none) private(i) shared(info_array, pixel_array, datatypes, img, n_parts_by_image, n_images)
    {
        #pragma omp for
            for (i = 0; i < n_images; i++){

                // FILL DATATYPE : Create this image's column datatype (handling different heights)
                datatypes[i] = create_column(image.width[i], image.height[i]);

                // FILL INFO_ARRAY : create all the img_info of the parts of this image
                fill_info_part_for_one_image(info_array, n_parts_by_image, i, image.width[i], image.height[i]);

                // FILL PIXEL ARRAY : 
                fill_pixel_column_pointers_for_one_image( pixel_array, image.p[i], n_parts_by_image, i, info_array );
            }
    }
}

/****************************************************************************************************************************************************/

void call_worker(MPI_Comm local_comm, img_info info_recv, pixel *pixel_recv){
    pixel *pixel_ghost_left, *pixel_ghost_right, *pixel_middle;
    int end_local, end_global;
    int height_recv, width_recv, rank_left, rank_right;
    int n_int_ghost_cells;
    MPI_Status status_left, status_right;

    height_recv = info_recv.height;
    width_recv = info_recv.ghost_cells_left + info_recv.n_columns + info_recv.ghost_cells_right;
    rank_left = info_recv.rank_left;
    rank_right = info_recv.rank_right;
    pixel_ghost_left = pixel_recv;
    pixel_middle = pixel_ghost_left + info_recv.ghost_cells_left * height_recv;
    pixel_ghost_right = pixel_middle + info_recv.n_columns * height_recv;
    n_int_ghost_cells = SIZE_STENCIL * height_recv * sizeof(pixel) / sizeof(int);

    int global_rank, local_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &global_rank);
    MPI_Comm_rank(local_comm, &local_rank);

    apply_gray_filter_one_img(width_recv, height_recv, pixel_recv);
    do{
        end_local = apply_blur_filter_one_iter_col(width_recv, height_recv, pixel_recv, SIZE_STENCIL, 20);
        MPI_Allreduce(&end_local, &end_global, 1, MPI_INT, MPI_LOR, local_comm);
    
        if( !end_global ){
            // Send left ghost cells, receive rigth ghost cells
            if( rank_left != -1 )
                MPI_Send(pixel_ghost_left, n_int_ghost_cells, MPI_INT, rank_left, 0, local_comm);
            if( rank_right != -1 )
                MPI_Recv(pixel_ghost_right, n_int_ghost_cells, MPI_INT, rank_right, MPI_ANY_TAG, local_comm, &status_right);
            
            // Send right ghost cells, receive left ghost cells  
            if( rank_right != -1 )
                MPI_Send(pixel_ghost_right, n_int_ghost_cells, MPI_INT, rank_right, 0, local_comm);
            if( rank_left != -1 )
                MPI_Recv(pixel_ghost_left, n_int_ghost_cells, MPI_INT, rank_left, MPI_ANY_TAG, local_comm, &status_left);
        }

    } while( !end_global);            
    
    apply_sobel_filter_one_img_col(width_recv, height_recv, pixel_recv);
}

void get_heuristic(int *n_rounds, int *n_parts_per_img, int n_process, int n_images){
    // *n_parts_per_img = (n_process > 3) ? 3 : n_process;
    // *n_rounds = (n_process - 1) / n_parts_per_img + 1;


    // n_parts_per_img * n_img_per_rounds = k * n_process
    // n_rounds * n_img_per_round = n_images

    // n_rounds * k * n_process = n_images * n_parts_per_img

    int r = 1, n_parts;

    while(1){
        if (n_images % r == 0){ // if the split is possible
            int n_img_per_round = n_images / r;
            for (n_parts=1; n_parts < n_process+1; n_parts++){ // test for all n_parts
                if ( n_img_per_round * n_parts == n_process ){
                    *n_rounds = r;
                    *n_parts_per_img = n_parts;
                    return;
                }
            }
        }
        r++;
        printf(".");
    }
}


int main( int argc, char ** argv ){
    /*
        n_process >= 2
    */
    // MPI_INIT
    int n_process, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &n_process);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Save performances
    FILE *fp;
    int is_file_performance = 0;
    char * perf_filename ;
    
    // FIRST ALLOCATIONS
    int n_int_img_info = sizeof(img_info) / sizeof(int);
    int n_parts, n_images, n_rounds;
    MPI_Comm local_comm;
    int i;
    img_info *parts_info = NULL;
    pixel **parts_pixel = NULL;
    MPI_Datatype *COLUMNS = NULL;

    struct timeval t1, t2;

    if(argc < 3){
        printf("INVALID ARGUMENT\n ./sobelf input_filename output_filename\n");
        return 0;
    }
    if(argc >= 4){
        is_file_performance = 1;
        perf_filename = argv[3];
    }    
    char *input_filename = argv[1];
    char *output_filename = argv[2];

    animated_gif * image ;

    // CHOOSING THE CUTTING STRATEGIE
    if(rank == 0){

        load_image_from_file(&image, &n_images, input_filename);
        gettimeofday(&t1, NULL);

        // Choose how to split images between process
        get_heuristic(&n_rounds, &n_parts,n_process,n_images);
        printf(" ----> For %d images and %d process, the heuristics found %d rounds of %d images splitted in %d\n", n_images, n_process, n_rounds, n_images/n_rounds, n_parts );

        // Structures needed for splitting data
        parts_info = (img_info *)malloc(n_parts * n_images * sizeof(img_info));
        parts_pixel = (pixel **)malloc(n_parts * n_images * sizeof(pixel *));
        COLUMNS = (MPI_Datatype*)malloc(n_images * sizeof(MPI_Datatype));

        // Fill_info_parts and pixel_arts and columns 
        fill_tables(parts_info,parts_pixel,COLUMNS,image,n_parts, n_images);

        printf("_____INITIALIZATION FINISHED\n");
    }


    // CREATING ALL THE DIFFERENT COMMUNICATORS
    MPI_Bcast(&n_parts, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Comm_split(MPI_COMM_WORLD, rank/n_parts, rank, &local_comm);
    double time = 0;
    
    if(rank == 0){
        
        // Initialize
        printf("_____START SENDING PARTS\n");
        int r,s;
        int n_img_per_round = n_images / n_rounds;


        for(r=0; r < n_rounds; r++){

            int j;
            // SENDING THE IMAGE IN PARTS
            pixel *beg_pixel;
            int n_total_columns;
            for (s=0; s < n_img_per_round; s++){

                j = r*n_img_per_round + s;

                int begin = (s == 0) ? 1 : 0;
                for(i=begin; i<n_parts; i++){
                    int part_n = j * n_parts + i;
                    int sub_part_n = s * n_parts + i;

                    beg_pixel = parts_pixel[part_n] - parts_info[part_n].ghost_cells_left;
                    n_total_columns = parts_info[part_n].width;

                    MPI_Send(&(parts_info[part_n]), n_int_img_info, MPI_INT, sub_part_n, 0, MPI_COMM_WORLD);
                    MPI_Send(beg_pixel, n_total_columns, COLUMNS[j], sub_part_n, 0, MPI_COMM_WORLD);
                }
            }

            // DOING THE ROOT'S PART OF THE JOB

            // Initialize
            MPI_Status status;
            MPI_Request req;
            int root_img = r * n_img_per_round;
            int root_part_n = root_img * n_parts;
            beg_pixel = parts_pixel[root_part_n]; // - parts_info[root_part_n].ghost_cells_left;
            n_total_columns = parts_info[root_part_n].width;

            // Send to itself
            MPI_Isend(beg_pixel, n_total_columns, COLUMNS[root_img], 0, 0, MPI_COMM_SELF, &req);
            int n_pixels_recv = n_total_columns * parts_info[root_part_n].height;
            pixel *pixel_recv = (pixel *)malloc( n_pixels_recv * sizeof(pixel) );
            MPI_Recv(pixel_recv, n_pixels_recv * 3, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_SELF, &status);

            //Working part
            call_worker(local_comm, parts_info[root_part_n], pixel_recv);

            // Receive the job again
            int n_pixels_to_send = parts_info[root_part_n].n_columns * parts_info[root_part_n].height;
            pixel* pixel_middle = pixel_recv; // No ghost_left
            MPI_Isend(pixel_middle, n_pixels_to_send * 3, MPI_INT, 0, status.MPI_TAG, MPI_COMM_SELF, &req);
            MPI_Recv(parts_pixel[root_part_n], parts_info[root_part_n].n_columns, COLUMNS[root_img], 0, MPI_ANY_TAG, MPI_COMM_SELF, &status);


            // RECEIVING THE PARTS 
            img_info info_recv;
            for (j=0; j < n_img_per_round; j++){
                int begin = (j == 0) ? 1 : 0;
                for(i=begin; i<n_parts; i++){
                    
                    int part_number;

                    MPI_Recv(&info_recv, n_int_img_info, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                    part_number = info_recv.order;
                    beg_pixel = parts_pixel[part_number];
                    n_total_columns = info_recv.n_columns;
                    MPI_Recv(beg_pixel, n_total_columns, COLUMNS[info_recv.image], status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
                }
            }
        }
        
        // Mesure performance
        printf("_____FINISHED RECEIVING\n");
        gettimeofday(&t2, NULL);
        double duration = (t2.tv_sec-t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
        printf("Duration of the process %lf (calculus time: %lf)", duration, time);
        if(is_file_performance == 1){
            fp = fopen(perf_filename, "a");
            fprintf(fp, "%lf (n = %d, n_images = %d) for %s\n", duration, n_process, n_images, input_filename) ;
            fclose(fp);
        }

        // Export the gif
        if ( !store_pixels( output_filename, image ) ) { return 0 ; }
        printf("_____EXPORTED_____\n");

        // Stop all process
        int buf = 0;
        MPI_Request req;
        for(i=1; i < n_parts * n_img_per_round; i++)
            MPI_Isend(&buf, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &req);

    }
    else {
        MPI_Status status;
        int n_int_recv;

        img_info info_recv;
        pixel *pixel_recv;

        while(1){

            // Check what is sent
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_INT, &n_int_recv);
            if(n_int_recv == 1) // Signal for stop
                break;

            // Receive header
            MPI_Recv(&info_recv, n_int_img_info, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
            int n_pixels_recv = info_recv.height *(info_recv.ghost_cells_left + info_recv.n_columns + info_recv.ghost_cells_right);

            // Alloc and receive data
            pixel_recv = (pixel *)malloc( n_pixels_recv * sizeof(pixel) );
            MPI_Recv(pixel_recv, n_pixels_recv * 3, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);

            // Work
            call_worker(local_comm, info_recv, pixel_recv);

            // Send back
            pixel *pixel_middle = pixel_recv + info_recv.ghost_cells_left * info_recv.height;
            int n_pixels_to_send = info_recv.n_columns * info_recv.height;
            MPI_Send(&info_recv, n_int_img_info, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD);
            MPI_Send(pixel_middle, n_pixels_to_send * 3, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD);

            free(pixel_recv);
        }
    }
    MPI_Finalize();
    return 1;
}
