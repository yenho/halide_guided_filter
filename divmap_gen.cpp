#include <stdio.h>

#include "gf_cfg.h"

int main(void)
{
    FILE *fptr = fopen("divmap.bin", "wb");
    unsigned short divmap[DIV_TAB_SIZE];
    divmap[0] = (1 << DIV_Q);
    int shift = (9 - DIV_BITS);
    for(int i = 1; i < DIV_TAB_SIZE; i++){
        divmap[i] = (1 << DIV_Q) / (i << shift);
    }
    fwrite(divmap, 1, DIV_TAB_SIZE*sizeof(unsigned short), fptr);
    fclose(fptr);
    return 0;
}
