#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>
#include <unistd.h>

// M4 supports __int128 for numbers up to 2^128
typedef unsigned __int128 uint128;

// Initializing 2^68
#define BATCH_SIZE 20000000
#define NUM_THREADS 12

atomic_uint_least64_t global_count = 0;
uint128 current_nr; 
pthread_mutex_t nr_mutex = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    while (1) {
        uint128 start_nr;
        
        // Get next batch
        pthread_mutex_lock(&nr_mutex);
        start_nr = current_nr;
        current_nr += BATCH_SIZE;
        pthread_mutex_unlock(&nr_mutex);

        uint128 end_nr = start_nr + BATCH_SIZE;

        // SKIP EVEN NUMBERS: We only check odd numbers (nr += 2)
        // This instantly doubles the speed because even numbers 
        // are trivial (n/2 < n).
        for (uint128 nr = start_nr; nr < end_nr; nr += 2) {
            uint128 n = nr;
            
            // Core logic for ODD numbers
            while (n >= nr) {
                // If odd: (3n + 1) / 2
                // We know 3n+1 is even, so we always shift at least once
                n = (3 * n + 1) >> 1;
                
                // Remove all additional trailing zeros (factors of 2)
                if (n > 0) {
                    while (!(n & 1)) {
                        n >>= 1;
                    }
                }
            }
        }
        // We report BATCH_SIZE even though we only calculated half,
        // to keep the "numbers checked" count consistent with the range.
        atomic_fetch_add(&global_count, BATCH_SIZE);
    }
    return NULL;
}

int main() {
    // Initialize current_nr to 2^68 (must be even for the +=2 logic to work on odds)
    current_nr = (uint128)1 << 68;
    // Start at the first odd number
    current_nr += 1;

    pthread_t threads[NUM_THREADS];
    struct timespec start_time, now_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    printf("Starting M4 ULTIMATE Collatz verification at 2^68...\n");
    printf("Threads: %d | Batch Size: %d | Skipping Evens: YES\n", NUM_THREADS, BATCH_SIZE);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    while (1) {
        sleep(1);
        clock_gettime(CLOCK_MONOTONIC, &now_time);
        double elapsed = (now_time.tv_sec - start_time.tv_sec) + 
                         (now_time.tv_nsec - start_time.tv_nsec) / 1e9;
        uint64_t total = atomic_load(&global_count);
        
        if (elapsed > 0) {
            printf("\rGreitis: %.2f M/s | Viso patikrinta: %llu | Laikas: %.0fs", 
                   (total / elapsed) / 1000000.0, total, elapsed);
            fflush(stdout);
        }
    }

    return 0;
}
