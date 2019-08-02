#include <stdio.h>

#include "gf_cfg.h"

int main(void)
{
    FILE *fptr = fopen("divmap.bin", "wb");
    unsigned short divmap[DIV_TAB_SIZE];
    divmap[0] = (1 << DIV_F) - 1;
    int shift = (16 - Q_BITS - DIV_BITS);

    for(int i = 1; i < DIV_TAB_SIZE; i++)
        divmap[i] = (1 << DIV_F) / (i << shift);

    if(shift == 0)
        divmap[1] = (1 << DIV_F) - 1;

    fwrite(divmap, 1, DIV_TAB_SIZE*sizeof(unsigned short), fptr);
    fclose(fptr);
    return 0;
}
