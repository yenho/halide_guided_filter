
#include "Halide.h"
#include <stdio.h>

using namespace Halide;
using namespace Halide::ConciseCasts;

/*
 * gauss_down4 -- applies a 3x3 integer gauss kernel and downsamples an image by 4 in
 * one step.
 */
Func gauss_down4(Func input) {

    Func output;
    Func k;

    Var x, y;
    RDom r(-2, 5, -2, 5);

    // gaussian kernel

    k(x, y) = 0;

    k(-2,-2) = 2; k(-1,-2) =  4; k(0,-2) =  5; k(1,-2) =  4; k(2,-2) = 2;
    k(-2,-1) = 4; k(-1,-1) =  9; k(0,-1) = 12; k(1,-1) =  9; k(2,-1) = 4;
    k(-2, 0) = 5; k(-1, 0) = 12; k(0, 0) = 15; k(1, 0) = 12; k(2, 0) = 5;
    k(-2, 1) = 4; k(-1, 1) =  9; k(0, 1) = 12; k(1, 1) =  9; k(2, 1) = 4;
    k(-2, 2) = 2; k(-1, 2) =  4; k(0, 2) =  5; k(1, 2) =  4; k(2, 2) = 2;

    // output with applied kernel and stride 4

    output(x, y) = f32(sum(u32(input(4*x + r.x, 4*y + r.y) * k(r.x, r.y))) / 159);

    ///////////////////////////////////////////////////////////////////////////
    // schedule
    ///////////////////////////////////////////////////////////////////////////

    k.compute_root().parallel(y).parallel(x);

    //output.compute_root().parallel(y).vectorize(x, 16);
    Var xo, xi, yo, yi, tidx;
    output.compute_root().tile(x, y, xo, yo, xi, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, tidx).parallel(tidx);

    return output;
}

Expr bilinear(Func src, Expr x, Expr y){
    Expr x0 = cast<int>(x);
    Expr y0 = cast<int>(y);
    Expr xR   = x - x0;
    Expr yR   = y - y0;
    Expr f00  = src(x0, y0);
    Expr f01  = src(x0, y0 + 1);
    Expr f10  = src(x0 + 1, y0);
    Expr f11  = src(x0 + 1, y0 + 1);
    Expr a1   = f00;
    Expr a2   = f10 - f00;
    Expr a3   = f01 - f00;
    Expr a4   = f00 + f11 - f10 - f01;
    return a1 + a2 * xR + a3 * yR + a4 * xR * yR;
}


Func BilinearScale(Func input, Expr w_factor, Expr h_factor) {
    Func scale("scale");
    Var x,y,c;
 
    Expr x_lower = cast<int>(x * w_factor);
    Expr y_lower = cast<int>(y * h_factor);
    Expr s = (x * w_factor) - x_lower;
    Expr t = (y * h_factor) - y_lower;

    scale(x,y) =  (
                        (1-s) * (1-t) * f32(input(x_lower, y_lower))  +
                        s * (1-t)     * f32(input(x_lower+1, y_lower))  +
                        (1-s) * t     * f32(input(x_lower, y_lower+1))  + 
                        s * t         * f32(input(x_lower+1, y_lower+1))

                    );
    /*
    #if 1
    //scale.compute_root().parallel(y).vectorize(x, 4);
    #else
    Var xo, xi, yo, yi, tidx;
    scale.compute_root().tile(x, y, xo, yo, xi, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, tidx).parallel(tidx);
    #endif
    */

    return scale;
}

class GuidedFilter : public Generator<GuidedFilter> 
{
public:
    Input<Buffer<>> input_I{"input_I", 2};
    Input<Buffer<>> input_P{"input_P", 2};
    Input<int32_t> radius{"radius"};
    Input<float> eps{"eps"};
    Output<Func> output{"output", 2};
    void generate(){
        Func clamp_I = BoundaryConditions::repeat_edge(input_I);
        Func clamp_P = BoundaryConditions::repeat_edge(input_P);

        RDom box(-radius, (radius*2+1), -radius, (radius*2+1));
        Expr area = f32((radius*2+1)*(radius*2+1));

        #if 0
        Expr val_I = f32(clamp_I(x + box.x, y + box.y));
        Expr val_P = f32(clamp_P(x + box.x, y + box.y));
        Expr mean_I = sum(val_I)/area;
        Expr mean_P = sum(val_P)/area;
        Expr corr_I = sum(val_I*val_I)/area;
        Expr corr_IP = sum(val_I*val_P)/area;
        Tuple sums = Tuple(mean_I, mean_P, corr_I, corr_IP);
        Expr var_I = sums[2] - sums[0]*sums[0];
        Expr cov_IP = sums[3] - sums[0]*sums[1];

        Expr a = cov_IP/(var_I + eps);
        Expr b = sums[1] - a*sums[0];
        #else
        RDom r(-radius, (radius*2+1));
        Expr val_I = f32(clamp_I(x + r, y));
        Expr val_P = f32(clamp_P(x + r, y));
        Expr mean_I = sum(val_I);
        Expr mean_P = sum(val_P);
        Expr corr_I = sum(val_I*val_I);
        Expr corr_IP = sum(val_I*val_P);
        sums_x(x, y) = Tuple(mean_I, mean_P, corr_I, corr_IP);
        Tuple sums = Tuple(
                            sum(sums_x(x, y+r)[0])/area, 
                            sum(sums_x(x, y+r)[1])/area, 
                            sum(sums_x(x, y+r)[2])/area, 
                            sum(sums_x(x, y+r)[3])/area
                        );
        Expr var_I = sums[2] - sums[0]*sums[0];
        Expr cov_IP = sums[3] - sums[0]*sums[1];

        Expr a = cov_IP/(var_I + eps);
        Expr b = sums[1] - a*sums[0];
        #endif
        a_b(x, y) = Tuple(a, b);

        #if 0
        mean_ab(x, y) = Tuple(
                            sum(a_b(x + box.x, y + box.y)[0])/area, 
                            sum(a_b(x + box.x, y + box.y)[1])/area
                        );
        #else
        mean_ab_x(x, y) = Tuple(
                            sum(a_b(x + r, y)[0]), 
                            sum(a_b(x + r, y)[1])
                        );
        mean_ab(x, y) = Tuple(
                            sum(mean_ab_x(x, y + r)[0])/area, 
                            sum(mean_ab_x(x, y + r)[1])/area
                        );
        #endif
        output(x, y) = cast(output.type(), mean_ab(x, y)[0]*f32(clamp_I(x, y)) + mean_ab(x, y)[1]);
    }

    void schedule(){
        if(auto_schedule){
            input_I.dim(0).set_bounds_estimate(0, 512);
            input_I.dim(1).set_bounds_estimate(0, 512);
            //input_P
            input_P.dim(0).set_bounds_estimate(0, 512);
            input_P.dim(1).set_bounds_estimate(0, 512);
            //radius
            radius.set_estimate(8);
            //eps
            eps.set_estimate(250);
            //output
            output.estimate(x, 0, 512);
            output.estimate(y, 0, 512);
        }
        else{
            Var xo, xi, yo, yi, idx;
            sums_x.compute_root().tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
            a_b.compute_root().tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
            mean_ab_x.compute_root().tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
            mean_ab.compute_root().tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
            output.tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
        }
    }
private:
    Var x, y;
    Func a_b, mean_ab;
    Func sums_x, mean_ab_x;
};
HALIDE_REGISTER_GENERATOR(GuidedFilter, guided_filter);

class FastGuidedFilter : public Generator<FastGuidedFilter> 
{
public:
    Input<Buffer<>> input_I{"input_I", 2};
    Input<Buffer<>> input_P{"input_P", 2};
    Input<int32_t> radius{"radius"};
    Input<int32_t> down{"down"};
    Input<float> eps{"eps"};
    Output<Func> output{"output", 2};
    void generate(){
        Func clamp_I = BoundaryConditions::repeat_edge(input_I);
        Func clamp_P = BoundaryConditions::repeat_edge(input_P);

        down4_I = BilinearScale(clamp_I, down, down);
        down4_P = BilinearScale(clamp_P, down, down);

        Expr down_r = max(radius/down, 1);

        RDom box(-down_r, (down_r*2+1), -down_r, (down_r*2+1));
        Expr area = f32((down_r*2+1)*(down_r*2+1));

        #if 0
        Expr val_I = f32(down4_I(x + box.x, y + box.y));
        Expr val_P = f32(down4_P(x + box.x, y + box.y));
        Expr mean_I = sum(val_I)/area;
        Expr mean_P = sum(val_P)/area;
        Expr corr_I = sum(val_I*val_I)/area;
        Expr corr_IP = sum(val_I*val_P)/area;
        Expr var_I = corr_I - mean_I*mean_I;
        Expr cov_IP = corr_IP - mean_I*mean_P;

        Expr a = cov_IP/(var_I + eps);
        Expr b = mean_P - a*mean_I;
        #else
        RDom r(-down_r, (down_r*2+1));
        Expr val_I = f32(down4_I(x + r, y));
        Expr val_P = f32(down4_I(x + r, y));
        Expr mean_I = sum(val_I);
        Expr mean_P = sum(val_P);
        Expr corr_I = sum(val_I*val_I);
        Expr corr_IP = sum(val_I*val_P);
        sums_x(x, y) = Tuple(mean_I, mean_P, corr_I, corr_IP);
        Tuple sums = Tuple(
                            sum(sums_x(x, y+r)[0])/area, 
                            sum(sums_x(x, y+r)[1])/area, 
                            sum(sums_x(x, y+r)[2])/area, 
                            sum(sums_x(x, y+r)[3])/area
                        );
        Expr var_I = sums[2] - sums[0]*sums[0];
        Expr cov_IP = sums[3] - sums[0]*sums[1];

        Expr a = cov_IP/(var_I + eps);
        Expr b = sums[1] - a*sums[0];
        #endif
        a_b(x, y) = Tuple(a, b);

        #if 0
        mean_ab(x, y) = Tuple(
                            sum(a_b(x + box.x, y + box.y)[0])/area, 
                            sum(a_b(x + box.x, y + box.y)[1])/area
                        );
        #else
        mean_ab_x(x, y) = Tuple(
                            sum(a_b(x + r, y)[0]), 
                            sum(a_b(x + r, y)[1])
                        );
        mean_ab(x, y) = Tuple(
                            sum(mean_ab_x(x, y + r)[0])/area, 
                            sum(mean_ab_x(x, y + r)[1])/area
                        );
        #endif

        mean_a(x, y) = mean_ab(x, y)[0];
        mean_b(x, y) = mean_ab(x, y)[1];

        Expr up_factor = 1.0f/f32(down);
        up_mean_a = BilinearScale(mean_a, up_factor, up_factor);
        up_mean_b = BilinearScale(mean_b, up_factor, up_factor);

        output(x, y) = cast(output.type(), up_mean_a(x, y)*f32(clamp_I(x, y)) + up_mean_b(x, y));
    }
    void schedule(){
        if(auto_schedule){
            //input_I
            input_I.dim(0).set_bounds_estimate(0, 512);
            input_I.dim(1).set_bounds_estimate(0, 512);
            //input_P
            input_P.dim(0).set_bounds_estimate(0, 512);
            input_P.dim(1).set_bounds_estimate(0, 512);
            //radius
            radius.set_estimate(8);
            //down
            down.set_estimate(8);
            //eps
            eps.set_estimate(250);
            //output
            output.estimate(x, 0, 512);
            output.estimate(y, 0, 512);
        }
        else{
            Var xo, xi, yo, yi, idx;
            sums_x.compute_root().tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
            mean_ab_x.compute_root().tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
            //a_b.compute_root().tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
            mean_ab.compute_root().tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
            output.tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
        }
    }
private:
    Var x, y;
    Func down4_I, down4_P, a_b, mean_ab, mean_a, mean_b, up_mean_a, up_mean_b;
    Func sums_x, mean_ab_x;
};
HALIDE_REGISTER_GENERATOR(FastGuidedFilter, fast_guided_filter);

class FastGuidedFilterSkip : public Generator<FastGuidedFilterSkip> 
{
public:
    Input<Buffer<>> input_I{"input_I", 2};
    Input<Buffer<>> input_P{"input_P", 2};
    Input<int32_t> radius{"radius"};
    Input<int32_t> down{"down"};
    Input<float> eps{"eps"};
    Output<Func> output{"output", 2};
    Output<Func> out_mean_a{"mean_a", 2};
    Output<Func> out_mean_b{"mean_b", 2};
    void generate(){
        Func clamp_I = BoundaryConditions::repeat_edge(input_I);
        Func clamp_P = BoundaryConditions::repeat_edge(input_P);
        down4_I = BilinearScale(clamp_I, down, down);
        down4_P = BilinearScale(clamp_P, down, down);

        Expr down_r = max(radius/down, 1);

        RDom box(-down_r, (down_r*2+1), -down_r, (down_r*2+1));
        Expr area = f32((down_r*2+1)*(down_r*2+1));

        #if 0
        Expr val_I = f32(down4_I(x + box.x, y + box.y));
        Expr val_P = f32(down4_P(x + box.x, y + box.y));
        Expr mean_I = sum(val_I)/area;
        Expr mean_P = sum(val_P)/area;
        Expr corr_I = sum(val_I*val_I)/area;
        Expr corr_IP = sum(val_I*val_P)/area;
        Expr var_I = corr_I - mean_I*mean_I;
        Expr cov_IP = corr_IP - mean_I*mean_P;

        Expr a = cov_IP/(var_I + eps);
        Expr b = mean_P - a*mean_I;
        #else
        RDom r(-down_r, (down_r*2+1));
        Expr val_I = f32(down4_I(x + r, y));
        Expr val_P = f32(down4_I(x + r, y));
        Expr mean_I = sum(val_I);
        Expr mean_P = sum(val_P);
        Expr corr_I = sum(val_I*val_I);
        Expr corr_IP = sum(val_I*val_P);
        sums_x(x, y) = Tuple(mean_I, mean_P, corr_I, corr_IP);
        Tuple sums = Tuple(
                            sum(sums_x(x, y+r)[0])/area, 
                            sum(sums_x(x, y+r)[1])/area, 
                            sum(sums_x(x, y+r)[2])/area, 
                            sum(sums_x(x, y+r)[3])/area
                        );
        Expr var_I = sums[2] - sums[0]*sums[0];
        Expr cov_IP = sums[3] - sums[0]*sums[1];

        Expr a = cov_IP/(var_I + eps);
        Expr b = sums[1] - a*sums[0];
        #endif
        a_b(x, y) = Tuple(a, b);

        #if 0
        mean_ab(x, y) = Tuple(
                            sum(a_b(x + box.x, y + box.y)[0])/area, 
                            sum(a_b(x + box.x, y + box.y)[1])/area
                        );
        #else
        mean_ab_x(x, y) = Tuple(
                            sum(a_b(x + r, y)[0]), 
                            sum(a_b(x + r, y)[1])
                        );
        mean_ab(x, y) = Tuple(
                            sum(mean_ab_x(x, y + r)[0])/area, 
                            sum(mean_ab_x(x, y + r)[1])/area
                        );
        #endif

        mean_a(x, y) = mean_ab(x, y)[0];
        mean_b(x, y) = mean_ab(x, y)[1];

        Expr up_factor = 1.0f/f32(down);
        up_mean_a = BilinearScale(mean_a, up_factor, up_factor);
        up_mean_b = BilinearScale(mean_b, up_factor, up_factor);

        out_mean_a(x, y) = up_mean_a(x, y);
        out_mean_b(x, y) = up_mean_b(x, y);
        output(x, y) = cast(output.type(), up_mean_a(x, y)*f32(clamp_I(x, y)) + up_mean_b(x, y));
    }
    void schedule(){
        if(auto_schedule){
            //input_I
            input_I.dim(0).set_bounds_estimate(0, 512);
            input_I.dim(1).set_bounds_estimate(0, 512);
            //input_P
            input_P.dim(0).set_bounds_estimate(0, 512);
            input_P.dim(1).set_bounds_estimate(0, 512);
            //radius
            radius.set_estimate(8);
            //down
            down.set_estimate(8);
            //eps
            eps.set_estimate(250);
            //output
            output.estimate(x, 0, 512);
            output.estimate(y, 0, 512);
            out_mean_a.estimate(x, 0, 512);
            out_mean_a.estimate(y, 0, 512);
            out_mean_b.estimate(x, 0, 512);
            out_mean_b.estimate(y, 0, 512);
        }
        else{
            Var xo, xi, yo, yi, idx;
            sums_x.compute_root().tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
            mean_ab_x.compute_root().tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
            //a_b.compute_root().tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
            mean_ab.compute_root().tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
            output.tile(x, y, xo, xi, yo, yi, 16, 16).vectorize(xi, 4).fuse(xo, yo, idx).parallel(idx);
        }
    }
private:
    Var x, y;
    Func down4_I, down4_P, a_b, mean_ab, mean_a, mean_b, up_mean_a, up_mean_b;
    Func sums_x, mean_ab_x;
};
HALIDE_REGISTER_GENERATOR(FastGuidedFilterSkip, fast_guided_filter_skip);
