#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <linux/gpio.h>
#include <linux/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <time.h>
#include <libudev.h>
#include <sys/select.h>

#define SPACING   20000000
#define STOP_DUTYC 1500000
#define MAX_DUTYC  1700000
#define MIN_DUTYC  1300000

static char *progname;
int period = SPACING + STOP_DUTYC;
int dutyc  = STOP_DUTYC;

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

void timespec_delta(struct timespec *ts, time_t sec, long nsec)
{
	ts->tv_sec = ts->tv_sec + sec;
	ts->tv_nsec = ts->tv_nsec + nsec;
}

int pwm_export_wait(void)
{
	struct udev *udev;
	struct udev_device *dev;
   	struct udev_monitor *mon;
	int fd;
	int done = 0;

	/* create udev object */
	udev = udev_new();
	if (!udev) {
		fprintf(stderr, "Can't create udev\n");
		return -1;
	}

	mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "pwm", NULL);
	udev_monitor_enable_receiving(mon);
	fd = udev_monitor_get_fd(mon);

	while (!done) {
		fd_set fds;
		struct timeval tv;
		int ret;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		ret = select(fd+1, &fds, NULL, NULL, &tv);
		if (ret > 0 && FD_ISSET(fd, &fds)) {
			dev = udev_monitor_receive_device(mon);
			if (dev) {
				printf("I: ACTION=%s\n", udev_device_get_action(dev));
				printf("I: DEVNAME=%s\n", udev_device_get_sysname(dev));
				printf("I: DEVPATH=%s\n", udev_device_get_devpath(dev));
				done = 1;
			}
		}
	}
	udev_device_unref(dev);
	udev_monitor_unref(mon);
	udev_unref(udev);

	return 0;
}

void pwm_export(int channel)
{
	int fd;
	int err;
	char buf[8];

	fd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY);
	if (fd < 0)
		err_exit_en(errno, "open export");

	snprintf(buf, 7, "%d\n", channel);
	buf[7] = '\0';
	err = write(fd, buf, strlen(buf));
	if (err < 0) {
		if (errno == EBUSY)
			fprintf(stderr, "Warning: pwm port already exported\n");
		else
			err_exit_en(errno, "write pwm channel");
	} else {
		pwm_export_wait();
	}
	fsync(fd);
	close(fd);
}

void pwm_write_channel(int channel, char *file, int value)
{
	int fd;
	int err;
	char buf[16];
	char fname[64];
	char err_str[70];

	snprintf(fname, 63, "/sys/class/pwm/pwmchip0/pwm%d/%s", channel, file);
	fname[63] = '\0';
	fd = open(fname, O_WRONLY);
	if (fd < 0) {
		snprintf(err_str, 70, "open %s", fname);
		err_exit_en(errno, err_str);
	}

	snprintf(buf, 15, "%d\n", value);
	buf[15] = '\0';
	err = write(fd, buf, strlen(buf));
	if (err < 0) {
		err_exit_en(errno, "write value");
	}
	fsync(fd);
	close(fd);
}

void pwm_setperiod(int channel, int period_ns)
{
	pwm_write_channel(channel, "period", period_ns);
}

void pwm_setdutyc(int channel, int duty_cycle_ns)
{
	pwm_write_channel(channel, "duty_cycle", duty_cycle_ns);
}

void pwm_enable(int channel)
{
	pwm_write_channel(channel, "enable", 1);
}

void pwm_disable(int channel)
{
	pwm_write_channel(channel, "enable", 0);
}

void pwm_update(int channel)
{
	pwm_setperiod(channel, period);
	pwm_setdutyc(channel, dutyc);
}

void pwm_stop(int channel)
{
	period = SPACING + STOP_DUTYC;
	dutyc  = STOP_DUTYC;
	pwm_update(channel);
}

void pwm_update_dutyc(int channel, int delta)
{
	dutyc  += delta;
	if (dutyc > MAX_DUTYC)
		dutyc = MAX_DUTYC;
	if (dutyc < MIN_DUTYC)
		dutyc = MIN_DUTYC;

	period = SPACING + dutyc;
	pwm_update(channel);
}

int main(int argc, char *argv[])
{
	int fd, err, i;
	struct gpio_v2_line_request req;
	char *gpios[3] = {"decrease", "stop", "increase"};
	int pwm_channel;

	progname = basename(argv[0]);

	if (argc != 6)
		usage_err("%s gpiodev pwm-channel decreasepin stoppin increasepin\n", progname);

	memset(&req, 0, sizeof(struct gpio_v2_line_request));

	req.num_lines = 3;

	req.config.flags = GPIO_V2_LINE_FLAG_INPUT | GPIO_V2_LINE_FLAG_EDGE_FALLING;
	req.config.num_attrs = 1;
	req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE;
	// input pins first
	for (i = 0; i < 3; i++) {
		req.offsets[i] = get_int(argv[i+3], 10, gpios[i]);
		req.config.attrs[0].mask |= (0x1 << i);
		req.config.attrs[0].attr.debounce_period_us = 10000;
	}


	fd = open(argv[1], O_RDONLY);
	if (fd < 0)
		err_exit_en(errno, "open");

	err = ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req);
	if (err == -1)
		err_exit_en(errno, "ioctl1");
	close(fd);

	pwm_channel = get_int(argv[2], 10, "pwm channel");
	pwm_export(pwm_channel);
	pwm_stop(pwm_channel);
	pwm_enable(pwm_channel);

	printf("System ready for operation, push buttons to control the servo\n");
	while (1) {
		struct gpio_v2_line_event gev;
		err = read(req.fd, &gev, sizeof(struct gpio_v2_line_event));
		if (err < 0)
			err_exit_en(errno, "read");

		//printf("Ev[%d] offset %d glob seq# %d line seq# %d tstmp %llu ns\n",
		//		gev.id, gev.offset, gev.seqno, gev.line_seqno, gev.timestamp_ns);

		if (gev.offset == req.offsets[0]) {
			pwm_update_dutyc(pwm_channel, -10000);
			printf("Pulse width set to %d\n", dutyc);
		} else if (gev.offset == req.offsets[1]) {
			printf("Stopping\n");
			pwm_stop(pwm_channel);
		} else if (gev.offset == req.offsets[2]) {
			pwm_update_dutyc(pwm_channel,  10000);
			printf("Pulse width set to %d\n", dutyc);
		} else {
			fprintf(stderr, "unkown event\n");
		}
	}

	return 0;
}
