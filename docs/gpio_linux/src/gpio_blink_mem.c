#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <err.h>
#include <time.h>
#include <sys/mman.h>

uint32_t gpfsel_offset[] = {0x00, 0x04, 0x08, 0x0C, 0x10, 0x14};
uint32_t gpfset_offset[] = {0x1C, 0x20};
uint32_t gpfclr_offset[] = {0x28, 0x2C};

#define INPUT  0x0
#define OUTPUT 0x1

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

void set_pins(uint8_t *gpiomem, int *pins, int npins, int value)
{
	int i, n;
	uint32_t gpf0_val = 0;
	uint32_t gpf1_val = 0;
	volatile uint32_t *gpf0;
	volatile uint32_t *gpf1;

	if (value) {
		gpf0 = (uint32_t *)(gpiomem + gpfset_offset[0]);
		gpf1 = (uint32_t *)(gpiomem + gpfset_offset[1]);
	} else {
		gpf0 = (uint32_t *)(gpiomem + gpfclr_offset[0]);
		gpf1 = (uint32_t *)(gpiomem + gpfclr_offset[1]);
	}

	for (i = 0; i < npins; i++) {
		n = pins[i];
		if (n / 32)
			gpf1_val |= (1 << (n %32));
		else
			gpf0_val |= (1 << (n %32));
	}

	*gpf0 = gpf0_val;
	*gpf1 = gpf1_val;
}

int main(int argc, char *argv[])
{
	int fd, i;
	struct timespec ts;
	uint8_t *gpiomem;
	int leds_active = 1;
	int npins;
	int *pins;

	if (argc < 2)
		errx(EXIT_FAILURE, "%s pin-list\n", basename(argv[0]));

	fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
	if (fd < 0)
		err(EXIT_FAILURE, "open");

	gpiomem = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x3f200000);
	if (gpiomem == MAP_FAILED)
		err(EXIT_FAILURE, "mmap");

	npins = argc - 1;
	pins = malloc(npins * sizeof(int));
	if (pins == NULL)
		err(EXIT_FAILURE, "malloc");

	for (i = 0; i < npins; i++) {
		char desc[16];
		volatile uint32_t * fpsel;
		int n;

		snprintf(desc, 15, "pin%d", i+1);
		desc[15] = '\0';
		n = get_int(argv[i+1], 10, desc);
		if (n < 0 || n > 53)
			err(EXIT_FAILURE, "pin value");
		fpsel = (uint32_t *)(gpiomem + gpfsel_offset[n / 10]);
		*fpsel |= (0x1 << (n % 10)*3);
		pins[i] = n;
	}

	set_pins(gpiomem, pins, npins, leds_active);
	clock_gettime(CLOCK_MONOTONIC, &ts);
	while (1) {
		timespec_delta(&ts, 1, 0);
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
		leds_active = !leds_active;
		set_pins(gpiomem, pins, npins, leds_active);
	}

	return 0;
}
