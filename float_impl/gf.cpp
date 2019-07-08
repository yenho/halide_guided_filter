
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <assert.h>
#include "halide_image_io.h"
#include "HalideBuffer.h"

#include "guided_filter.h"
#include "fast_guided_filter.h"

using namespace std;
using namespace Halide::Tools;

#define OCL     0


int main(int argc, char **argv)
{
    struct timeval stime, etime;

    Halide::Runtime::Buffer<uint8_t> input_I = load_image("lena.png");
    #if OCL
    input_I.device_malloc(halide_opencl_device_interface());
    input_I.copy_to_device(halide_opencl_device_interface());
    #endif
    Halide::Runtime::Buffer<uint8_t> input_P = load_image("lena.png");
    #if OCL
    input_P.device_malloc(halide_opencl_device_interface());
    input_P.copy_to_device(halide_opencl_device_interface());
    #endif
    Halide::Runtime::Buffer<uint8_t> output(512, 512);
    #if OCL
    output.device_malloc(halide_opencl_device_interface());
    #endif

    gettimeofday(&stime, NULL);
    guided_filter(input_I, input_P, 8, 250, output);
    #if OCL
    output.copy_to_host();
    #endif
    gettimeofday(&etime, NULL);
    printf("%ld us\n", (etime.tv_sec - stime.tv_sec)*1000000 + (etime.tv_usec - stime.tv_usec));
    save_image(output, "output.png");

    gettimeofday(&stime, NULL);
    fast_guided_filter(input_I, input_P, 8, 8, 250, output);
    #if OCL
    output.copy_to_host();
    #endif
    gettimeofday(&etime, NULL);
    printf("%ld us\n", (etime.tv_sec - stime.tv_sec)*1000000 + (etime.tv_usec - stime.tv_usec));
    save_image(output, "output_fast.png");

    return 0;
}
