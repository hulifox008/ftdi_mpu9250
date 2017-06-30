#ifndef _FTDI_MPU9250_INCLUDED_
#define _FTDI_MPU9250_INCLUDED_

#include <ftdi.h>

struct mpu9250_ctx {
    struct ftdi_context *ftdi_ctx;
};

int mpu9250_readbytes(struct mpu9250_ctx *mpu9250, u_int8_t addr, int n, u_int8_t *values);

int mpu9250_writebytes(struct mpu9250_ctx *mpu9250, u_int8_t addr, int n, u_int8_t *values);

u_int8_t mpu9250_readbyte(struct mpu9250_ctx *mpu9250, u_int8_t addr);

void mpu9250_writebyte(struct mpu9250_ctx *mpu9250, u_int8_t addr, u_int8_t data);

#endif
