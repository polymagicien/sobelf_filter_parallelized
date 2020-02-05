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

/* ************************************ FILTER FUNCTIONS *********************************** */

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

// When columns are concatenated
#define CONV_COL(l,c,nb_l) \
    (c)*(nb_l)+(l) 

void apply_blur_filter_one_iter_col( int width, int height, pixel *p, int threshold )
{
    int j, k ;
    int end = 0 ;

    pixel * new ;
    n_iter = 0 ;
    width = image->width[i] ;
    height = image->height[i] ;

    /* Allocate array of new pixels */
    new = (pixel *)malloc(width * height * sizeof( pixel ) ) ;

    end = 1 ;
    n_iter++ ;

    // Copy pixels of images in ew 
    for(j=0; j<height-1; j++)
    {
        for(k=0; k<width-1; k++)
        {
            new[CONV_COL(j,k,height)].r = p[i][CONV_COL(j,k,height)].r ;
            new[CONV_COL(j,k,height)].g = p[i][CONV_COL(j,k,height)].g ;
            new[CONV_COL(j,k,height)].b = p[i][CONV_COL(j,k,height)].b ;
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
                    t_r += p[i][CONV_COL(j+stencil_j,k+stencil_k,height)].r ;
                    t_g += p[i][CONV_COL(j+stencil_j,k+stencil_k,height)].g ;
                    t_b += p[i][CONV_COL(j+stencil_j,k+stencil_k,height)].b ;
                }
            }

            new[CONV_COL(j,k,height)].r = t_r / ( (2*size+1)*(2*size+1) ) ;
            new[CONV_COL(j,k,height)].g = t_g / ( (2*size+1)*(2*size+1) ) ;
            new[CONV_COL(j,k,height)].b = t_b / ( (2*size+1)*(2*size+1) ) ;
        }
    }

    /* Copy the middle part of the image */
    for(j=height/10-size; j<height*0.9+size; j++)
    {
        for(k=size; k<width-size; k++)
        {
            new[CONV_COL(j,k,height)].r = p[i][CONV_COL(j,k,height)].r ; 
            new[CONV_COL(j,k,height)].g = p[i][CONV_COL(j,k,height)].g ; 
            new[CONV_COL(j,k,height)].b = p[i][CONV_COL(j,k,height)].b ; 
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
                    t_r += p[i][CONV_COL(j+stencil_j,k+stencil_k,height)].r ;
                    t_g += p[i][CONV_COL(j+stencil_j,k+stencil_k,height)].g ;
                    t_b += p[i][CONV_COL(j+stencil_j,k+stencil_k,height)].b ;
                }
            }

            new[CONV_COL(j,k,height)].r = t_r / ( (2*size+1)*(2*size+1) ) ;
            new[CONV_COL(j,k,height)].g = t_g / ( (2*size+1)*(2*size+1) ) ;
            new[CONV_COL(j,k,height)].b = t_b / ( (2*size+1)*(2*size+1) ) ;
        }
    }

    for(j=1; j<height-1; j++)
    {
        for(k=1; k<width-1; k++)
        {

            float diff_r ;
            float diff_g ;
            float diff_b ;

            diff_r = (new[CONV_COL(j  ,k  ,height)].r - p[i][CONV_COL(j  ,k  ,height)].r) ;
            diff_g = (new[CONV_COL(j  ,k  ,height)].g - p[i][CONV_COL(j  ,k  ,height)].g) ;
            diff_b = (new[CONV_COL(j  ,k  ,height)].b - p[i][CONV_COL(j  ,k  ,height)].b) ;

            if ( diff_r > threshold || -diff_r > threshold 
                    ||
                        diff_g > threshold || -diff_g > threshold
                        ||
                        diff_b > threshold || -diff_b > threshold
                ) {
                end = 0 ;
            }

            p[i][CONV_COL(j  ,k  ,height)].r = new[CONV_COL(j  ,k  ,height)].r ;
            p[i][CONV_COL(j  ,k  ,height)].g = new[CONV_COL(j  ,k  ,height)].g ;
            p[i][CONV_COL(j  ,k  ,height)].b = new[CONV_COL(j  ,k  ,height)].b ;
        }
    }
    free (new) ;
    if ( threshold > 0 && !end )
        return 0;
    else
        return 1;
}



/* ****************************** MPI COMMUNICATION FUNCTIONS  ***************************** */

typedef struct img_info
{
    int width, height; 
    int order, order_sub_img; // global # of part, # of part in image
    int image; // # of image 
    int ghost_cells_right, ghost_cells_left;
    int n_columns;
    int rank, rank_left, rank_right;
    MPI_Comm local_comm;
} img_info;


void get_parts(img_info * info_array[], pixel *pixel_array[], MPI_Datatype datatypes[], MPI_Comm communicators, animated_gif image, int n_rounds, int n_im_per_round, int n_parts_by_image){

    int n_images = image.n_images;
    int i,j,k;
    int n_parts_by_image = n_parts/image.n_images;
    
    // FILL COMMUNICATORS
    MPI_Group world_group;
    MPI_Comm_group(MPI_COMM_WORLD, &world_group);
    MPI_Comm new_comm;
    MPI_Group new_group;
    for( i = 0; i < n_im_per_round; i++){

        int ranks_rep[n_parts_by_image];
        for (j = 0; j < n_parts_by_image; j++)
            ranks_rep[j] = i*n_parts_by_image+j;

        MPI_Group_incl(world_group, n_parts_by_image, ranks_rep, &new_group); 
        MPI_Comm_create(MPI_COMM_WORLD,new_group,&new_comm)
        communicators[i] = new_com;
    }

    // FILL DATATYPES
    MPI_Datatype COLUMN;
    for (i = 0; i < n_images; i++){
        MPI_Type_vector(image.height[i], 3*1, image.width[i]*3, MPI_INT, &COLUMN); // One column (width of 1 pixel)
        MPI_Type_create_resized(COLUMN, 0, 3*sizeof(int), &COLUMN);
        MPI_Type_commit(&COLUMN);
        datatypes[i] = COLUMN;
    }

    // FILL INFO_ARRAY, PIXEL-ARRAY AND DATATYPES
    for (i = 0; i < n_rounds; i++){

        for (j = 0; j < n_im_per_round; j++){
            int n_curr_img = i*n_im_per_round + j;

            int p1 = 0;
            int sub_width = image.width[n_curr_img] / n_parts_by_image;
            info_array[n_curr_img] = (img_info *)malloc(n_parts_by_image * sizeof(img_info));

            for (k = 0; k < n_parts_by_image; k++){
                // GENERAL INFOS
                info_array[n_curr_img][k].width = image.width[n_curr_img];
                info_array[n_curr_img][k].height = image.height[n_curr_img];
                info_array[n_curr_img][k].order = n_curr_img*n_parts_by_image + k;
                info_array[n_curr_img][k].order_sub_img = k;
                info_array[n_curr_img][k].image = n_curr_img;

                // GHOST PROPERTIES
                int p2 = p1 + sub_width;
                int ghost_right = 5;
                int ghost_left = 5;
                if (j == n_parts_by_image - 1){
                    p2 = image.width[i];
                    ghost_right = 0;
                } else if (j == 0){
                    ghost_left = 0;
                }
                info_array[n_curr_img][k].ghost_cells_left = ghost_left;
                info_array[n_curr_img][k].ghost_cells_right = ghost_right;
                info_array[n_curr_img][k].n_columns = p2 - p1;

                // OTHER PROCESS INFO
                info_array[n_curr_img][k].rank = k;
                info_array[n_curr_img][k].rank_left = k-1;
                info_array[n_curr_img][k].rank_right = k+2;
                info_array[n_curr_img][k].local_comm = communicators[j];

                //FILL PIXEL_ARRAY
                pixel_array[info_array[n_curr_img][k].order] = image.p[n_curr_img] + p1;
            }   
        
        }
    }

{/* OLD VERSION
        // Set the image info
        int p1 = 0;
        int sub_width = width / n_parts_by_image;
        for(j = 0; j < n_parts_by_image; j++){
            info_array[i*n_parts_by_image+j].width = image.width[i];
            info_array[i*n_parts_by_image+j].height = image.height[i];
            info_array[i*n_parts_by_image+j].order = i*n_parts_by_image+j;
            info_array[i*n_parts_by_image+j].order_sub_img = j;
            info_array[i*n_parts_by_image+j].image = i;
            info_array[i*n_parts_by_image+j].rank_right = i*n_images+;
            info_array[i*n_parts_by_image+j].rank_left = i;

            // MAKING THE SUBDIVISION
            int p2 = p1 + sub_width;
            int ghost_right = 5;
            int ghost_left = 5;

            if (j == n_parts_by_image - 1){
                p2 = image.width[i];
                ghost_right = 0;
            } else if (j == 0){
                ghost_left = 0;
            }
            
            info_array[i*n_parts_by_image+j].n_columns = p2 - p1;
            info_array[i*n_parts_by_image+j].ghost_cells_left = ghost_left;
            info_array[i*n_parts_by_image+j].ghost_cells_right = ghost_right;
            pixel_array[i*n_parts_by_image+j] = image.p[i] + p1;

            p1 = p2; // update
        }
    }
*/}
}


/* *********************************** UTILITY FUNCTIONS *********************************** */

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

/* ********************************* MPI DECISION FUNCTIONS ******************************** */

int get_n_parts(animated_gif image, int n_process){
    return 1;
}

/* ************************************* MAIN FUNCTION ************************************* */

int main( int argc, char ** argv )
{
    // Loading and saving
    char * input_filename ; 
    char * output_filename ;
    animated_gif * image ;
    int height, width;

    // Measure performance
    FILE *fp; 
    int is_file_performance = 0;
    char * perf_filename ;

    // Time 
    struct timeval t1, t2;
    double duration ;
    
    // Temporary variables
    int i, j, k;
    MPI_Request req;
    MPI_Status status;
    int n_int_rcvd;
    int n_images;

    // Constants
    int n_int_img_info = sizeof( img_info ) / sizeof( int );

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

    if(rank == 0){ // MASTER PROCESS
        int n_total_parts;
        img_info **infos_parts;
        pixel **pixels_parts;
        MPI_Datatype *datatypes;
        MPI_Comm *communicators;

        /****** LOAD FILE INTO image PHASE ******/
        if(load_image_from_file(&image, &n_images, input_filename) == 1)
            return 1;


        /****** OPTIMISATION BEGINS ******/
        gettimeofday(&t1, NULL );
        
        // Decide # processes / img 
        n_total_parts = get_n_parts( image, n_process );
        int n_rounds = 10;
        int n_parts_per_image = 10;
        int n_img_per_round = 2;

        // Malloc the different tables
        infos_parts = ( img_info ** )malloc( n_images * sizeof( img_info * ) );
        pixels_parts = ( pixels_parts ** )malloc( n_total_parts * sizeof( pixel * ) );
        datatypes = ( MPI_Datatype * )malloc( n_images * sizeof( MPI_Datatype ) );
        communicators = ( MPI_Comm * )malloc( n_img_per_round * sizeof( MPI_Comm ) );

        // Preprocessing of the headers, datatypes and communicators 
        get_parts( infos_parts, pixels_parts, datatypes, communicators, image, n_rounds, n_im_per_round, n_parts_per_image);

        // Declare other things
        int size_img_info = sizeof( img_info ) / sizeof( int );
        MPI_Datatype temp_type;
        int n_columns_sent[n_parts_per_image];
        int info_size = sizeof(img_info)/sizeof(int);

        // PROCESSING PHASE
        for (i = 0; i < n_rounds; i++){

            // SEND ONE ROUND
            for (j = 0; j < n_img_per_round; j++){
                int curr_image_number = i*n_img_per_round+j;

                for(k = 0; k < n_parts_per_image; k++){
                    int curr_part_number = curr_image_number*n_parts_per_image + k;
                    img_info * curr_infos = *(infos_parts[curr_image_number][k]);

                    MPI_Isend( curr_infos , size_img_info, MPI_INT, k+j*n_parts_per_image, 0, MPI_COMM_WORLD, &req);
                    MPI_Isend( pixels_parts[curr_part_number], curr_infos->n_columns, datatypes[ curr_image_number], k + j*n_img_per_round, MPI_COMM_WORLD, &req);
                }
                
                {// OLD VERSION SCATTER
                    /*MPI_Scatter(infos_parts,info_size,MPI_INT,&info_img_to_process, info_size,MPI_INT,0,communicators[j] );
                    // allocate the good size to receive the part of the image to the name img_to_process_buf, and p_i_size

                    MPI_Scatterv(&(image.p[curr_image_number]), n_columns_sent, , datatypes[ curr_image_number ],
                            &img_to_process_buf, p_i_size, MPI_INT, 0, communicators[j])
                
                */
                }
           }

            // PROCESS DATA AFFECTED TO THE ROOT
            ///

            for (j = 0; j < n_img_per_round; j++){
                
            }
        }



        /*
        // SENDING PHASE: original images
        for( i = 0; i < n_total_parts; i++ ){
            n_columns_sent = infos_parts[i].n_columns;
            MPI_Isend( infos_parts + i, size_img_info, MPI_INT, i % (n_process - 1) + 1, 0, MPI_COMM_WORLD, &req);
           
        }

        // PROCESSING THE FEW ASSIGNED PARTS
        /*

        

        // RECEIVING PHASE: modified images
        MPI_Request req_array[n_total_parts];
        MPI_Status status_array[n_total_parts];
        img_info info_recv;
        pixel *first_column;
        int n_columns;
        for( i=0; i < n_total_parts; i++ ){
            MPI_Recv( &info_recv, size_img_info, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            first_column = pixels_parts[i] + info_recv.ghost_cells_left;
            temp_type = datatypes[ info_recv.image ];
            MPI_Irecv( first_column, info_recv.n_columns, temp_type, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &(req_array[i]));
        }
        */
        MPI_Waitall(n_total_parts, req_array, status_array);

        // Send "end" signal to processes
        int buf = 0;
        for(i=1; i < n_process; i++)
            MPI_Isend(&buf, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &req);

        /****** OPTIMISATION ENDS ******/

        /****** CREATE GIF AND EXPORTATION ******/
        gettimeofday(&t2, NULL);
        duration = (t2.tv_sec-t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
        if(is_file_performance == 1){
            fp = fopen(perf_filename, "a");
            fprintf(fp, "%lf for %s\n", duration, input_filename) ;
            fclose(fp);
        }

        // export the new gif
        gettimeofday(&t1, NULL);
        if (!store_pixels(output_filename, image))
            return 1;
        gettimeofday(&t2, NULL);
        duration = (t2.tv_sec -t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
        // printf( "Export done in %lf s in file %s\n", duration, output_filename ) ;

        // free allocated
        free(infos_parts);
        for( i = 0; i < n_n_total_parts; i++ ){
            free(pixels_parts[i]);
        }


    } else{ // SLAVE PROCESSES
        img_info info_recv;
        pixel *pixel_recv;
        int n_int_recv;
        int end_local;
        int end_global;

        int height_recv, width_recv, rank_left, rank_right;
        int ghost_left, ghost_right;
        int n_int_column;
        int n_int_ghost_cells;
        MPI_Comm local_comm;
        MPI_Status status_left, status_right;
        // MPI_Request req_left, req_right;
        while(1){

            // Check what is sent
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_INT, &n_int_recv);
            if(n_int_recv == 1) // Signal for stop
                break;

            // Receive and reduce header
            MPI_Recv(&info_recv, n_int_img_info, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
            local_comm = info_recv.local_comm;
            width_recv = info_recv.width;
            height_recv = info_recv.height;
            n_int_recv = width_recv * height_recv * sizeof(pixel) / sizeof(int);
            rank_left = info_recv.rank_left;
            rank_right = info_recv.rank_rigth;
            n_int_column = height_recv * sizeof(pixel) / sizeof(int);
            n_int_ghost_cells = n_int_column * SIZE_STENCIL;
            ghost_left = pixel_recv;
            ghost_right = pixel_recv + ( n_int_recv * sizeof(int) / sizeof( pixel ) ) - SIZE_STENCIL * height;

            // Alloc and receive data
            pixel_recv = (pixel *)malloc( n_int_recv * sizeof(int) );
            MPI_Recv(pixel_recv, n_int_recv, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, &status);

            apply_gray_filter_one_img(width_recv, height_recv, pixel_array);

            do{
                end_local = apply_blur_filter_one_iter_col(width_recv, height_recv, pixel_recv, SIZE_STENCIL, 20);
                MPI_Allreduce(&end_local, &end_global, 1, MPI_INT, MPI_LOR, local_comm);

                if( !end_global ){
                    // Send left ghost cells, receive rigth ghost cells
                    if( rank_left != -1 )
                        MPI_Send(ghost_left, n_int_ghost_cells, MPI_INT, rank_left, 0, local_comm);
                    if( rank_right != -1 )
                        MPI_Recv(ghost_right, n_int_ghost_cells, MPI_INT, rank_right, MPI_ANY_TAG, local_comm, &status_right);
                    
                    // Send right ghost cells, receive left ghost cells  
                    if( rank_right != -1 )
                        MPI_Send(ghost_right, n_int_ghost_cells, MPI_INT, rank_right, 0, local_comm);
                    if( rank_left != -1 )
                        MPI_Recv(ghost_left, n_int_ghost_cells, MPI_INT, rank_left, MPI_ANY_TAG, local_comm, &status_left);
                }

            } while( !end_global);            
            
            apply_sobel_filter_one_img(width_recv, height_recv, pixel_recv);

            info_recv.ghost_cells_left = 0;
            info_recv.ghost_cells_right = 0;

            // Send back
            MPI_Send(info_recv, n_int_img_info, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD);
            MPI_Send(pixel_recv, n_int_rcvd, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD);
            free(pixel_recv);
        }
    }
    MPI_Finalize();
    return 0;
}
