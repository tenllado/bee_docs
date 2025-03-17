#!/bin/bash

PWMDIR=/sys/class/pwm/pwmchip0

test_pwmdir() {
	if [ ! -d ${PWMDIR} ]; then
		echo "$PWMDIR directory not found" 
		exit 1
	fi
}

test_channeldir() {
	CHANNELDIR="${PWMDIR}/pwm$1"
	if [ ! -d ${CHANNELDIR} ]; then
		echo "$CHANNELDIR directory not found. PWM channel unavailable" 
		exit 2
	fi
}

do_enable() {
	CHANNELDIR="${PWMDIR}/pwm$1"
	test_channeldir $1
	echo "Enabling PWM$1 channel"
	echo 1 > ${CHANNELDIR}/enable
}

do_disable() {
	CHANNELDIR="${PWMDIR}/pwm$1"
	test_channeldir $1
	echo "Disabling PWM$1 channel"
	echo 0 > ${CHANNELDIR}/enable
}

do_export() {
	test_pwmdir
	echo "Exporting PWM$1 channel"
	echo $1 > ${PWMDIR}/export
	test_channeldir $1
}

do_unexport() {
	test_pwmdir
	echo "Unexporting PWM$1 channel"
	echo $1 > ${PWMDIR}/unexport
}

do_set_speed() {
	CHANNELDIR="${PWMDIR}/pwm$1"
    pw_ms=$2
	spacing_ms=20

	test_channeldir $1

	duty_cycle=$(echo "$pw_ms * 1000000" | bc | cut -d. -f1)
	period=$(echo "($pw_ms + $spacing_ms) * 1000000" | bc | cut -d. -f1)

	echo "period:     $period ns"
	echo "duty_cycle: $duty_cycle ns"

	echo $period > ${CHANNELDIR}/period
	echo $duty_cycle > ${CHANNELDIR}/duty_cycle
}

usage() {
	echo "Usage: $1  options"
	echo "options:"
	echo "-c selects channel 1 instead of default channel 0"
	echo "-d disables de the pwm output"
	echo "-e enables de the pwm output"
	echo "-h shows this help message"
	echo "-s pulse_width_ms sets the pulse width (speed)"
	echo "-x exports the pwm channel"
	echo "-u unexports the pwm channel"
}

err_usage() {
	usage
	exit 1
}


CHANNEL=0

if [ "$#" -eq 0 ]; then
	err_usage
fi

while getopts "dehxus:c" OPTION; do
    case $OPTION in
	c)  CHANNEL=1
		;;
	h)  usage
		;;
    e)  ENABLE=1
		DONE=1
        ;;
    d)  DISABLE=1
		do_dsable
        ;;
    s)  SET_SPEED=1
		PULSE_WIDTH=$OPTARG
        ;;
	u)  UNEXPORT=1
        ;;
	x)  EXPORT=1
        ;;
    *)
		err_usage
        ;;
    esac
done

if [ ${EXPORT} ]; then
	do_export $CHANNEL
fi

if [ ${SET_SPEED} ]; then
	do_set_speed $CHANNEL $PULSE_WIDTH
fi

if [ ${ENABLE} ]; then
	do_enable $CHANNEL
fi

if [ ${DISABLE} ]; then
	do_disable $CHANNEL
fi

if [ ${UNEXPORT} ]; then
	do_unexport $CHANNEL
fi

exit 0

