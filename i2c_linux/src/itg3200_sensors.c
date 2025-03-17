#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <libgen.h>

static char *progname;
static int addr_itg3200 = 0x68;
float GYRO_SENSITIVITY_INV = (1.0/14.375); 

enum itgsensor {
	TEMP = 0,
	GYRO_X,
	GYRO_Y,
	GYRO_Z
};

const char * sensornames[] = {
	"Temp",
	"Gyro-X",
	"Gyro-Y",
	"Gyro-Z"
};

struct sensorid {
	unsigned char h;
	unsigned char l;
};

struct sensorid sensorids[] = {
	{.h = 0x1b, .l = 0x1c},
	{.h = 0x1d, .l = 0x1e},
	{.h = 0x1f, .l = 0x20},
	{.h = 0x21, .l = 0x22}
};


void usage_err(char *format, ...)
{
    va_list arglist;
    va_start(arglist, format);
	vfprintf(stderr, format, arglist);
	fflush(stderr);
	exit(EXIT_FAILURE);
}


void err_exit_en(int errnum,char *format, ...)
{
    va_list arglist;
    va_start(arglist, format);

#define BUFSIZE 500
    char buf[BUFSIZE], user_msg[BUFSIZE], err_msg[BUFSIZE];
    vsnprintf(user_msg, BUFSIZE, format, arglist);
    snprintf(err_msg, BUFSIZE, "[%d %s]", errnum, strerror(errnum));
    snprintf(buf,BUFSIZE, "Error %s %s\n", err_msg, user_msg);
    fputs(buf, stderr);
    fflush(stderr);

    va_end(arglist);
    exit(EXIT_FAILURE);
}

int get_int(char *arg, int base, char *desc)
{
	char *endptr;
	long val;

	errno = 0;
	val = strtol(arg, &endptr, base);

	if (errno != 0) {
		perror(desc);
		exit(EXIT_FAILURE);
	}

	if (*endptr != '\0') {
		fprintf(stderr, "error parsing integer parameter %s\n", desc);
		exit(EXIT_FAILURE);
	}

	return (int) val;
}

void itg3200_setup(int fd)
{
	unsigned char dlpf_reg = 0x16;
	unsigned char smplrt_reg = 0x15;
	unsigned char val;

	val = ((0x3 << 3) | 0x6) & 0xff;
	i2c_smbus_write_byte_data(fd, dlpf_reg, val);

	val = 7;
	i2c_smbus_write_byte_data(fd, smplrt_reg, val);
}

short int itg3200_read_sensor_smbus(int fd, enum itgsensor s)
{
	int hval, lval;
	short int val;
	unsigned char regh = sensorids[s].h;
	unsigned char regl = sensorids[s].l;

	hval = i2c_smbus_read_byte_data(fd, regh);
	if (hval == -1)
		err_exit_en(errno, "i2c_smbus_read_byte_data");

	lval = i2c_smbus_read_byte_data(fd, regl);
	if (lval == -1)
		err_exit_en(errno, "i2c_smbus_read_byte_data");

	val = ((hval & 0xff) << 8) | (lval & 0xff);
	return val;
}

short int itg3200_read_sensor_standard(int fd, enum itgsensor s)
{
	unsigned char hval, lval;
	short int val;
	int err;
	unsigned char regh = sensorids[s].h;
	unsigned char regl = sensorids[s].l;

	err = write(fd, &regh, 1);
	if (err < 0)
		err_exit_en(errno, "write");

	err = read(fd, &hval, 1);
	if (err < 0)
		err_exit_en(errno, "read");

	err = write(fd, &regl, 1);
	if (err < 0)
		err_exit_en(errno, "write");

	err = read(fd, &lval, 1);
	if (err < 0)
		err_exit_en(errno, "read");

	val = ((hval & 0xff) << 8) | (lval & 0xff);
	return val;
}

short int itg3200_read_sensor_rdwr(int fd, enum itgsensor s)
{
	unsigned char hval, lval;
	short int val;
	int err;
	unsigned char regh = sensorids[s].h;
	unsigned char regl = sensorids[s].l;
	struct i2c_msg msgs[2];
	struct i2c_rdwr_ioctl_data i2cdata;

	msgs[0].addr = addr_itg3200;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &regh;

	msgs[1].addr = addr_itg3200;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = &hval;

	i2cdata.msgs = msgs;
	i2cdata.nmsgs = 2;
	err = ioctl(fd, I2C_RDWR, &i2cdata);
	if (err == -1)
		err_exit_en(errno, "ioctl I2C_RDWR");

	msgs[0].addr = addr_itg3200;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &regl;

	msgs[1].addr = addr_itg3200;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = &lval;

	i2cdata.msgs = msgs;
	i2cdata.nmsgs = 2;
	err = ioctl(fd, I2C_RDWR, &i2cdata);
	if (err == -1)
		err_exit_en(errno, "ioctl I2C_RDWR");

	val = ((hval & 0xff) << 8) | (lval & 0xff);
	return val;
}


int main(int argc, char *argv[])
{
	int fd, err;
	float realval;
	short rawval;
	int opt,i;

	int show_raw = 0;
	int read_sensors[4] = {0,0,0,0};
	int method = 0;

	progname = basename(argv[0]);

	while ((opt = getopt(argc, argv, "txyzwcs")) != -1) {
		switch (opt) {
		case 't':
			read_sensors[TEMP] = 1;
			break;
		case 'x':
			read_sensors[GYRO_X] = 1;
			break;
		case 'y':
			read_sensors[GYRO_Y] = 1;
			break;
		case 'z':
			read_sensors[GYRO_Z] = 1;
			break;
		case 'c':
			if (method != 0) {
				usage_err("usage: %s [-txyzw] [-c|-s]\n", progname);
			}
			method = 1;
			break;
		case 's':
			if (method != 0) {
				usage_err("usage: %s [-txyz] [-c|-s]\n", progname);
			}
			method = 2;
			break;
		case 'w':
			show_raw = 1;
			break;
		}
	}

	fd = open("/dev/i2c-1", O_RDWR);
	if (fd == -1)
		err_exit_en(errno, "open");

	err = ioctl(fd, I2C_SLAVE, addr_itg3200);
	if (err == -1)
		err_exit_en(errno, "ioctl");

	itg3200_setup(fd);

	for (i = 0; i < 4; i++) {
		if (!read_sensors[i])
			continue;

		switch (method) {
		case 1:
			rawval = itg3200_read_sensor_standard(fd, i);
			break;
		case 2:
			rawval = itg3200_read_sensor_rdwr(fd, i);
			break;
		default:
			rawval = itg3200_read_sensor_smbus(fd, i);
			break;
		}

		if (i == TEMP) {
			realval = 35.0 +  ((float)(rawval + 13200) / 280.0);
		} else {
			realval = ((float)rawval) * GYRO_SENSITIVITY_INV; 
		}

		if (show_raw)
			printf("%s raw value %hd\n", sensornames[i], rawval);
		printf("%s real value: %g\n", sensornames[i], realval);
	}

	return 0;
}
