SRC_DIR=src
HEADER_DIR=include
OBJ_DIR=obj

# CC=gcc
CC=mpicc
CFLAGS=-Wall -O3 -I$(HEADER_DIR) -fopenmp
LDFLAGS=-lm

# COMPILE main
SRC_main= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	main.c \
	openbsd-reallocarray.c \
	quantize.c

OBJ_main= $(SRC_main:%.c=obj/%.o)

SRC_main_nc= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	main_no_print.c \
	openbsd-reallocarray.c \
	quantize.c \
	gpu.cu

OBJ_main_nc= $(OBJ_DIR)/dgif_lib.o \
	$(OBJ_DIR)/egif_lib.o \
	$(OBJ_DIR)/gif_err.o \
	$(OBJ_DIR)/gif_font.o \
	$(OBJ_DIR)/gif_hash.o \
	$(OBJ_DIR)/gifalloc.o \
	$(OBJ_DIR)/main_no_print.o \
	$(OBJ_DIR)/openbsd-reallocarray.o \
	$(OBJ_DIR)/quantize.o \
	$(OBJ_DIR)/gpu.o

# OBJ_main_nc= $(SRC_main_nc:%.c=obj/%.o)

# COMPILE main_init_openMP
SRC_init_openMP= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	main_init_openMP.c \
	openbsd-reallocarray.c \
	quantize.c

OBJ_init_openMP= $(SRC_init_openMP:%.c=obj/%.o)

# COMPILE main_init_openMP
SRC_initial= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	main_initial.c \
	openbsd-reallocarray.c \
	quantize.c

OBJ_initial= $(SRC_initial:%.c=obj/%.o)
	
# COMPILE test
SRC_test= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	test.c \
	openbsd-reallocarray.c \
	quantize.c

OBJ_test= $(SRC_test:%.c=obj/%.o)

# COMPILE main_paral_per_img_static_alloc_without_copy
SRC_img_without_copy= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	main_paral_per_img_static_alloc_without_copy.c \
	openbsd-reallocarray.c \
	quantize.c

OBJ_img_without_copy= $(SRC_img_without_copy:%.c=obj/%.o)

#  COMPILE main_paral_per_img_static_alloc
SRC_img= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	main_paral_per_img_static_alloc.c \
	openbsd-reallocarray.c \
	quantize.c

OBJ_img= $(SRC_img:%.c=obj/%.o)

# Compile main_paral_columns_mpi_working
SRC_columns_mpi= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	main_paral_columns_mpi_working.c \
	openbsd-reallocarray.c \
	quantize.c

OBJ_columns_mpi= $(SRC_columns_mpi:%.c=obj/%.o)


all: $(OBJ_DIR) sobelf_main sobelf_img_without_copy sobelf_img sobelf_columns_mpi sobelf_openMP sobelf_main_nc

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $^

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cu
	nvcc -o $@ $^

sobelf_main:$(OBJ_main)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

sobelf_initial:$(OBJ_initial)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

sobelf_main_nc:$(OBJ_main_nc)
	$(CC) $(CFLAGS) -L/usr/local/cuda/lib64 -lcudart -o $@ $^ $(LDFLAGS)


sobelf_img_without_copy:$(OBJ_img_without_copy)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

sobelf_img:$(OBJ_img)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

sobelf_columns_mpi:$(OBJ_columns_mpi)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

sobelf_openMP:$(OBJ_init_openMP)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	# $(CC) $(CFLAGS) -o $@ -fopenmp $^ $(LDFLAGS)




test:$(OBJ_test)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f sobelf_main $(OBJ_main)
	rm -f sobelf_img $(OBJ_img)
	rm -f sobelf_img_without_copy $(OBJ_img_without_copy)
	rm -f sobelf_columns_mpi $(OBJ_columns_mpi)
