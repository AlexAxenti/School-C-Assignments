#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

enum {THINKING, HUNGRY, EATING} state[5];
pthread_cond_t forks[5];
pthread_mutex_t mutex;

void *runner(void *param);
void pickup(int i);
void putdown(int i);
void test(int i);

int main(int argc, char *argv[]) {
    
    // Set the three thread ids
    pthread_t phil[5];
    
    // Initialize the mutex lock as well as srand
    pthread_mutex_init(&mutex, NULL);
    srand(time(NULL));

    // Initialize the philosophers' state and the condition variables
    for(int i = 0; i < 5; i++){
        state[i] = THINKING;
        pthread_cond_init(&forks[i], NULL);
    }

    // After everything else is initialized, then the threads can start running
    int phil_num[5];
    for(int i = 0; i < 5; i++){
        phil_num[i] = i;
        pthread_create(&phil[i], NULL, runner, (void *) &phil_num[i]);
    }
    
    // close the five threads
    for(int i = 0; i < 5; i++){
        pthread_join(phil[i], NULL);
    }
}

void *runner(void *param){
    int phil_num = *((int *)param);
    int activityLength;

    // The assignment says they alternate between eating and sleeping, and I selected 1 iteration in the for loop below for test case 1, 
    // and 3 iterations for test case 2.
    for(int i = 0; i < 3; i++){
        // The philosopher waits before eating so that the order in which they first begin eating is random
        activityLength = (rand() % (3 - 1 + 1)) + 1; // Random int from 1 to 3
        sleep(activityLength);
        // After waiting, start eating
        pickup(phil_num);
        printf("Philosopher %d has begun eating\n", phil_num);

        activityLength = (rand() % (3 - 1 + 1)) + 1; // Random int from 1 to 3
        sleep(activityLength);
        // After eating, go back to thinking
        putdown(phil_num);
        printf("Philosopher %d has begun thinking\n", phil_num);
    }
    
    pthread_exit(0);
}

void pickup(int i){
    pthread_mutex_lock(&mutex);
    state[i] = HUNGRY;
    test(i);
    while(state[i] != EATING){
        pthread_cond_wait(&forks[i], &mutex);
    }
    pthread_mutex_unlock(&mutex);
}

void putdown(int i){
    pthread_mutex_lock(&mutex);
    state[i] = THINKING;
    test((i + 4) % 5);
    test((i + 1) % 5);
    pthread_mutex_unlock(&mutex);
}

void test(int i){
    if ((state[(i + 4) % 5] != EATING) && (state[i] == HUNGRY) && (state[(i + 1) % 5] != EATING)) {
        state[i] = EATING;
        pthread_cond_signal(&forks[i]);
    }
}