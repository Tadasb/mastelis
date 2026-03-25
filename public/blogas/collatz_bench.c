#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>
#include <unistd.h>

typedef unsigned __int128 uint128;

#define BATCH_SIZE 10000000
#define NUM_THREADS 10
#define TARGET_LIMIT 10000000000ULL // 10 Billion

atomic_uint_least64_t global_count = 0;
uint128 current_nr = 1; 
pthread_mutex_t nr_mutex = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    while (1) {
        uint128 start_nr;
        
        pthread_mutex_lock(&nr_mutex);
        if (current_nr >= TARGET_LIMIT) {
            pthread_mutex_unlock(&nr_mutex);
            break;
        }
        start_nr = current_nr;
        current_nr += BATCH_SIZE;
        pthread_mutex_unlock(&nr_mutex);

        uint128 end_nr = start_nr + BATCH_SIZE;
        if (end_nr > TARGET_LIMIT) end_nr = TARGET_LIMIT;

        for (uint128 nr = start_nr; nr < end_nr; nr++) {
            uint128 n = nr;
            while (n >= nr && n > 1) {
                if (n & 1) {
                    n = (3 * n + 1) >> 1;
                } else {
                    n >>= 1;
                }
            }
        }
        atomic_fetch_add(&global_count, BATCH_SIZE);
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    printf("Benchmarking Collatz: 1 to 10 Billion...\n");
    printf("Threads: %d | Batch Size: %d\n", NUM_THREADS, BATCH_SIZE);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    // Wait for all threads to finish
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed = (end_time.tv_sec - start_time.tv_sec) + 
                     (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    
    printf("\n--- Result ---\n");
    printf("Total numbers checked: 10,000,000,000\n");
    printf("Time elapsed: %.2f seconds\n", elapsed);
    printf("Average Speed: %.2f M/s\n", (10000.0 / elapsed));

    return 0;
}
