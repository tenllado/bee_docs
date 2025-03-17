#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <err.h>
#include <time.h>
#include <linux/spi/spidev.h>

static char *progname;

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

void toggle_values(__u8 *values, int n)
{
    int i;
    for (i = 0; i < n; i++)
        values[i] = !values[i];
}

void timespec_delta(struct timespec *ts, time_t sec, long nsec)
{
    ts->tv_sec = ts->tv_sec + sec;
    ts->tv_nsec = ts->tv_nsec + nsec;
}

#define MCP3008_START 0x1
#define MCP3008_SIGL_DIFF (0x1 << 7)

int mcp3008_read(int fd, int channel, int *adcval)
{
    struct spi_ioc_transfer xfer[1];
    unsigned char buf[3];
    int ret = -1;

    memset(xfer, 0, sizeof xfer);
    memset(buf, 0, sizeof buf);

    buf[0] = MCP3008_START;
    buf[1] = MCP3008_SIGL_DIFF | ((channel & 0x7) << 4);
    xfer[0].tx_buf = (__u64) buf;
    xfer[0].rx_buf = (__u64) buf;
    xfer[0].len = sizeof buf;
    xfer[0].speed_hz = 1350000;
    if (ioctl(fd, SPI_IOC_MESSAGE(1), xfer) == 0) {
        ret = 0;
        *adcval = ((buf[1] & 0x3) << 8) | buf[2];
    }

    return ret;
}

int main(int argc, char *argv[])
{
    int fd, adc_mv, adc_val;
    struct timespec ts;
    __u8 spi_mode = (__u8) SPI_MODE_0;
    int channel, vdd_mv;

    progname = basename(argv[0]);

    if (argc != 4)
        errx(EXIT_FAILURE, "usage: %s spi-dev channel vdd_mv\n", progname);

    channel = get_int(argv[2], 10, "channel");
    vdd_mv = get_int(argv[3], 10, "vdd_mv");

    fd = open(argv[1], O_RDWR);
    if (fd < 0)
        err(EXIT_FAILURE, "open");

    if (ioctl(fd, SPI_IOC_WR_MODE, &spi_mode) < 0)
        err(EXIT_FAILURE, "ioctl set mode");

    clock_gettime(CLOCK_MONOTONIC, &ts);
    while (1) {
        timespec_delta(&ts, 1, 0);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
        if (mcp3008_read(fd, channel, &adc_val) < 0) {
            err(EXIT_FAILURE, "mcp3008_read");
        }
        adc_mv = (vdd_mv * adc_val) / 1023;
        printf("adc (mv): %d\n", adc_mv);
    }

    return 0;
}
