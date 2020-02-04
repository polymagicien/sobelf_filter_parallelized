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


int apply_blur_filter( int width, int height, pixel *p, int size, int threshold, int *pixel_iterated)
{
    int j, k ;
    int end = 0 ;
    int n_iter = 0 ;

    pixel * new ;
    n_iter = 0 ;
    /* Allocate array of new pixels */
    new = (pixel *)malloc(width * height * sizeof( pixel ) ) ;

    *pixel_iterated = 0;

    /* Perform at least one blur iteration */
    do
    {
        end = 1 ;
        n_iter++ ;

        /* Apply blur on top part of image (10%) */
        for(j=size; j<height/10-size; j++)
        {
            for(k=size; k<width-size; k++)
            {
                if(n_iter == 1)
                    *pixel_iterated += 1;
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

        /* Apply blur on the bottom part of the image (10%) */
        for(j=height*0.9+size; j<height-size; j++)
        {
            for(k=size; k<width-size; k++)
            {
                if(n_iter == 1)
                    *pixel_iterated += 1;
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

        for(j=size; j<height/10-size; j++)
        {
            for(k=size; k<width-size; k++)
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

        for(j=height*0.9+size; j<height-size; j++)
        {
            for(k=size; k<width-size; k++)
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
    return n_iter;
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

int conv(int i, int j, int width){
    return i*width + j;
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


int test_rotation( int argc, char **argv )
{
    // Time 
    struct timeval t1, t2;
    double duration ;
    
    // Temporary variables
    int i, j, k;

    // Init MPI
    int n_process, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &n_process);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    srand48(6);

    if(rank == 0){
        char *saving_filename = "test_results/result.txt";
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

void test_filters( int argc, char **argv )
{

    // printf("------------------\n");
    // printf(" %s \n %s \n %s \n", argv[0], argv[1], argv[2]);
    // printf("------------------\n");

    if( argc < 2 )
        return;

    char *input_filename = argv[1];
    char *output_filename = ( argc > 2 )? argv[2] : NULL;
    int n_images;
    animated_gif * image ;
    FILE *fp; 

    // Time 
    struct timeval t1, t2;
    double duration ;
    double grey_per_pixel, blur_per_pixel, sobel_per_pixel;

    // Loops
    int k;

    // Image
    int width, height;
    pixel *pixel_array; 

    int pixel_iterated;
    
    load_image_from_file(&image, &n_images, input_filename);

    // TEST DELTA_T(FILTER) / IMG
    printf("%d\n", n_images);
    for( k = 0; k < n_images; ++k){
        height = image->height[k];
        width = image->width[k];
        pixel_array = image->p[k];
        
        gettimeofday(&t1, NULL);
        apply_gray_filter_one_img(width, height, pixel_array);
        gettimeofday(&t2, NULL);
        duration = (t2.tv_sec-t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
        grey_per_pixel = (duration*10000)/(width*height);
        printf("Grey : %lf\n", grey_per_pixel);

        gettimeofday(&t1, NULL);
        int n_iter = apply_blur_filter(width, height, pixel_array, 5, 10, &pixel_iterated);
        gettimeofday(&t2, NULL);
        duration = (t2.tv_sec-t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
        blur_per_pixel = (pixel_iterated > 0) ? (duration*10000)/pixel_iterated : 0;
        printf("blur : %lf\n", blur_per_pixel);

        gettimeofday(&t1, NULL);
        apply_sobel_filter_one_img(width, height, pixel_array);
        gettimeofday(&t2, NULL);
        duration = (t2.tv_sec-t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
        sobel_per_pixel = (duration*10000)/(width*height);
        printf("Sobel : %lf\n", grey_per_pixel);

        if(output_filename != NULL && blur_per_pixel > 0){
            double total_blur = grey_per_pixel + blur_per_pixel + sobel_per_pixel;
            double total_other = grey_per_pixel + sobel_per_pixel;
            fp = fopen(output_filename, "a");
            fprintf(fp, "%lf ---- %d     %s \n", total_blur / total_other, n_iter, input_filename);
            fclose(fp);
        }
        // printf("%d\n", k+1);
    }
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


void test_mpi_vector(int argc, char **argv){

    // MPI
    int n_process, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &n_process);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status status;
    // MPI_Request req;

    printf("rank : %d\n", rank);

    // Loop
    int i;
    
    int size_rcvd;
    int height = 11;
    int width = 9;
    int size = width * height;

    if(rank == 0){

        // Initialisation
        pixel *pixel_array = (pixel *)malloc(size * sizeof(pixel));
        srand48(11);
        for(i = 0; i < size; i++){
            pixel_array[i].b = i;
            pixel_array[i].r = i;
            pixel_array[i].g = i;
        }

        // Declare type
        MPI_Datatype COLUMN;
        MPI_Type_vector(height, 3*1, width*3, MPI_INT, &COLUMN); // One column (width of 1 pixel)
        MPI_Type_create_resized(COLUMN, 0, 3*sizeof(int), &COLUMN);
        MPI_Type_commit(&COLUMN);


        print_array(pixel_array, size, width, "BEFORE");

        // Sending
        MPI_Send(pixel_array+2, 4, COLUMN, 1, 0, MPI_COMM_WORLD);
        MPI_Recv(pixel_array+3, 2, COLUMN, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        print_array(pixel_array, size, width, "AFTER");    

    } else {
        // Get size
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &size_rcvd);
        // printf("Count : %d\n", size_rcvd); // --> Prints the right number of ints

        pixel array[size_rcvd/3];
        MPI_Recv(array, size_rcvd, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        for(i = 0; i < size_rcvd/3; i++){
            printf("%d ", array[i].b);
            array[i].b += 100;
            array[i].g += 100;
            array[i].r += 100;
        }
        printf("\n");

        int new_size = size_rcvd - 2 * height * sizeof(pixel) / sizeof(int);
        MPI_Send(array + height, new_size, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }


    MPI_Finalize();
}

void test_send_huge_img(int argc, char **argv){

    // MPI
    int n_process, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &n_process);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status status;
    // MPI_Request req;

    printf("rank : %d\n", rank);

    // Loop
    int i;

    // Time 
    struct timeval t1, t2;
    double duration;

    
    int size_rcvd;
    int height = 1000;
    int width = 1000;
    int size = width * height;

    if(rank == 0){

        // Initialisation
        pixel *pixel_array = (pixel *)malloc(size * sizeof(pixel));
        // srand48(11);
        for(i = 0; i < size; i++){
            pixel_array[i].b = i;
            pixel_array[i].r = i;
            pixel_array[i].g = i;
        }

        
        gettimeofday(&t1, NULL);

        MPI_Send(pixel_array, size * 3, MPI_INT, 1, 0, MPI_COMM_WORLD);

        gettimeofday(&t2, NULL);
        duration = (t2.tv_sec-t1.tv_sec)+((t2.tv_usec-t1.tv_usec)/1e6);
        printf("Send and receive done in %lf s\n", duration);

        
        MPI_Recv(pixel_array, size * 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

    } else {
        // Get size
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &size_rcvd);
        // printf("Count : %d\n", size_rcvd); // --> Prints the right number of ints

        
        pixel *array = (pixel *)malloc(size_rcvd / 3 * sizeof(pixel));
        MPI_Recv(array, size_rcvd, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        MPI_Send(array, size_rcvd, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }


    MPI_Finalize();
}


// Main entry point
int main( int argc, char ** argv )
{
    int fun = atoi(argv[1]);
    // printf("%d\n", fun);

    switch( fun ){
        case 1:
            test_rotation(argc, argv + 1);
            break;
        case 2:
            test_filters(argc, argv + 1);
            break;
        case 3:
            test_mpi_vector(argc, argv + 1);
            break;
            break;
        case 4:
            test_send_huge_img(argc, argv + 1);
            break;
        default:
            printf("Wrong argument\n");
            break;
    }
    return 0;
}