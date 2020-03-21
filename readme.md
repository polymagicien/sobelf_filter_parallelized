# IF ISSUE RUNNING
CHANGE path cuda in source "set_env.sh.1" (lib --> lib64)


# Architecture
There is two binaries here. 
 - `test` was made to run a bunch of tests before implementing an improvement / optimisation. You can every function has comments, and you can see which tests were carried out.
 - `main_sobelf` is the final version of what the project was meant to do. It implements all the features we had the time to test and implement.

# Parameters
When testing our code on GIFs, you can choose : 
 - To use GPUs or not
 - To verify that the output image is the correct one (takes more time)

# Make and run
## Make
 - Do not forget to run `source set_env.sh` beforehand.
 - Run `make`

## Run
 - To run `main_sobelf`, please consider the following definition : `OMP_NUM_THREADS=1 salloc -n 1 - N 1 mpirun ./sobelf_main path_to_input_gif path_to_output_gif`
 - To run a test, consider using `./test test_number`

 ## Possible error
  If the make or run step results in an error regarding a cuda library, consider using the given `set_env.sh` and not yours. Otherwise, replace `LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${CUDA_ROOT}/lib/` by `LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${CUDA_ROOT}/lib64/`.



