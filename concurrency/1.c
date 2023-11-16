#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <limits.h>

#define CYAN "\033[0;36m"
#define YELLOW "\033[0;33m"
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define BLUE "\033[0;34m"
#define RESET "\033[0m"

struct customer{
    int arrival_time;
    char coffee_type[50];
    int coffee_preparation_time;
    int tolerance_time;
};

sem_t semaphores[1000];
sem_t mutex;

int semaphores_count;
int simulation_time = 0;
int remaining_customers;
int baristas_busy = 0;
int total_customers;
int taken_coffee = 0;
int customers_waiting = 0;
int total_wait_time = 0;

struct customer customer_list[1000];
bool prepared[1000] = {false};
int customer_waitlist[1000];
bool customer_in_waitlist[1000] = {false};
int last_completed[1000] = {0};

void *barista_thread(void *barista_id){
    int id = *(int *)barista_id;
    bool busy = false;
    int time_left_to_prepare = 0;
    int customer_id;
    while (1){
        sem_wait(&semaphores[id+total_customers]);
        sem_wait(&mutex);

        if (!remaining_customers && !baristas_busy) break;
        if (!busy && simulation_time > last_completed[id]){
            if (customers_waiting > 0){
                int min_customer_id = INT_MAX;
                int min_customer_index = -1;
                for (int i = 0; i < customers_waiting; i++){
                    if (simulation_time > customer_list[customer_waitlist[i]].arrival_time){
                        if (customer_waitlist[i] < min_customer_id){
                            min_customer_id = customer_waitlist[i];
                            min_customer_index = i;
                        }
                    }
                }
                if (min_customer_index != -1){
                    customer_id = min_customer_id;
                    printf(CYAN);
                    printf("Barista %d begins preparing the order of customer %d at %d second(s)\n", id+1, customer_id+1, simulation_time);
                    printf(RESET);
                    total_wait_time += simulation_time - customer_list[customer_id].arrival_time;
                    busy = true;
                    baristas_busy++;
                    time_left_to_prepare = customer_list[customer_id].coffee_preparation_time;
                    customers_waiting--;
                    for (int i = min_customer_index; i < customers_waiting; i++){
                        customer_waitlist[i] = customer_waitlist[i+1];
                    }
                    customer_in_waitlist[customer_id] = false;
                }
            }
        }
        if (busy){
            time_left_to_prepare--;
            if (time_left_to_prepare == 0){
                printf(BLUE);
                printf("Barista %d completes the order of customer %d at %d second(s)\n", id+1, customer_id+1, simulation_time + 1);
                printf(RESET);
                last_completed[id] = simulation_time + 1;
                prepared[customer_id] = true;
                busy = false;
                baristas_busy--;
            }
        }

        if (id+total_customers+1 == semaphores_count){
            simulation_time++;
        }
        sem_post(&mutex);
        sem_post(&semaphores[(id+total_customers+1)%semaphores_count]);
    }
    sem_post(&mutex);
    sem_post(&semaphores[(id+total_customers+1)%semaphores_count]);
}

void *customer_thread(void *customer_id){
    int id = *(int *)customer_id;
    struct customer customer = customer_list[id];
    bool done = false;
    while (1){
        sem_wait(&semaphores[id]);
        sem_wait(&mutex);
        if (!remaining_customers && !baristas_busy) break;
        if (!done) {
            if (customer.arrival_time == simulation_time){
                printf("Customer %d arrives at %d second(s)\n", id+1, simulation_time);
                printf(YELLOW);
                printf("Customer %d orders a %s\n", id+1, customer.coffee_type);
                printf(RESET);
                customer_waitlist[customers_waiting++] = id;
                customer_in_waitlist[id] = true;
            }
            else if (prepared[id]){
                printf(GREEN);
                printf("Customer %d leaves with their order at %d second(s)\n", id+1, simulation_time);
                printf(RESET);
                remaining_customers--;
                taken_coffee++;
                done = true;
            }
            else if (simulation_time - customer.arrival_time > customer.tolerance_time){
                printf(RED);
                printf("Customer %d leaves without their order at %d second(s)\n", id+1, simulation_time);
                printf(RESET);
                remaining_customers--;
                if (customer_in_waitlist[id]){
                    for (int i = 0; i < customers_waiting; i++){
                        if (customer_waitlist[i] == id){
                            for (int j = i; j < customers_waiting-1; j++){
                                customer_waitlist[j] = customer_waitlist[j+1];
                            }
                            customers_waiting--;
                            customer_in_waitlist[id] = false;
                            break;
                        }
                    }
                    total_wait_time += simulation_time - customer_list[id].arrival_time;
                }
                done = true;
            }
        }
        sem_post(&mutex);
        sem_post(&semaphores[id+1]);
    }
    sem_post(&mutex);
    sem_post(&semaphores[id+1]);
}

int main(){
    int B, K, N;
    scanf("%d %d %d", &B, &K, &N);

    total_customers = N;
    remaining_customers = N;

    char* coffee_types[K];
    int coffee_times_to_prepare[K];
    for (int i = 0; i < K; i++){
        coffee_types[i] = (char *)malloc(50 * sizeof(char));
        scanf("%s %d", coffee_types[i], &coffee_times_to_prepare[i]);
    }

    for (int i = 0; i < N; i++){
        char coffee_type[50];
        int temp_id;
        scanf("%d %s %d %d", &temp_id, coffee_type, &customer_list[i].arrival_time, &customer_list[i].tolerance_time);
        strcpy(customer_list[i].coffee_type, coffee_type);
        for (int j = 0; j < K; j++){
            if (strcmp(coffee_type, coffee_types[j]) == 0){
                customer_list[i].coffee_preparation_time = coffee_times_to_prepare[j];
                break;
            }
        }
    }

    semaphores_count = B + N;
    for (int i = 0; i < B + N; i++){
        sem_init(&semaphores[i], 0, 0);
    }
    sem_init(&mutex, 0, 1);

    pthread_t threads[B + N];
    int customer_ids[N];
    int barista_ids[B];
    for (int i = 0; i < N; i++){
        customer_ids[i] = i;
    }
    for (int i = 0; i < B; i++){
        barista_ids[i] = i;
    }

    for (int i = 0; i < N; i++){
        pthread_create(&threads[i], NULL, customer_thread, (void *)&customer_ids[i]);
    }

    for (int i = 0; i < B; i++){
        pthread_create(&threads[N+i], NULL, barista_thread, (void *)&barista_ids[i]);
    }

    sem_post(&semaphores[0]);

    for (int i = 0; i < B + N; i++){
        pthread_join(threads[i], NULL);
    }

    for (int i = 0; i < K; i++){
        free(coffee_types[i]);
    }

    int coffees_prepared = 0;
    for (int i = 0; i < N; i++){
        if (prepared[i]){
            coffees_prepared++;
        }
    }
    printf("\n%d coffee wasted\n\n", coffees_prepared - taken_coffee);
    printf("Average waiting time: %.2f\n", (float)total_wait_time / (float)N);
    printf("Average waiting time is %.2f more than when the cafe has infinite baristas\n", (float)total_wait_time / (float)N - 1);
}