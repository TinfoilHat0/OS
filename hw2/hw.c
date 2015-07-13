#include <stdio.h>
#include <pthread.h>
#include <time.h>

//mutex&signals to achieve desired synchronization
pthread_mutex_t mu;
pthread_cond_t registerSignal;

int toTreat = 0, nTreated = 0, size = 0;
int *treatTimes;
FILE *outputF;

void fileOP(const char* inputFile, const char* outputFile) {

	int num, i;
	//read numbers from the file
	FILE* file = fopen(inputFile, "r");
	while (!feof(file)) {
		fscanf(file, "%d", &num);
		size++;
	}
	fclose(file);

	treatTimes = (int*) malloc(size * sizeof(int));
	file = fopen(inputFile, "r");
	for (i = 0; i < size; i++) {
		fscanf(file, "%d", &num);
		treatTimes[i] = num;
	}
	fclose(file);

	//open the output file
	outputF = fopen(outputFile, "w");

}
void* nurse(void *ptr) {
	int i;
	for (i = 0; i < size; i++) { //go over the patients and register them
		pthread_mutex_lock(&mu);
		printf("Nurse: Patient %d registered  \n", i + 1);
		fprintf(outputF, "Nurse: Patient %d registered  \n", i + 1);
		toTreat++; //# of patients in waiting room
		pthread_cond_signal(&registerSignal); //let the doctors know that a new patient has just registered
		pthread_mutex_unlock(&mu);
		sleep(2); //registering takes 2 time units

	}
	pthread_cond_signal(&registerSignal);
	//printf("Nurse exits\n");
	pthread_exit(0);
}
void* doctor(int dNo) {
	while (1) {
		pthread_mutex_lock(&mu);
		//if all patients are not treated and there are no registered patients,
		//wait on them to register
		while (toTreat <= 0 && nTreated < size) {
			//printf("Doctor %d waits. nTreated %d\n", dNo, nTreated);
			pthread_cond_wait(&registerSignal, &mu); //waiting on a patient to register
		}
		int tmp = nTreated;
		toTreat--;
		nTreated++; //a new patient is accepted for treatment
		pthread_mutex_unlock(&mu);

		if (tmp < size) {
			sleep(treatTimes[tmp]); //treating patient for pats[tmp] time units
			pthread_mutex_lock(&mu);
			printf("Doctor %d: Patient %d treated\n", dNo, tmp + 1);
			fprintf(outputF, "Doctor %d: Patient %d treated\n", dNo, tmp + 1);
			pthread_mutex_unlock(&mu);
		}
		if (nTreated >= size) //all patients are treated, break the loop and exit the thread
			break;

	}
	//printf("Doctor %d exits. Treated = %d\n", dNo, nTreated);
	pthread_exit(0);
}

int main(int argc, char **argv) {

	pthread_t nurse_t, doc1_t, doc2_t;

	//initializing mutex vars&reading data from file
	pthread_mutex_init(&mu, NULL);
	pthread_cond_init(&registerSignal, NULL);
	fileOP("numbers.txt", "output.txt");

	//creating threads
	pthread_create(&nurse_t, NULL, nurse, NULL);
	pthread_create(&doc1_t, NULL, doctor, 1);
	pthread_create(&doc2_t, NULL, doctor, 2);

	//Don't let main to terminate before nurse and doctor threads terminate
	pthread_join(doc1_t, NULL);
	pthread_join(doc2_t, NULL);
	pthread_join(nurse_t, NULL);

	//deallocate mutex&arrays n close output file
	pthread_mutex_destroy(&mu);
	pthread_cond_destroy(&registerSignal);
	free(treatTimes);
	fclose(outputF);

	return 0;

}
