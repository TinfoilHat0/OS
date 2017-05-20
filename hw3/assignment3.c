#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <fcntl.h>

//mutex&signals&semaphores to achieve the desired synchronization
pthread_mutex_t mu;
pthread_cond_t registerSignal;
key_t shmkey; //shared mem. key
int shmid; //shared mem. id
sem_t *sem; //synch semaphore for x-rays
int nXray = 3; // # of x-rays available

int toTreat = 0, nTreated = 0, size = 0, dep_no = 0;
int **treatTimes;

void fileOP(const char* inputFile) {

	int num, num2, i, j;
	//read numbers from the file
	FILE* file = fopen(inputFile, "r");
	while (!feof(file)) {
		fscanf(file, "%d %d", &num, &num);
		size++;
	}
	fclose(file);

	treatTimes = malloc(size * sizeof(int*));
	for (i = 0; i < size; i++) {
		treatTimes[i] = malloc(2 * sizeof(int));
	}

	file = fopen(inputFile, "r");
	for (i = 0; i < size; i++)
		fscanf(file, "%d %d", &treatTimes[i][0], &treatTimes[i][1]);
	fclose(file);

}
void* nurse(void *ptr) {
	int i;
	for (i = 0; i < size; i++) { //go over the patients and register them
		sleep(2); //registering takes 2 time units
		pthread_mutex_lock(&mu);
		printf("Department no: %d Nurse: Patient %d registered  \n", dep_no,
				i + 1);
		toTreat++; //# of patients in waiting room
		pthread_cond_signal(&registerSignal); //let the doctors know that a new patient has just registered
		pthread_mutex_unlock(&mu);

	}
	pthread_cond_signal(&registerSignal);
	pthread_exit(0);
}
void* doctor(int dNo) {
	while (1) {
		pthread_mutex_lock(&mu);
		//if all patients are not treated and there are no registered patients,
		//wait on them to register
		while (toTreat <= 0 && nTreated < size)
			pthread_cond_wait(&registerSignal, &mu); //waiting on a patient to register
		int tmp = nTreated;
		toTreat--;
		nTreated++; //a new patient is accepted for treatment
		pthread_mutex_unlock(&mu);

		if (tmp < size) {
			//treating patient for pats[tmp] time units
			sleep(treatTimes[tmp][0]);
			pthread_mutex_lock(&mu);
			if (!treatTimes[tmp][1])//no x-ray, treatment is finished
				printf("Department no: %d Doctor %d: Patient %d treated \n",
						dep_no, dNo, tmp + 1);
			pthread_mutex_unlock(&mu);
			if (treatTimes[tmp][1]){//x-ray required
				sem_wait(sem); //wait on x-rays to become free if not
				sleep(2);//X-Ray takes 2 time units
				pthread_mutex_lock(&mu);
				printf(
						"Department no: %d Doctor %d: Patient %d treated (X-Ray taken)\n",
						dep_no, dNo, tmp + 1);
				pthread_mutex_unlock(&mu);
				sem_post(sem);//free the x-ray
			}
		}
		if (nTreated >= size) //all patients are treated, break the loop and exit the thread
			break;

	}
	pthread_exit(0);
}

int main(int argc, char **argv) {
	int i, f;
	//initiliaze the shared semaphore
	shmkey = ftok("/dev/null", 5);
	shmid = shmget(shmkey, sizeof(int), 0644 | IPC_CREAT);
	if (shmid < 0) {
		perror("shmget\n");
		exit(1);
	}
	//initial value of semaphore = nXray
	sem = sem_open("pSem", O_CREAT | O_EXCL, 0644, nXray);
	sem_unlink("pSem");

	//creating 5 child process'(departments)
	for (i = 0; i < argc-1; i++) {
		f = fork();
		if (f == 0) {
			dep_no = i + 1;
			break;
		} else if (f == -1) {
			printf("Fork error! Restart the program");
			return 0;
		}
	}
	//Department running
	if (f == 0) {
		pthread_t nurse_t, doc1_t, doc2_t;

		//initializing mutex vars&reading data from file
		pthread_mutex_init(&mu, NULL);
		pthread_cond_init(&registerSignal, NULL);
		fileOP(argv[dep_no]);

		//creating threads
		pthread_create(&nurse_t, NULL, nurse, NULL);
		pthread_create(&doc1_t, NULL, doctor, 1);
		pthread_create(&doc2_t, NULL, doctor, 2);

		//Don't let a process to terminate before nurse and doctor threads terminate
		pthread_join(doc1_t, NULL);
		pthread_join(doc2_t, NULL);
		pthread_join(nurse_t, NULL);

		//deallocate mutex&arrays
		pthread_mutex_destroy(&mu);
		pthread_cond_destroy(&registerSignal);
		free(treatTimes);
	} else {
		//Don't let the main to terminate before child process' terminate
		wait(NULL);
		//clean semaphores
		shmctl(shmid, IPC_RMID, 0);
		sem_destroy(sem);
	}

	return 0;

}
