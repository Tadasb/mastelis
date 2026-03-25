#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

typedef unsigned __int128 uint128;

#define NUM_THREADS 10
#define BATCH_SIZE 10000000
#define TARGET_LIMIT ((uint64_t)1 << 38) // 274,877,906,944
#define MAX_PENDING 2000 // How many batches we can track at once in the sliding window

// Global State
uint128 global_nr = 3;                // Next number to be assigned for checking
uint128 verified_frontier = 3;        // Highest number where EVERYTHING below is mathematically proven
atomic_bool batch_done[MAX_PENDING];  // Tracking window: which batches are finished?
uint64_t window_start_batch = 0;      // The index of the first batch in our sliding window

pthread_mutex_t work_mutex = PTHREAD_MUTEX_INITIALIZER;

void* worker(void* arg) {
    while (1) {
        uint128 start_nr;
        uint64_t batch_idx;

        // 1. GRAB WORK: Thread-safe assignment of a block of numbers
        pthread_mutex_lock(&work_mutex);
        if (global_nr >= TARGET_LIMIT) {
            pthread_mutex_unlock(&work_mutex);
            break;
        }
        start_nr = global_nr;
        // Batch index is the distance from 3 divided by the batch size
        batch_idx = (uint64_t)((start_nr - 3) / BATCH_SIZE);
        global_nr += BATCH_SIZE;
        pthread_mutex_unlock(&work_mutex);

        // 2. CALCULATE: Strict Induction Logic
        uint128 end_nr = start_nr + BATCH_SIZE;
        if (end_nr > TARGET_LIMIT) end_nr = TARGET_LIMIT;

        for (uint128 nr = start_nr; nr < end_nr; nr += 2) {
            uint128 n = nr;
            // Induction Principle: If n drops below start_nr, and we are 
            // verified up to start_nr, then nr is proven to lead to 1.
            while (n >= start_nr) {
                n = (3 * n + 1) >> 1;
                while (!(n & 1)) n >>= 1;
            }
            // Prevent compiler from skipping the loop
            __asm__ volatile("" : : "r"(n)); 
        }

        // 3. MARK BATCH AS DONE
        // Update the sliding window bitmask
        pthread_mutex_lock(&work_mutex);
        uint64_t relative_idx = batch_idx - window_start_batch;
        if (relative_idx < MAX_PENDING) {
            atomic_store(&batch_done[relative_idx], true);
        }
        pthread_mutex_unlock(&work_mutex);
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    struct timespec start_time, now_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // Initialize the tracking window to false
    for (int i = 0; i < MAX_PENDING; i++) atomic_init(&batch_done[i], false);

    printf("RIGOROUS M4 COLLATZ VERIFIER\n");
    printf("Target: 1 to 2^38 (%llu numbers)\n", TARGET_LIMIT);
    printf("Threads: %d | Batch: %d\n", NUM_THREADS, BATCH_SIZE);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    while (1) {
        // SLIDING WINDOW LOGIC: Move the "Verified Frontier" forward
        // only when the OLDEST pending batches are all finished.
        pthread_mutex_lock(&work_mutex);
        int shifts = 0;
        while (atomic_load(&batch_done[0])) {
            // Shift the boolean window left by one
            for (int i = 0; i < MAX_PENDING - 1; i++) {
                atomic_store(&batch_done[i], atomic_load(&batch_done[i+1]));
            }
            atomic_store(&batch_done[MAX_PENDING - 1], false);
            window_start_batch++;
            verified_frontier += BATCH_SIZE;
            shifts++;
            if (shifts > 100) break; // Don't block the UI forever
        }
        uint128 current_v = verified_frontier;
        pthread_mutex_unlock(&work_mutex);

        clock_gettime(CLOCK_MONOTONIC, &now_time);
        double elapsed = (now_time.tv_sec - start_time.tv_sec) + 
                         (now_time.tv_nsec - start_time.tv_nsec) / 1e9;
        
        if (current_v >= TARGET_LIMIT) break;

        double percent = ((double)current_v * 100.0) / (double)TARGET_LIMIT;
        double speed = (elapsed > 0) ? ((double)current_v / 1000000.0) / elapsed : 0;
        
        printf("\rProven: %llu (%.2f%%) | Time: %.0fs | Speed: %.2f M/s", 
               (uint64_t)current_v, percent, elapsed, speed);
        fflush(stdout);
        sleep(1);
    }

    for (int i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &now_time);
    double final_elapsed = (now_time.tv_sec - start_time.tv_sec) + 
                           (now_time.tv_nsec - start_time.tv_nsec) / 1e9;

    printf("\n--- COMPLETE ---\n");
    printf("Verified 100%% of numbers up to 2^38.\n");
    printf("Final Time: %.2f seconds.\n", final_elapsed);
    printf("Overall Speed: %.2f M/s.\n", ((double)TARGET_LIMIT / 1000000.0) / final_elapsed);

    return 0;
}
