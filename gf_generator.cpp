
#include "Halide.h"
#include "gf_cfg.h"
#include <stdio.h>

using namespace Halide;
using namespace Halide::ConciseCasts;

class GuidedFilter : public Generator<GuidedFilter> 
{
public:
    Input<Buffer<uint8_t>> input_I{"input_I", 2};
    Input<Buffer<uint8_t>> input_P{"input_P", 2};
    Input<Buffer<uint16_t>> div_tab{"div_tab", 1};
    Input<int32_t> rad{"radius"};
    Input<int32_t> eps{"eps"};
    Output<Buffer<uint8_t>> out{"out", 2};

    void generate(){
        in_I = BoundaryConditions::repeat_edge(input_I);
        in_P = BoundaryConditions::repeat_edge(input_P);
        div_map = BoundaryConditions::repeat_edge(div_tab);

        RDom r(-rad, 2*rad+1);
        Expr area = u16((rad*2+1)*(rad*2+1));
        Expr div_factor = u32((1<<DIV_Q)/area);

        Expr val_I = u16(in_I(x+r, y));
        Expr val_P = u16(in_P(x+r, y));
        Expr mean_I = sum(val_I);
        Expr mean_P = sum(val_P);
        Expr corr_I = sum(u32(val_I*val_I));
        Expr corr_IP = sum(u32(val_I*val_P));

        sums_x(x, y) = Tuple(mean_I, mean_P, corr_I, corr_IP);
        Tuple box = Tuple( 
                            u16( (sum(u32(sums_x(x, y+r)[0]))*div_factor) >> DIV_Q), 
                            u16( (sum(u32(sums_x(x, y+r)[1]))*div_factor) >> DIV_Q), 
                            u16( (sum(sums_x(x, y+r)[2])*div_factor) >> DIV_Q ), 
                            u16( (sum(sums_x(x, y+r)[3])*div_factor) >> DIV_Q )
                        );
        Expr var_I = (box[2] - box[0]*box[0]);
        Expr cov_IP = (box[3] - box[0]*box[1]);

        //Expr a = cov_IP/((var_I + eps) >> 7);
        Expr a = u16(u32(cov_IP) * div_map((var_I + eps) >> 7) >> DIV_Q);
        Expr b = i16((box[1]<<7)) - a*box[0];
        a_b(x, y) = Tuple(u16(a), i16(b));

        mean_ab_x(x, y) = Tuple( u32(sum(a_b(x+r, y)[0])), sum(i32(a_b(x+r, y)[1])) );
        Tuple mean_ab = Tuple( 
                            u16(clamp((sum(mean_ab_x(x, y+r)[0])*div_factor) >> DIV_Q, 0, 0xFFFF)), 
                            i16( (sum(mean_ab_x(x, y+r)[1])*div_factor) >> DIV_Q) 
                        );
        out(x, y) = u8((mean_ab[0]*in_I(x, y) + mean_ab[1]) >> 7);
    }

    void schedule(){
        if(auto_schedule){
            // 1. Buffer : buf.dim(N).set_bounds_estimate(MIN, EXTENT)
            input_I.dim(0).set_bounds_estimate(0, 4096);
            input_I.dim(1).set_bounds_estimate(0, 3072);
            input_P.dim(0).set_bounds_estimate(0, 4096);
            input_P.dim(1).set_bounds_estimate(0, 3072);
            div_tab.dim(0).set_bounds_estimate(0, 512);

            // 2. parameters : parm.set_estimate(VALUE)
            rad.set_estimate(5);
            eps.set_estimate(1000);

            // 3. Func : func.estimate(Var, MIN, EXTENT);
            out.dim(0).set_bounds_estimate(0, 4096);
            out.dim(1).set_bounds_estimate(0, 3072);
        }
        else{
            printf("Generate Hexagon\n");
            if (get_target().has_feature(Halide::Target::HVX_128)) {
                Var xo, yo, xi, yi;
                div_map.store_in(MemoryType::LockedCache);
                /*
                sums_x.hexagon().compute_root()
                    .tile(x, y, xo, yo, xi, yi, 128, 32)
                    .parallel(yo).vectorize(xi, 128);
                    */ 
                a_b.hexagon().compute_root()
                    .tile(x, y, xo, yo, xi, yi, 128, 32)
                    .parallel(yo).vectorize(xi, 128);
                sums_x.compute_at(a_b, yo)
                    .vectorize(x, 128);
                /*
                mean_ab_x.hexagon().compute_root()
                    .tile(x, y, xo, yo, xi, yi, 128, 32)
                    .parallel(yo).vectorize(xi, 128);
                    */
                out.hexagon()
                    .tile(x, y, xo, yo, xi, yi, 128, 32)
                    .parallel(yo).vectorize(xi, 128);
                mean_ab_x.compute_at(out, yo)
                    .vectorize(x, 128);
            }
        }
    }
private:
    Var x, y;
    Func div_map, in_I, in_P, sums_x, a_b, mean_ab, mean_ab_x;
};
HALIDE_REGISTER_GENERATOR(GuidedFilter, guided_filter);


