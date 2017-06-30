CFLAGS=-I/usr/include/libftdi1

%.o : %.c
	$(CC) -c $^ -o $@ $(CFLAGS)

mpu9250: ftdi_mpu9250.o MPU9250BasicAHRS_t3.o
	$(CC) $^ -o $@ -lm -lusb-1.0 -lftdi1

clean:
	rm *.o
	rm mpu9250
