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
#include <err.h>
#include <errno.h>
#include <time.h>
#include <sys/epoll.h>

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

    if (argc < 4 || (argc % 2) != 0)
        err(EXIT_FAILURE,
            "%s gpiodev inpin-0 inpin-1 ... outpin-0 outpin-1 ...\n",
            basename(argv[0]));

    memset(&values, 0, sizeof(struct gpio_v2_line_values));
    memset(&req, 0, sizeof(struct gpio_v2_line_request));

    req.num_lines = argc - 2;
    if (req.num_lines > GPIO_V2_LINES_MAX)
        errx(EXIT_FAILURE, "number of pins");

    req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT; // default
    req.config.num_attrs = 3;
    req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    req.config.attrs[1].attr.id = GPIO_V2_LINE_ATTR_ID_FLAGS; // for inputs
    req.config.attrs[2].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE; //for inputs
    // input pins first
    for (i = 0; i < req.num_lines/2; i++) {
        char desc[16];
        int n;
        snprintf(desc, 15, "inpin%d", n);
        desc[15] = '\0';
        n = get_int(argv[i+2], 10, desc);
        req.offsets[i] = n;

        req.config.attrs[1].mask |= (0x1 << i);
        req.config.attrs[1].attr.flags = (GPIO_V2_LINE_FLAG_INPUT |
                                          GPIO_V2_LINE_FLAG_EDGE_FALLING);
        req.config.attrs[2].mask |= (0x1 << i);
        req.config.attrs[2].attr.debounce_period_us = 10000;

    }

    // output pins come last
    for (i = req.num_lines/2; i < req.num_lines; i++) {
        char desc[16];
        int n;
        snprintf(desc, 15, "outpin%d", n);
        desc[15] = '\0';
        n = get_int(argv[i+2], 10, desc);
        req.offsets[i] = n;

        values.mask |= (0x1 << i);
        values.bits |= (0x1 << i);
        req.config.attrs[0].mask |= (0x1 << i);
        req.config.attrs[0].attr.values |= (0x1 << i);
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0)
        err(EXIT_FAILURE, "open");

    if (ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req) == -1)
        err(EXIT_FAILURE, "ioctl1");
    close(fd);

    while (1) {
        struct gpio_v2_line_event gev;
        int j;
        // blocking read, wait until event detected
        if (read(req.fd, &gev, sizeof(struct gpio_v2_line_event)) < 0)
            err(EXIT_FAILURE, "read");

        printf("Ev[%d] offset %d glob seq# %d line seq# %d tstmp %llu ns\n",
                gev.id, gev.offset, gev.seqno, gev.line_seqno, gev.timestamp_ns);

        // Change status of the corresponding output pin (j)
        for (i = 0, j = req.num_lines/2; i < req.num_lines/2; i++, j++) {
            if (req.offsets[i] == gev.offset)
                values.bits = values.bits ^ (1 << j);
        }

        // Set the new status on the line
        if (ioctl(req.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) == -1)
            err(EXIT_FAILURE, "ioctl2");
    }

    return 0;
}
