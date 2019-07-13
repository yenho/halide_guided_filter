
#include "Halide.h"
#include "gf_cfg.h"
#include <stdio.h>

using namespace Halide;
using namespace Halide::ConciseCasts;

#define USE_DIV_OP 0

class GuidedFilter : public Generator<GuidedFilter>
{
public:
    Input<Buffer<uint8_t>> input_I{"input_I", 2};
    Input<Buffer<uint8_t>> input_P{"input_P", 2};
    Input<Buffer<uint16_t>> div_tab{"div_tab", 1};
    Input<int16_t> rad{"radius"};
    Input<int16_t> eps{"eps"};
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
                            #if USE_DIV_OP
                            u16( sum(u32(sums_x(x, y+r)[0]))/area),
                            u16( sum(u32(sums_x(x, y+r)[1]))/area),
                            u16( sum(sums_x(x, y+r)[2])/area ),
                            u16( sum(sums_x(x, y+r)[3])/area )
                            #else
                            u16( (sum(u32(sums_x(x, y+r)[0]))*div_factor) >> DIV_Q),
                            u16( (sum(u32(sums_x(x, y+r)[1]))*div_factor) >> DIV_Q),
                            u16( (sum(sums_x(x, y+r)[2])*div_factor) >> DIV_Q ),
                            u16( (sum(sums_x(x, y+r)[3])*div_factor) >> DIV_Q )
                            #endif
                        );
        Expr var_I = (box[2] - box[0]*box[0]) + u16(eps);
        Expr cov_IP = (box[3] - box[0]*box[1]);

        #if USE_DIV_OP
        Expr a = u16((u32(cov_IP)<<7)/var_I);
        #else
        Expr a = u16(u32(cov_IP) * div_map(var_I  >> (7 + (9-DIV_BITS))) >> DIV_Q);
        #endif
        Expr b = i16((box[1]<<7)) - a*box[0];
        a_b(x, y) = Tuple(u16(a), i16(b));

        mean_ab_x(x, y) = Tuple( u32(sum(a_b(x+r, y)[0])), sum(i32(a_b(x+r, y)[1])) );
        Tuple mean_ab = Tuple(
                            #if USE_DIV_OP
                            u16(clamp((sum(mean_ab_x(x, y+r)[0])/area), 0, 0xFFFF)),
                            i16(sum(mean_ab_x(x, y+r)[1])/area)
                            #else
                            u16(clamp((sum(mean_ab_x(x, y+r)[0])*div_factor) >> DIV_Q, 0, 0xFFFF)),
                            i16( (sum(mean_ab_x(x, y+r)[1])*div_factor) >> DIV_Q)
                            #endif
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
            div_tab.dim(0).set_bounds_estimate(0, DIV_TAB_SIZE);

            // 2. parameters : parm.set_estimate(VALUE)
            rad.set_estimate(5);
            eps.set_estimate(1000);

            // 3. Func : func.estimate(Var, MIN, EXTENT);
            out.dim(0).set_bounds_estimate(0, 4096);
            out.dim(1).set_bounds_estimate(0, 3072);
        }
        else{
            Var tidx, xo, yo, xi, yi;
            if (get_target().has_feature(Halide::Target::HVX_128)) {
                printf("Generate Hexagon\n");
                div_map.hexagon().bound(x, 0, DIV_TAB_SIZE)
                    .compute_root()
                    .store_in(MemoryType::LockedCache)
                    .vectorize(x);

                a_b.hexagon().compute_root().fold_storage(y, 16)
                    .tile(x, y, xo, yo, xi, yi, 128, 32)
                    .fuse(xo, yo, tidx)
                    .parallel(tidx).vectorize(xi, 128, TailStrategy::RoundUp);
                sums_x.hexagon().compute_at(a_b, tidx).fold_storage(y, 16)
                    .vectorize(x, 128, TailStrategy::RoundUp);

                out.hexagon().fold_storage(y, 16)
                    .tile(x, y, xo, yo, xi, yi, 256, 32)
                    .fuse(xo, yo, tidx)
                    .parallel(tidx).vectorize(xi, 256, TailStrategy::RoundUp);
                mean_ab_x.hexagon().compute_at(out, tidx).fold_storage(y, 16)
                    .vectorize(x, 256, TailStrategy::RoundUp);
            } else {
                a_b.compute_root()
                    .tile(x, y, xo, yo, xi, yi, 128, 32)
                    .fuse(xo, yo, tidx)
                    .parallel(tidx).vectorize(xi, 128, TailStrategy::RoundUp);
                sums_x.store_at(a_b, tidx).compute_at(a_b, yi)
                    .vectorize(x, 128, TailStrategy::RoundUp);

                out.fold_storage(y, 16)
                    .tile(x, y, xo, yo, xi, yi, 256, 32)
                    .fuse(xo, yo, tidx)
                    .parallel(tidx).vectorize(xi, 256, TailStrategy::RoundUp);
                mean_ab_x.compute_at(out, tidx)
                    .vectorize(x, 256, TailStrategy::RoundUp);
            }
        }
    }
private:
    Var x, y;
    Func div_map, in_I, in_P, sums_x, a_b, mean_ab_x;
};
HALIDE_REGISTER_GENERATOR(GuidedFilter, guided_filter);


