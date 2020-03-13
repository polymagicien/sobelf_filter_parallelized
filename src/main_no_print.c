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
#include <omp.h>

#include "gif_lib.h"
#include "utils.h"
#include "gpu.h"

// Make tester
// mpirun -n 1 ./test args
// OMP_NUM_THREADS=1 salloc -n 6 -N 1 mpirun ./sobelf_main images/original/Campusplan-Hausnr.gif images/processed/2.gif

/* Set this macro to 1 to enable debugging information */
#define SOBELF_DEBUG 0
#define SIZE_STENCIL 5

int VERIFY_GIF = 0;


/****************************************************************************************************************************************************/

// FUNCTIONS TO CREATE INFORMATION OF PARTS

typedef struct img_info
{
    int height;
    int order, order_sub_img; // global # of part, # of part in image
    int image; // image # 
    int ghost_cells_right, n_columns, ghost_cells_left, width ; // number of pixels in a width of a part
    int rank, rank_left, rank_right;
} img_info;

MPI_Datatype create_column(int width, int height){
    MPI_Datatype COLUMN;
    MPI_Type_vector(height, 3*1, width*3, MPI_INT, &COLUMN); // One column (width of 1 pixel)
    MPI_Type_create_resized(COLUMN, 0, sizeof(pixel), &COLUMN);
    MPI_Type_commit(&COLUMN);
    return COLUMN;
}

void fill_info_part_for_one_image(img_info info_array[],int n_parts, int parts_done, int img_n, int width, int height){

    int standard_n_columns = width / n_parts;
    int last_col = width - standard_n_columns * (n_parts-1);

    int k;
    for (k = 0; k < n_parts; k++){
        int part_global_num = parts_done + k;

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
        int ghost_right = SIZE_STENCIL;
        int ghost_left = SIZE_STENCIL;
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

void fill_pixel_column_pointers_for_one_image(pixel* pixel_array[], pixel *img_pixel, int n_parts, int parts_done, int img_n, img_info infos[]){
    int i; 
    pixel *head = img_pixel;
    for (i=0; i < n_parts; i++){
        int part_global_number = parts_done + i;
        pixel_array[part_global_number] = head;
        head += infos[part_global_number].n_columns;
    }
}

void fill_tables(img_info info_array[], pixel* pixel_array[], MPI_Datatype datatypes[], animated_gif * img, int n_parts_by_image[], int n_images){

    animated_gif image = *img;
    int i;

    int parts_done = 0;
    // #pragma omp parallel default(none) private(i) shared(info_array, pixel_array, datatypes, img, n_parts_by_image, n_images, image)
    // {
    //     #pragma omp for
            for (i = 0; i < n_images; i++){

                int n_parts_this_img = n_parts_by_image[i];

                // FILL DATATYPE : Create this image's column datatype (handling different heights)
                datatypes[i] = create_column(image.width[i], image.height[i]);

                // FILL INFO_ARRAY : create all the img_info of the parts of this image
                fill_info_part_for_one_image(info_array, n_parts_this_img, parts_done, i, image.width[i], image.height[i]);

                // FILL PIXEL ARRAY : 
                fill_pixel_column_pointers_for_one_image( pixel_array, image.p[i], n_parts_this_img, parts_done, i, info_array );

                // UPDATE PARTS_DONE
                parts_done+= n_parts_this_img;
            }
    // }
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

/****************************************************************************************************************************************************/

void call_worker_one_thread(MPI_Comm local_comm, img_info info_recv, pixel *pixel_recv, int rank){ // Function to handle one part of an image
    int end_local = 1;
    int height_recv = info_recv.height;
    int width_recv = info_recv.width;

    int global_rank, local_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &global_rank);
    MPI_Comm_rank(local_comm, &local_rank);

    //Used in functions to store
    pixel *interm = (pixel *)malloc(width_recv * height_recv * sizeof( pixel ) );

    apply_gray_filter_one_img(width_recv, height_recv, pixel_recv);
    do{
        end_local = 1;
        apply_blur_filter_one_iter_col(width_recv, height_recv, pixel_recv, SIZE_STENCIL, 20, interm, &end_local);
    } while( !end_local);
    apply_sobel_filter_one_img_col(width_recv, height_recv, pixel_recv, interm);
    

    free(interm);
}


void call_worker(MPI_Comm local_comm, img_info info_recv, pixel *pixel_recv, int rank){ // Function to handle one part of an image
    pixel *pixel_ghost_left, *pixel_ghost_right, *pixel_middle, *pixel_middle_plus;
    int end_local, end_global;
    int height_recv, width_recv, rank_left, rank_right;
    int n_int_ghost_cells;
    MPI_Status status_left, status_right;

    end_local = 1;
    height_recv = info_recv.height;
    width_recv = info_recv.width;
    rank_left = info_recv.rank_left;
    rank_right = info_recv.rank_right;
    pixel_ghost_left = pixel_recv;
    pixel_middle = pixel_ghost_left + info_recv.ghost_cells_left * height_recv;
    pixel_ghost_right = pixel_middle + info_recv.n_columns * height_recv;
    pixel_middle_plus = pixel_ghost_right - info_recv.ghost_cells_right * height_recv;
    n_int_ghost_cells = SIZE_STENCIL * height_recv * sizeof(pixel) / sizeof(int);

    int global_rank, local_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &global_rank);
    MPI_Comm_rank(local_comm, &local_rank);

    // Used in functions to store
    pixel *interm = (pixel *)malloc(width_recv * height_recv * sizeof( pixel ) );
    pixel *pixel_gpu = (pixel *)malloc(width_recv * height_recv * sizeof( pixel ) );
    memcpy(pixel_gpu, pixel_recv, width_recv * height_recv * sizeof( pixel ));
    int i;

    #pragma omp parallel default(none) shared(pixel_gpu, i, pixel_recv, height_recv, width_recv, end_local, end_global, interm, rank_left, rank_right, pixel_ghost_left, pixel_middle, pixel_ghost_right, pixel_middle_plus, n_int_ghost_cells, local_comm, ompi_mpi_op_land, ompi_mpi_int, status_right, status_left, rank)
    {   

        apply_gray_filter_one_img(width_recv, height_recv, pixel_recv);
        apply_gray_filter_one_img(width_recv, height_recv, pixel_gpu);

        int counter = 0;
        do{
            #pragma omp barrier
            end_local = 1;
            counter++;
            gpu_part(width_recv, height_recv, pixel_gpu, SIZE_STENCIL, 20, interm, &end_local);
            apply_blur_filter_one_iter_col(width_recv, height_recv, pixel_recv, SIZE_STENCIL, 20, interm, &end_local);

            int blur_not_correct = 0;
            int count = 0;
            for(i=0; i<width_recv * height_recv; i++){
                if(pixel_recv[i].r != pixel_gpu[i].r || pixel_recv[i].g != pixel_gpu[i].g || pixel_recv[i].b != pixel_gpu[i].b ){
                    count++;
                    blur_not_correct = 1;             }
            }
            if(blur_not_correct){
                printf("\tBlur GPU not correct - %d\n", count);
                break;
            }

            #pragma omp barrier
            #pragma omp master
            {
                MPI_Allreduce(&end_local, &end_global, 1, MPI_INT, MPI_LAND, local_comm);
                if( !end_global ){
                    // Send left ghost cells, receive rigth ghost cells
                    if( rank_left != -1 )
                        MPI_Send(pixel_middle, n_int_ghost_cells, MPI_INT, rank_left, 0, local_comm);
                    if( rank_right != -1 )
                        MPI_Recv(pixel_ghost_right, n_int_ghost_cells, MPI_INT, rank_right, MPI_ANY_TAG, local_comm, &status_right);
                    
                    // Send right ghost cells, receive left ghost cells  
                    if( rank_right != -1 )
                        MPI_Send(pixel_middle_plus, n_int_ghost_cells, MPI_INT, rank_right, 0, local_comm);
                    if( rank_left != -1 )
                        MPI_Recv(pixel_ghost_left, n_int_ghost_cells, MPI_INT, rank_left, MPI_ANY_TAG, local_comm, &status_left);
                }
            }
            #pragma omp barrier
        } while( !end_global);
        apply_sobel_filter_one_img_col(width_recv, height_recv, pixel_recv, interm);
        printf("Number of iterations for blur : %d\n", counter);
    }

    // Free struct used in function to store
    free(interm);
}

void call_worker_solo(MPI_Comm local_comm, img_info info_recv, pixel *pixel_recv, int rank){ // Function to handle one part of an image
    pixel *pixel_ghost_left, *pixel_ghost_right, *pixel_middle, *pixel_middle_plus;
    int end_local, end_global;
    int height_recv, width_recv, rank_left, rank_right;
    int n_int_ghost_cells;
    MPI_Status status_left, status_right;

    end_local = 1;
    height_recv = info_recv.height;
    width_recv = info_recv.width;
    rank_left = info_recv.rank_left;
    rank_right = info_recv.rank_right;
    pixel_ghost_left = pixel_recv;
    pixel_middle = pixel_ghost_left + info_recv.ghost_cells_left * height_recv;
    pixel_ghost_right = pixel_middle + info_recv.n_columns * height_recv;
    pixel_middle_plus = pixel_ghost_right - info_recv.ghost_cells_right * height_recv;
    n_int_ghost_cells = SIZE_STENCIL * height_recv * sizeof(pixel) / sizeof(int);

    int global_rank, local_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &global_rank);
    MPI_Comm_rank(local_comm, &local_rank);

    //Used in functions to store
    pixel *interm = (pixel *)malloc(width_recv * height_recv * sizeof( pixel ) );

    apply_gray_filter_one_img(width_recv, height_recv, pixel_recv);

    do{
        end_local = 1;
        apply_blur_filter_one_iter_col(width_recv, height_recv, pixel_recv, SIZE_STENCIL, 20, interm, &end_local);

        MPI_Allreduce(&end_local, &end_global, 1, MPI_INT, MPI_LOR, local_comm);
        if( !end_global ){
            // Send left ghost cells, receive rigth ghost cells
            if( rank_left != -1 )
                MPI_Send(pixel_middle, n_int_ghost_cells, MPI_INT, rank_left, 0, local_comm);
            if( rank_right != -1 )
                MPI_Recv(pixel_ghost_right, n_int_ghost_cells, MPI_INT, rank_right, MPI_ANY_TAG, local_comm, &status_right);
            
            // Send right ghost cells, receive left ghost cells  
            if( rank_right != -1 )
                MPI_Send(pixel_middle_plus, n_int_ghost_cells, MPI_INT, rank_right, 0, local_comm);
            if( rank_left != -1 )
                MPI_Recv(pixel_ghost_left, n_int_ghost_cells, MPI_INT, rank_left, MPI_ANY_TAG, local_comm, &status_left);
        }
    } while( !end_global);
    apply_sobel_filter_one_img_col(width_recv, height_recv, pixel_recv, interm);

    // Free struct used in function to store
    free(interm);
}

void get_heuristic(int *n_rounds, int *n_parts_per_img, int n_process, int n_images){ // First draw of heuristics
    int r = 1, n_parts;

    int test = 0;
    while(test < 1000){
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

void get_heuristic2(int *n_rounds, int *n_parts_per_img, int table_n_img[], int n_process, int n_images, int beta){ // First draw of heuristics
    int r = 1;

    if (n_images < n_process || beta == 0){
        get_heuristic(n_rounds,n_parts_per_img,n_process, n_images);
        for (r = 0; r < n_images; r++)
            table_n_img[r] = *n_parts_per_img;
    } else {
        int number_of_rounds = 0;
        int n_img_last = n_images % n_process;
        int n_img_solo = n_images - n_img_last;
        if (n_img_last != 0)
            get_heuristic(&number_of_rounds,n_parts_per_img,n_process,n_img_last);
        else
            *n_parts_per_img = 1;
        
        number_of_rounds += n_images / n_process;
        for (r=0; r < n_img_solo ; r++)
            table_n_img[r] = 1;
        for (r = n_img_solo; r < n_images; r++)
            table_n_img[r] = *n_parts_per_img;
        *n_rounds = number_of_rounds;
    }
}

void get_threads_heuristics( int *other_tech_threads, int *number_of_pix, int n_rounds, int num_threads,int n_process, int beta, int n_parts_per_img[]){
    if (beta == 0 || n_parts_per_img[0] != 1 || num_threads == 0){
        return;
    } else {
        if (n_rounds -1 > 1){
            int max_img = (n_rounds - 1);
            *number_of_pix = n_rounds;
        }
    }
}

int main( int argc, char ** argv ){

    // MPI_INIT
    int n_process, rank;
    int given;

    MPI_Init_thread(&argc, &argv,3,&given);
    MPI_Comm_size(MPI_COMM_WORLD, &n_process);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int num_threads = 1;
    int beta  = 1;
#ifdef _OPENMP
    #pragma omp parallel
    {
        num_threads = omp_get_num_threads();
    }
#endif

    // Save performances
    int is_file_performance = 0;
    char * perf_filename ;
    
    // FIRST ALLOCATIONS
    int n_int_img_info = sizeof(img_info) / sizeof(int);
    int n_parts, n_images, n_rounds;
    int height = 0;
    int width = 0;
    MPI_Comm local_comm;
    int root_work = 1; // to set if you want that the root process work or not
    int other_tech_threads = 0, pict_simultaneous = 1; // 

    int only_one_process = 0;

    img_info *parts_info = NULL;
    pixel **parts_pixel = NULL;
    MPI_Datatype *COLUMNS = NULL;

    struct timeval t11, t12;

    if(argc < 3){ 
        printf("INVALID ARGUMENT\n ./sobelf input_filename output_filename\n");
        return 1;
    }
    if(argc >= 4){
        is_file_performance = 1;
        perf_filename = argv[3];
    }
    if (argc >=5){
        beta = atoi(argv[4]);
    }
    char *input_filename = argv[1];
    char *output_filename = argv[2];

    animated_gif * image ;
    int i, j;
    // CHOOSING THE CUTTING STRATEGIE
    if(rank == 0){
        
        //printf("Thread-safety asked: %i and given: %i\n",3,given );
        if (root_work == 0)
            n_process--;

        load_image_from_file(&image, &n_images, input_filename);
        height = image->height[0];
        width = image->width[0];
        gettimeofday(&t11, NULL);

        if (!only_one_process){
            // Choose how to split images between process
            int n_parts_per_img[n_images];
            get_heuristic2(&n_rounds, &n_parts, n_parts_per_img, n_process,n_images,beta);
            print_heuristics(n_images, n_process, n_rounds, n_parts_per_img);

            get_threads_heuristics(&other_tech_threads,&pict_simultaneous,n_rounds,num_threads,n_process,beta,n_parts_per_img);


            // Structures needed for splitting data
            parts_info = (img_info *)malloc(n_parts * n_images * sizeof(img_info));
            parts_pixel = (pixel **)malloc(n_parts * n_images * sizeof(pixel *));
            COLUMNS = (MPI_Datatype*)malloc(n_images * sizeof(MPI_Datatype));

            // Fill_info_parts and pixel_arts and columns 
            fill_tables(parts_info,parts_pixel,COLUMNS,image,n_parts_per_img, n_images);
        }

    }

    // CREATING ALL THE DIFFERENT COMMUNICATORS
    MPI_Bcast(&n_parts, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int pseudo_rank = rank - (1-root_work);
    if (pseudo_rank == -1)
        pseudo_rank = 10000;
    MPI_Comm_split(MPI_COMM_WORLD, pseudo_rank/n_parts, pseudo_rank, &local_comm);
    
    if(rank == 0){
        
        // Initialize
        int r;

        MPI_Status status;
        MPI_Request req;
        img_info lol;

        if (only_one_process){

            gettimeofday(&t11, NULL);

            #pragma omp parallel default(none) shared(image)
            {

                printf("Solomode activated\n");
                int i;
                #pragma omp for schedule(static,2) nowait
                for (i=0; i < image->n_images; i++){
                      /* Convert the pixels into grayscale */
                    apply_gray_filter_initial_one_image( image,i ) ;

                    /* Apply blur filter with convergence value */
                    apply_blur_filter_initial_one_image( image, 5, 20,i ) ;

                    /* Apply sobel filter on pixels */
                    apply_sobel_filter_initial_one_image( image,i ) ;
                }
            }

            gettimeofday(&t12, NULL);

            printf_time("total", t11, t12);



        } else {

            // SENDING THE IMAGE IN PARTS

            int parts_done = 0;

            for(r=0; r < n_rounds; r++){
                
                int n_part_to_send =  n_process;
                int root_part =  parts_done;
                img_info *begin_table = parts_info + root_part;
                parts_done++;

                // Send the part_info to the process
                MPI_Scatter(begin_table -(1-root_work), n_int_img_info, MPI_INT,&lol,n_int_img_info, MPI_INT,0,MPI_COMM_WORLD);

                // Sending the images
                int beginning = root_work;
                int ending = n_part_to_send;
                for (j=beginning; j < ending ; j++){
                    pixel *beg_pixel = parts_pixel[root_part + j] - parts_info[root_part + j].ghost_cells_left;
                    MPI_Send(beg_pixel, parts_info[root_part + j].width, COLUMNS[parts_info[root_part + j].image], j + (1 - root_work), 0, MPI_COMM_WORLD);
                    parts_done++;
                }
                
                if (root_work){
                    // Prepare receiving it's own data && Send to itself
                    int n_pixels_recv = parts_info[root_part].width * parts_info[root_part].height;
                    pixel *pixel_recv = (pixel *)malloc( n_pixels_recv * sizeof(pixel) );
                    MPI_Isend(parts_pixel[root_part], parts_info[root_part].width, COLUMNS[parts_info[root_part].image], 0, 0, MPI_COMM_SELF, &req);
                    MPI_Recv(pixel_recv, n_pixels_recv * 3, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_SELF, &status);

                    //Working part
                    call_worker(local_comm, parts_info[root_part], pixel_recv, rank);

                    // Receive the job again
                    int n_pixels_to_send = parts_info[root_part].n_columns * parts_info[root_part].height;
                    MPI_Isend(pixel_recv, n_pixels_to_send * 3, MPI_INT, 0, status.MPI_TAG, MPI_COMM_SELF, &req);
                    MPI_Recv(parts_pixel[root_part], parts_info[root_part].n_columns, COLUMNS[parts_info[root_part].image], 0, MPI_ANY_TAG, MPI_COMM_SELF, &status);
                }

                // RECEIVING THE PARTS 
                //printf("receiving in round %i\n", r);
                for (j=beginning; j < ending; j++){
                    
                    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,&status);
                    int pn = status.MPI_SOURCE - (1-root_work);
                    //printf(" from %i \n", pn);
                    MPI_Recv(parts_pixel[root_part + pn], parts_info[root_part + pn].n_columns, COLUMNS[parts_info[root_part + pn].image], status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
                        
                }
            } 
            
                gettimeofday(&t12, NULL);

            printf_time("total", t11, t12);

            int proc_node = 8/num_threads;
            
            int nodes = n_process/proc_node;
            if (n_process%proc_node!=0)
                nodes++;

            if(is_file_performance == 1){
                FILE *filetow = fopen(perf_filename, "a");
                double duration = (t12.tv_sec-t11.tv_sec)+((t12.tv_usec-t11.tv_usec)/1e6);
                char *name = input_filename;
                fprintf(filetow, "time: %lf; process: %d; threads: %d; node: %d; image: %s; n_images: %d; w: %d; h: %d; beta: %d; \n", duration, n_process, num_threads, nodes, name, n_images, width, height,beta ) ;
                fclose(filetow);
            }

        }

        // test if correct
        if(VERIFY_GIF){
            animated_gif *compared_img;
            int temp, is_ok = 1;
            load_image_from_file(&compared_img, &temp, input_filename);
            printf("Verification image\n");
            apply_gray_filter_initial(compared_img);
            apply_blur_filter_initial(compared_img, SIZE_STENCIL, 20);
            apply_sobel_filter_initial(compared_img);

            for(i=0; i<compared_img->width[0] * compared_img->height[0]; i++){
                int r = compared_img->p[0][i].r;
                int g = compared_img->p[0][i].g;
                int b = compared_img->p[0][i].b;
                int r_ = image->p[0][i].r;
                int g_ = image->p[0][i].g;
                int b_ = image->p[0][i].b;
                if(r != r_ || g != g_ || b != b_){
                    is_ok = 0;
                    int row, col;
                    INV_CONV(&row, &col, i, compared_img->width[0]);
                    printf("\t(%d, %d) \n", row, col);
                    // break;
                }
            }
            if(is_ok)   printf("OK\n");
            else    printf("NOT ok\n");
        }


        // Export the gif
        if ( !store_pixels( output_filename, image ) )
            return 1 ;

        // Stop all process
        
        if (!only_one_process){
            for(i=0; i < n_process + 1-root_work; i++)
                parts_info[i].height = 0;
            MPI_Scatter(parts_info, n_int_img_info, MPI_INT,&lol,n_int_img_info, MPI_INT,0,MPI_COMM_WORLD);
        }
    }
    else {
        MPI_Status status;

        img_info info_recv;
        pixel *pixel_recv, *pixel_middle;

        while(1){

            // Check what is sent
            MPI_Scatter(NULL,0,MPI_INT,&info_recv, n_int_img_info, MPI_INT,0,MPI_COMM_WORLD);
            
            int n_pixels_recv = info_recv.height * info_recv.width;

            if (n_pixels_recv == 0)
                break;

            // Alloc and receive data
            pixel_recv = (pixel *)malloc( n_pixels_recv * sizeof(pixel) );
            MPI_Recv(pixel_recv, n_pixels_recv * 3, MPI_INT,0, 0, MPI_COMM_WORLD, &status);

            // Work
            call_worker(local_comm, info_recv, pixel_recv, rank);

            // Send back
            pixel_middle = pixel_recv + info_recv.ghost_cells_left * info_recv.height;
            int n_pixels_to_send = info_recv.n_columns * info_recv.height;
            MPI_Send(pixel_middle, n_pixels_to_send * 3, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD);

            free(pixel_recv);
        }
    }
    MPI_Finalize();
    return 0;
}
