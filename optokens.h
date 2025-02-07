//
// definitions for various binary (and unary) operators
//
#ifndef BINOP_H
#define BINOP_H

#define K_ASSIGN 0x100
#define K_BOOL_OR 0x101
#define K_BOOL_AND 0x102
#define K_GE     0x103
#define K_LE     0x104
#define K_NE     0x105
#define K_EQ     0x106
#define K_LIMITMIN 0x107
#define K_LIMITMAX 0x108
#define K_MODULUS  0x109
#define K_HIGHMULT 0x10A
#define K_ROTR     0x10B
#define K_ROTL     0x10C
#define K_SHL      0x10D
#define K_SHR      0x10E
#define K_SAR      0x10F
#define K_REV      0x110
#define K_NEGATE   0x111
#define K_BIT_NOT  0x112
#define K_SQRT     0x113
#define K_ABS      0x114
#define K_DECODE   0x115
#define K_ENCODE   0x116
#define K_BOOL_NOT      0x117
#define K_SGNCOMP  0x118
#define K_INCREMENT 0x119
#define K_DECREMENT 0x11a
#define K_BOOL_XOR   0x11b
#define K_UNS_DIV    0x11c
#define K_UNS_MOD    0x11d
#define K_SIGNEXTEND 0x11e
#define K_ZEROEXTEND 0x11f
#define K_LTU       0x120
#define K_LEU       0x121
#define K_GTU       0x122
#define K_GEU       0x123
#define K_ASC       0x124
#define K_LIMITMIN_UNS 0x125
#define K_LIMITMAX_UNS 0x126
#define K_POWER     0x127
#define K_FRAC64    0x128
#define K_UNS_HIGHMULT 0x129
#define K_ONES_COUNT 0x12a
#define K_QLOG      0x12b
#define K_QEXP      0x12c
#define K_SCAS      0x12d
#define K_ENCODE2   0x12e
/* spin versions of && and ||, which cannot short-circuit */
#define K_LOGIC_AND 0x12f
#define K_LOGIC_OR  0x130
#define K_LOGIC_XOR 0x131

/* floating point */
/* these get turned into function calls */
#define K_FEQ       0x132
#define K_FNE       0x133
#define K_FLT       0x134
#define K_FGT       0x135
#define K_FLE       0x136
#define K_FGE       0x137
#define K_FADD      0x138
#define K_FSUB      0x139
#define K_FMUL      0x13a
#define K_FDIV      0x13b
#define K_FNEGATE   0x13c
#define K_FABS      0x13d
#define K_FSQRT     0x13e

#endif
