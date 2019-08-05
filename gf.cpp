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

#include "guided_filter.h"

#ifdef _AOT_HVX_V66_
#include "remote64.h"
#endif

int main(int argc, char** argv)
{
    struct timeval stime, etime;
    {

        int w = atoi(argv[3]);
        int h = atoi(argv[4]);
        Buffer<uint8_t> in_I(nullptr, w, h);
        Buffer<uint8_t> in_P(nullptr, w, h);
        Buffer<uint16_t> div_map(nullptr, DIV_TAB_SIZE);
        Buffer<uint8_t> output(nullptr, w, h);

        #ifdef _AOT_HVX_
        #ifdef _AOT_HVX_V66_
		#pragma weak remote_session_control
		if (remote_session_control)
		{
		    struct remote_rpc_control_unsigned_module data;
		    data.enable = 1;
		    data.domain = CDSP_DOMAIN_ID;
		    remote_session_control(DSPRPC_CONTROL_UNSIGNED_MODULE, (void*)&data, sizeof(data));
		}
        #endif

        // zero-copy buffer
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

        // read input image, here we use same image for I and P
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
            fread(div_map.data(), sizeof(uint16_t), DIV_TAB_SIZE, fptr);
            fclose(fptr);
        }

        int rad = atoi(argv[5]);
        int eps = atof(argv[6]) * 65536;
        printf("converted eps %d\n", eps);

        #ifdef _AOT_HVX_
        // To avoid the cost of powering HVX on in each call of the
        // pipeline, power it on once now. Also, set Hexagon performance to turbo.
        halide_hexagon_set_performance_mode(nullptr, halide_hexagon_power_turbo);
        halide_hexagon_power_hvx_on(nullptr);
        #endif

        gettimeofday(&stime, NULL);
        guided_filter(in_I, in_P, div_map, rad, eps, output);
        gettimeofday(&etime, NULL);

        #ifdef _AOT_HVX_
        // We're done with HVX, power it off, and reset the performance mode
        // to default to save power.
        halide_hexagon_power_hvx_off(nullptr);
        halide_hexagon_set_performance_mode(nullptr, halide_hexagon_power_default);
        #endif

        // save the output image
        {
            FILE *fptr = fopen(argv[7], "wb");
            uint8_t *ptr_out = output.data();
            int stride_out = output.dim(1).stride();
            for(int y = 0; y < h; y++){
                fwrite(ptr_out + y*stride_out, 1, w, fptr);
            }
            fclose(fptr);
        }

        #ifdef _AOT_HVX_
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
