#include "Halide.h"
#include <stdio.h>
using namespace Halide;
// Support code for loading pngs.
#include "halide_image_io.h"
using namespace Halide::Tools;
using namespace Halide::ConciseCasts;
int main(int argc, char **argv) {
    Var x, y;

    if(argc != 6){
        printf("usage: argv[0] IN_I IN_P RADIUS EPS OUT_IMG \n");
        return -1;
    }
    Buffer<uint8_t> input_I = load_image(argv[1]);
    Buffer<uint8_t> input_P = load_image(argv[2]);
    int rad = atoi(argv[3]);
    int eps = atoi(argv[4]);


    Func in_I = BoundaryConditions::repeat_edge(input_I);
    Func in_P = BoundaryConditions::repeat_edge(input_P);

    RDom r(-rad, 2*rad+1);
    RDom rbox(-rad, 2*rad+1, -rad, 2*rad+1);
    Expr area = u16((rad*2+1)*(rad*2+1));

#if 0
    Func down4_I, down4_P, a_b, mean_ab, mean_a, mean_b, up_mean_a, up_mean_b;
    Func sums_x, mean_ab_x;

    Expr val_I = f32(in_I(x + r, y));
    Expr val_P = f32(in_P(x + r, y));
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

    a_b(x, y) = Tuple(a, b);

    mean_ab_x(x, y) = Tuple(
                        sum(a_b(x + r, y)[0]), 
                        sum(a_b(x + r, y)[1])
                    );
    mean_ab(x, y) = Tuple(
                        sum(mean_ab_x(x, y + r)[0])/area, 
                        sum(mean_ab_x(x, y + r)[1])/area
                    );
    Func output;
    output(x, y) = u8(mean_ab(x, y)[0]*f32(in_I(x, y)) + mean_ab(x, y)[1]);
    //output(x, y) = u8(a_b(x, y)[0]*256);
#else
    Func sums_x, a_b, mean_ab, mean_ab_x, output;

    Expr val_I = u16(in_I(x+r, y));
    Expr val_P = u16(in_P(x+r, y));
    Expr mean_I = sum(val_I);
    Expr mean_P = sum(val_P);
    Expr corr_I = sum(u32(val_I*val_I));
    Expr corr_IP = sum(u32(val_I*val_P));

    sums_x(x, y) = Tuple(mean_I, mean_P, corr_I, corr_IP);
    Tuple tmp = Tuple( 
                        sum(sums_x(x, y+r)[0])/area, 
                        sum(sums_x(x, y+r)[1])/area, 
                        u16(sum(sums_x(x, y+r)[2])/area), 
                        u16(sum(sums_x(x, y+r)[3])/area)
                    );
    Expr var_I = (tmp[2] - tmp[0]*tmp[0]);
    Expr cov_IP = (tmp[3] - tmp[0]*tmp[1]);

    Expr a = u16((u32(cov_IP)<<7)/(var_I + eps));
    Expr b = i16((tmp[1]<<7)) - a*tmp[0];
    a_b(x, y) = Tuple(u16(a), i16(b));

    mean_ab_x(x, y) = Tuple( sum(a_b(x+r, y)[0]), sum(i32(a_b(x+r, y)[1])) );
    mean_ab(x, y) = Tuple( sum(mean_ab_x(x, y+r)[0])/area, i16(sum(mean_ab_x(x, y+r)[1])/area) );
    output(x, y) = u8((mean_ab(x, y)[0]*in_I(x, y) + mean_ab(x, y)[1]) >> 7);

#endif
    Buffer<uint8_t> result = output.realize(input_I.width(), input_I.height());
    save_image(result, argv[5]);
    return 0;
}
