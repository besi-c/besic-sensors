// BESI-C Environmental Sensors
//   https://github.com/besi-c/besic-sensors
//   Penn Bauman <pcb8gb@virginia.edu>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <besic.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "device.h"

// global variables
pthread_mutex_t read_lock;
pthread_cond_t read_cv, send_cv;
char *DATA_FILE;
char *PIPE_FILE;
char print = 0;
char *myPipe = BESIC_TMP_DYNAMOFILE;

//

// Print data to console
void printData(besic_data const *d) {
	printf("VEML7700 LUX: %10.2f\n", d->lux);
	printf("TMP117   TMP: %10.2f°C\n", d->tmp);
	printf("MS5607   PRS: %10.2f mBar\n", d->prs);
	printf("HS3002   HUM: %10.2f%%\n", d->hum);
}

// Write data to CSV file
void writeData(besic_data const *d, char const *filename) {
	if(d == NULL) {
		perror("NULL Data");
		exit(1);
	}
	FILE *fp;
	fp = fopen(filename,"a");
	if(fp == NULL) {
		perror("File Write Error");
		exit(1);
	}
	fprintf(fp, "%lld,%f,%f,%f,%f\n", d->timestamp, d->lux, d->tmp, d->prs, d->hum);
	fclose(fp);
}

// Write data to pipe for python program reading
void writePipe(besic_data *sensorData, char const *pipeFile){
	if(sensorData == NULL){
		perror("No sensor data to write to pipe");
		exit(1);
	}
	FILE *fd;
	fd = fopen(pipeFile,"a");
	if(fd == NULL){
		perror("Error writing to pipe");
		exit(1);
	}
	//printData(sensorData);
	fprintf(fd, "%lld,%f,%f,%f,%f\n", sensorData->timestamp, sensorData->lux, sensorData->tmp, sensorData->prs, sensorData->hum);
	fclose(fd);

}


// Sensor reading thread
void *readings_run(void *args) {
	besic_data *reading = (besic_data*)args;
	while (1) {
		pthread_mutex_lock(&read_lock);

		// Read data and start message
		readData(reading);
		pthread_cond_broadcast(&send_cv);
		pthread_mutex_unlock(&read_lock);

		if (!print){
			writeData(reading, DATA_FILE);
			//Also send reading to dynamo, if unsuccessful track line number, set flag, on next inter read from that line to eof for dynamo 
			// since its a temp file wipe on shutdown?
			writePipe(reading,myPipe);
		}
		// Wait for new cycle
		pthread_mutex_lock(&read_lock);
		//get PID, while the PID is waiting, read and send?
		pthread_cond_wait(&read_cv, &read_lock);
		pthread_mutex_unlock(&read_lock);
	}
	return NULL;
}

// Data sending thread
void *sending_run(void *args) {
	besic_data *reading = (besic_data*)args;
	while (1) {
		// Wait for data
		pthread_mutex_lock(&read_lock);
		pthread_cond_wait(&send_cv, &read_lock);

		// Print data
		if (print) {
			printf("#############################\n");
			printData(reading);
			pthread_mutex_unlock(&read_lock);
		} else {
			// Send data heartbeat
			//send to dynamo as well

			besic_data temp_reading;
			memcpy(&temp_reading, reading, sizeof(besic_data));
			pthread_mutex_unlock(&read_lock);
			int res = besic_data_heartbeat(&temp_reading);
			if (res < 0) {
				printf("CURL failed\n");
			} else if (res > 0) {
				printf("HTTP Error %d\n", res);
			}
		}
	}
	return NULL;
}


int main(int argc, char **argv) {

	//Create pipe in temp directory for files
	//int pipeDesc;
	if(access(myPipe, F_OK)==0){
	}
	else{
		mkfifo(myPipe, 0666);
	}
	//Implement write end in C, read end in Python

	//Have read try until acquired?
	//Have python try until read acquired
	//Have python close before send attempt, if send attempt failed, MQTT handles?
	//char writeArray[sizeof(besic_data)];
	//char readArray[sizeof(besic_data)];
	// Print simple test reading
	if ((argc > 1) && (0 == strcmp("test", argv[1]))) {
		besic_data reading;
		readData(&reading);
		printData(&reading);
		return 0;
	}

	// Compute data file location
	char *data_path = besic_data_dir();
	if (data_path == NULL) {
		printf("Data path missing\n");
		return 1;
	}
	DATA_FILE = malloc(strlen(data_path) + 13);
	sprintf(DATA_FILE, "%s/sensors.csv", data_path);

	// Setup console printing
	if ((argc > 1) && (0 == strcmp("print", argv[1]))) {
		print = 1;
		printf("Environmental Sensors\n");
		printf(">> %s\n", DATA_FILE);
	}

	// Initialize mutex and locks
	if (pthread_mutex_init(&read_lock, NULL) != 0) {
		perror("mutex_lock: ");
		return 1;
	}
	if (pthread_cond_init(&read_cv, NULL) != 0) {
		perror("condition: ");
		return 1;
	}
	if (pthread_cond_init(&send_cv, NULL) != 0) {
		perror("condition: ");
		return 1;
	}

	besic_data reading;
	pthread_t readings_thread, sending_thread;

	// Start sensors and data sending thread
	pthread_create(&readings_thread, NULL, readings_run, (void*)&reading);
	pthread_create(&sending_thread, NULL, sending_run, (void*)&reading);

	// Start reading once per second
	while (1) {
		sleep(1);
		pthread_mutex_lock(&read_lock);
		pthread_cond_broadcast(&read_cv);
		pthread_mutex_unlock(&read_lock);
	}
	return 0;
}
