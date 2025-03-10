#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/gpio.h>
#include <linux/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <err.h>
#include <time.h>

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

void timespec_delta(struct timespec *ts, time_t sec, long nsec)
{
    ts->tv_sec = ts->tv_sec + sec;
    ts->tv_nsec = ts->tv_nsec + nsec;
}

int main(int argc, char *argv[])
{
    int fd, i;
    struct gpio_v2_line_request req;
    struct gpio_v2_line_values values;
    struct timespec ts;

    progname = basename(argv[0]);

    if (argc < 3)
        errx(EXIT_FAILURE, "usage: %s gpiodev pin-list\n", progname);

    memset(&values, 0, sizeof(struct gpio_v2_line_values));
    memset(&req, 0, sizeof(struct gpio_v2_line_request));

    req.num_lines = argc - 2;
    if (req.num_lines > GPIO_V2_LINES_MAX)
        err(EXIT_FAILURE, "number of pins");

    req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    req.config.num_attrs = 1;
    req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    for (i = 0; i < req.num_lines; i++) {
        char desc[16];
        int n;
        snprintf(desc, 15, "pin%d", i+1);
        desc[15] = '\0';
        n = get_int(argv[i+2], 10, desc);
        req.offsets[i] = n;
        values.mask |= (0x1 << i);
        req.config.attrs[0].mask |= (0x1 << i);
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0)
        err(EXIT_FAILURE, "open");

    if (ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req) == -1)
        err(EXIT_FAILURE, "ioctl1");
    
    close(fd);

    clock_gettime(CLOCK_MONOTONIC, &ts);
    while (1) {
        timespec_delta(&ts, 1, 0);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
        values.bits ^= values.mask;
        if (ioctl(req.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) == -1)
            err(EXIT_FAILURE, "ioctl2");
    }

    return 0;
}
