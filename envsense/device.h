// BESI-C Sensor Board Revision 3.0 Driver
//   https://github.com/besi-c/besic-sensors
//   Yudel Martinez <yam3nv@virginia.edu>
//   Penn Bauman <pcb8gb@virginia.edu>
#ifndef DEVICE_H
#define DEVICE_H
#define BESIC_TMP_DYNAMOFILE "var/besic/sensorPipe"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pigpio.h>
#include <besic.h>

float veml7700();
float hs3002();
float tmp117();
float ms5607();

void readData(besic_data *d);

#endif
