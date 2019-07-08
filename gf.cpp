#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <math.h>

#include <assert.h>

#include "gf_cfg.h"

//HALIDE
#include "HalideBuffer.h"
using Halide::Runtime::Buffer;

#ifndef __ANDROID__
#include "halide_image_io.h"
using namespace Halide::Tools;
#endif

#include "guided_filter.h"


int main(int argc, char** argv)
{
    struct timeval stime, etime;
    {
        #if 0
        Buffer<uint8_t> in_I = Halide::Tools::load_image(argv[1]);
        Buffer<uint8_t> in_P = Halide::Tools::load_image(argv[2]);
        Buffer<uint8_t> output(in_I.width(), in_I.height());
        int rad = atoi(argv[3]);
        int eps = atoi(argv[4]);

        gettimeofday(&stime, NULL);
        guided_filter(in_I, in_P, rad, eps, output);
        gettimeofday(&etime, NULL);
        save_image(output, argv[5]);
        #else
        int w = atoi(argv[3]);
        int h = atoi(argv[4]);
        Buffer<uint8_t> in_I(nullptr, w, h);
        Buffer<uint8_t> in_P(nullptr, w, h);
        Buffer<uint16_t> div_map(nullptr, 512);
        Buffer<uint8_t> output(nullptr, w, h);

        #ifdef __ANDROID__
        printf("ANDROID HEXAGON buffer allocation\n");
        in_I.device_malloc(halide_hexagon_device_interface());
        in_P.device_malloc(halide_hexagon_device_interface());
        div_map.device_malloc(halide_hexagon_device_interface());
        output.device_malloc(halide_hexagon_device_interface());
        #else
        in_I.allocate();
        in_P.allocate();
        div_map.allocate();
        output.allocate();
        #endif

        {
            FILE *fptr = fopen(argv[1], "rb");
            uint8_t *in_img = (uint8_t*)malloc(w*h);
            fread(in_img, 1, w*h, fptr);
            uint8_t *ptr_I = in_I.data();
            int stride_I = in_I.dim(1).stride();
            uint8_t *ptr_P = in_P.data();
            int stride_P = in_P.dim(1).stride();
            printf("stride I[%d] P[%d]\n", stride_I, stride_P);
            for(int y = 0; y < h; y++){
                memcpy(ptr_I + stride_I*y, in_img + w*y, w);
                memcpy(ptr_P + stride_P*y, in_img + w*y, w);
            }
            free(in_img);
            fclose(fptr);

            fptr = fopen(argv[2], "rb");
            fread(div_map.data(), sizeof(uint16_t), 512, fptr);
            fclose(fptr);
        }

        int rad = atoi(argv[5]);
        int eps = atoi(argv[6]);

        #ifdef __ANDROID__
        // To avoid the cost of powering HVX on in each call of the
        // pipeline, power it on once now. Also, set Hexagon performance to turbo.
        halide_hexagon_set_performance_mode(nullptr, halide_hexagon_power_turbo);
        halide_hexagon_power_hvx_on(nullptr);
        #endif

        gettimeofday(&stime, NULL);
        guided_filter(in_I, in_P, div_map, rad, eps, output);
        gettimeofday(&etime, NULL);

        #ifdef __ANDROID__
        // We're done with HVX, power it off, and reset the performance mode
        // to default to save power.
        halide_hexagon_power_hvx_off(nullptr);
        halide_hexagon_set_performance_mode(nullptr, halide_hexagon_power_default);
        #endif

        {
            FILE *fptr = fopen(argv[7], "wb");
            uint8_t *ptr_out = output.data();
            int stride_out = output.dim(1).stride();
            for(int y = 0; y < h; y++){
                //memcpy(out_img + y*w, ptr_out + stride_out, w);
                fwrite(ptr_out + y*stride_out, 1, w, fptr);
            }
            fclose(fptr);
        }
        #endif

        #ifdef __ANDROID__
        printf("ANDROID HEXAGON buffer release\n");
        in_I.device_free();
        in_P.device_free();
        div_map.device_free();
        output.device_free();
        #else
        in_I.deallocate();
        in_P.deallocate();
        div_map.deallocate();
        output.deallocate();
        #endif
    }

    printf("Halide Guided Filter: %ld us\n", (etime.tv_sec - stime.tv_sec)*1000000 + (etime.tv_usec - stime.tv_usec));

    return 0;
}
