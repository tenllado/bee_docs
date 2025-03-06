#include <stdio.h>
#include <stdlib.h>
#include <linux/gpio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <libgen.h>
#include <err.h>

static char *progname;

int main(int argc, char *argv[])
{
    int opt, fd;
    struct gpiochip_info info;

    progname = basename(argv[0]);

    if (argc != 2)
        errx(EXIT_FAILURE, "%s filename\n", progname);

    fd = open(argv[1], O_RDONLY);
    if (fd < 0)
        err(EXIT_FAILURE, "open");

    if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) == -1)
        err(EXIT_FAILURE, "ioctl");
    close(fd);

    printf("label: %s\n", info.label);
    printf("name: %s\n", info.name);
    printf("number of lines: %u\n", info.lines);

    return 0;
}
