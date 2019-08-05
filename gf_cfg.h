#ifndef _GF_CFG_H_
#define _GF_CFG_H_

// division factors, here we use (1 << DIV_F) for division factor
#define DIV_F 16

// (less than 1) fractional quotient bits, should not larger than 7, 
// since for i16, 8 may make values become negative
#define Q_BITS 7
// division table size, DIV_BITS must be <= 16 - Q_BITS, 
// small value make table smaller, but may also introduce errors (currently should be 7, 8 or 9)
#define DIV_BITS 7
#define DIV_TAB_SIZE (1<<DIV_BITS)

#endif
