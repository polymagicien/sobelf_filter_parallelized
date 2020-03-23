SRC_DIR=src
HEADER_DIR=include
OBJ_DIR=obj

# CC=gcc
CC=mpicc
CFLAGS=-Wall -O3 -I$(HEADER_DIR) -fopenmp
LDFLAGS=-lm

# COMPILE main.c
SRC_main= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	utils.c \
	gpu.cu \
	main.c \
	openbsd-reallocarray.c \
	quantize.c

OBJ_main= $(OBJ_DIR)/quantize.o \
	$(OBJ_DIR)/gpu.o \
	$(OBJ_DIR)/dgif_lib.o \
	$(OBJ_DIR)/egif_lib.o \
	$(OBJ_DIR)/gif_err.o \
	$(OBJ_DIR)/gif_font.o \
	$(OBJ_DIR)/gif_hash.o \
	$(OBJ_DIR)/gifalloc.o \
	$(OBJ_DIR)/utils.o \
	$(OBJ_DIR)/main.o \
	$(OBJ_DIR)/openbsd-reallocarray.o \


# COMPILE main_nc.c
SRC_main_nc= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	utils.c \
	gpu.cu \
	main_no_print.c \
	openbsd-reallocarray.c \
	quantize.c 

OBJ_main_nc= $(OBJ_DIR)/quantize.o \
	$(OBJ_DIR)/gpu.o \
	$(OBJ_DIR)/dgif_lib.o \
	$(OBJ_DIR)/egif_lib.o \
	$(OBJ_DIR)/gif_err.o \
	$(OBJ_DIR)/gif_font.o \
	$(OBJ_DIR)/gif_hash.o \
	$(OBJ_DIR)/gifalloc.o \
	$(OBJ_DIR)/utils.o \
	$(OBJ_DIR)/main_no_print.o \
	$(OBJ_DIR)/openbsd-reallocarray.o \


# COMPILE main_initial
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


all: $(OBJ_DIR) sobelf_main sobelf_main_nc

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $^

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cu
	nvcc -O3 -I$(HEADER_DIR) -I/usr/local/openmpi/include -c -o $@ $^

sobelf_main:$(OBJ_main)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -L/usr/local/cuda/lib64 -lcudart

sobelf_initial:$(OBJ_initial)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

sobelf_main_nc:$(OBJ_main_nc)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -L/usr/local/cuda/lib64 -lcudart

test:$(OBJ_test)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f sobelf_main $(OBJ_main)
	rm -f sobelf_main_nc $(OBJ_main_nc)
	rm -f test $(OBJ_test)
	rm -f sobelf_initial $(OBJ_initial)
