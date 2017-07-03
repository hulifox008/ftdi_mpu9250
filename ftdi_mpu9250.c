#include <stdio.h>
#include <ftdi.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <libusb-1.0/libusb.h>
#include "ftdi_mpu9250.h"
#include "ahrs.h"


/* Pin map 
 *
 * FT2232H  DIR     MPU9250 
 * ADBUS0   ->        SCK
 * ADBUS1   ->        MOSI
 * ADBUS2   <-        MISO
 * ADBUS4   ->        CSn
 */

#define FTDI_LATENCY    10  /* ms  */

struct ftdi_context *ftdi_ctx;

#define BIT_SCK     (1<<0)
#define BIT_MOSI    (1<<1)
#define BIT_MISO    (1<<2)
#define BIT_CS_N    (1<<4)

u_int8_t port_status;


struct mpu9250_ctx mpu9250; 

int ftdi_read_data_wait(struct ftdi_context *ftdi_ctx, unsigned char *buf, int size) 
{
    assert(buf!=NULL);
    assert(size > 0);

    int ret;
    int got = 0;
    struct timespec ts_start, ts_now;

    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    while(got<size) {
        ret = ftdi_read_data(ftdi_ctx, buf+got, size-got); 
        if(ret<0) {
            fprintf(stderr, "ftdi_read_data() failed, ret %d\n", ret);
            return -1;
        }

        got+=ret;
        if(got==size) {
            return got;
        }

        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        if(( ts_now.tv_sec - ts_start.tv_sec) * 1000000000
        + ts_now.tv_nsec - ts_start.tv_nsec
        > 1000000000) {
            fprintf(stderr, "ftdi_read_data_wait() time out.\n");
            return -1;
        }

        usleep(FTDI_LATENCY * 1000);
    }

    /* Never reached. */
    return -1;
}

int mpu9250_writebytes(struct mpu9250_ctx *mpu9250, u_int8_t addr, int n, u_int8_t *values)
{
    assert(n>=0 && n<=512);
    assert(n==0 || NULL!=values);
    
    unsigned char buf[530];
    int data_len = 0;
    int i;

    /* CSn lo */
    port_status &= ~BIT_CS_N;
    buf[data_len++] = 0x80;
    buf[data_len++] = port_status;
    buf[data_len++] = 0x33;

    buf[data_len++] = 0x11; /* MOSI, MSB first, data change on -ve */
    buf[data_len++] = n;
    buf[data_len++] = 0;
    buf[data_len++] = addr; /* Write REG command */
    
    memcpy(buf + data_len, values, n);
    data_len += n;

    /* set data bits low. SCK, MOSI lo, CSn hi */
    port_status = port_status & ~(BIT_SCK | BIT_MOSI) | BIT_CS_N;
    buf[data_len++] = 0x80;
    buf[data_len++] = port_status;
    buf[data_len++] = 0x33;

    if(data_len != ftdi_write_data(ftdi_ctx, buf, data_len)) {
        return -1;
    }
    return 0;

}
int mpu9250_readbytes(struct mpu9250_ctx *mpu9250, u_int8_t addr, int n, u_int8_t *values)
{
    assert(NULL!=values);
    assert(n>0 && n<=512);

    int ret;
    unsigned char buf[16];

    int data_len = 0;

    port_status &= ~BIT_CS_N;
    buf[data_len++] = 0x80;
    buf[data_len++] = port_status;
    buf[data_len++] = 0x33;

    buf[data_len++] = 0x11; /*clock data out on -ve, MSB first */
    buf[data_len++] = 0x00; /* 1 bytes data. */
    buf[data_len++] = 0x00;
    buf[data_len++] = addr | 0x80; /* READ REG command */

    buf[data_len++] = 0x20; /* clock data in on +ve, MSB first */
    buf[data_len++] = n - 1; /* 1 bytes data. */
    buf[data_len++] = 0x00;

    port_status = (port_status & ~(BIT_SCK | BIT_MOSI)) | BIT_CS_N;
    buf[data_len++] = 0x80;
    buf[data_len++] = port_status;
    buf[data_len++] = 0x33;

    if(data_len != ftdi_write_data(ftdi_ctx, buf, data_len)) {
        return -1;
    }

    if(ftdi_read_data_wait(ftdi_ctx, values, n)) {
        return -1;
    }

    return 0;
}

u_int8_t mpu9250_readbyte(struct mpu9250_ctx *mpu9250, u_int8_t addr)
{
    u_int8_t v=0;
    mpu9250_readbytes(mpu9250, addr, 1, &v);
    return v;
}

void mpu9250_writebyte(struct mpu9250_ctx *mpu9250, u_int8_t addr, u_int8_t data)
{
    mpu9250_writebytes(mpu9250, addr, 1, &data);
}

int main()
{
    int ret;
    unsigned char buf[32] = {0};


    ftdi_ctx = ftdi_new();
    if(!ftdi_ctx) {
        fprintf(stderr, "ftdi_new() failed!\n");
        return -1;
    }

    ftdi_set_interface(ftdi_ctx, INTERFACE_A);

    ret = ftdi_usb_open(ftdi_ctx, 0x0403, 0x6010);
    if(ret) {
        fprintf(stderr, "ftdi_usb_open() failed. ret %d\n", ret);
        return -1;
    }

    fprintf(stderr, "usb_dev %p\n", ftdi_ctx->usb_dev);
    libusb_reset_device(ftdi_ctx->usb_dev);
    fprintf(stderr, "reset done\n");

    ret = ftdi_set_latency_timer(ftdi_ctx, FTDI_LATENCY);
    if(ret) {
        fprintf(stderr, "ftdi_set_latency_timer() failed. ret %d\n", ret);
        return -1;
    }

/*
    ret = ftdi_set_baudrate(ftdi_ctx, 1000);
    if(ret) {
        fprintf(stderr, "ftdi_set_baudrate() failed. ret %d\n", ret);
        return -1;
    }
    */

    ret = ftdi_set_bitmode(ftdi_ctx, 0xff, BITMODE_MPSSE);
    if(ret) {
        fprintf(stderr, "ftdi_set_bitmode() failed. ret %d\n", ret);
        return -1;
    }

    ret = ftdi_write_data(ftdi_ctx, "\xaa", 1);
    fprintf(stderr, "ftdi_write_data() ret %d\n", ret);

    ret = ftdi_read_data(ftdi_ctx, buf, 32);
    fprintf(stderr, "ftdi_read_data() ret %d\n", ret);
    if(ret>0) {
        int i;
        for(i=0;i<ret;i++) {
            fprintf(stderr, "%02X ", buf[i]);
        }
        fprintf(stderr, "\n");
    }


    /* set clk divisor for 300KHz clock */
    ret = ftdi_write_data(ftdi_ctx, "\x86\x10\x00", 3);
    fprintf(stderr, "ftdi_write_data() ret %d\n", ret);

    /* set data bits low.  SCK, MOSI lo, CSn hi */
    port_status = BIT_CS_N;
    buf[0] = 0x80;
    buf[1] = port_status;
    buf[2] = 0x33;
    ret = ftdi_write_data(ftdi_ctx, buf, 3);

    mpu9250.ftdi_ctx = ftdi_ctx;



    setup();

    return 0;
}
