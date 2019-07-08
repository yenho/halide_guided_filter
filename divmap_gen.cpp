#include <stdio.h>

#include "gf_cfg.h"

int main(void)
{
    FILE *fptr = fopen("divmap.bin", "wb");
    unsigned short divmap[512];
    divmap[0] = (1 << DIV_Q);
    for(int i = 1; i < 512; i++){
        divmap[i] = (1 << DIV_Q) / i;
    }
    fwrite(divmap, 1, 512*sizeof(unsigned short), fptr);
    fclose(fptr);
    return 0;
}
