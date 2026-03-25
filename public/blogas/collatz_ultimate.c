#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

// Use 128-bit unsigned integer to prevent overflows (Native on M4/ARM64)
typedef unsigned __int128 uint128;

#define NUM_THREADS 10
#define BATCH_SIZE 20000000         // 20 million numbers per batch for high throughput
#define TARGET_LIMIT ((uint64_t)1 << 38) // Target: 2^38 (274.8 Billion)
#define WINDOW_SIZE 4096            // Circular buffer size (must be power of 2)
#define WINDOW_MASK (WINDOW_SIZE - 1)

// Global State (Lock-Free)
atomic_uint_least64_t next_batch_start = 3; 
atomic_uint_least64_t verified_frontier = 3;
atomic_bool batch_results[WINDOW_SIZE];

// M4 Hardware-Optimized Trailing Zero Count (CTZ)
// Replaces the slow 'while (!(n & 1)) n >>= 1;' loop with a single CPU instruction
static inline int count_trailing_zeros(uint128 n) {
    uint64_t low = (uint64_t)n;
    if (low) return __builtin_ctzll(low);
    uint64_t high = (uint64_t)(n >> 64);
    return __builtin_ctzll(high) + 64;
}

void* worker(void* arg) {
    while (1) {
        // 1. ATOMIC WORK FETCHING (Lock-Free)
        // Threads grab a batch of numbers without waiting for each other
        uint64_t start = atomic_fetch_add(&next_batch_start, BATCH_SIZE);
        if (start >= TARGET_LIMIT) break;

        uint64_t end = start + BATCH_SIZE;
        if (end > TARGET_LIMIT) end = TARGET_LIMIT;

        // 2. MATHEMATICAL CALCULATION
        // Skip even numbers (nr += 2) because they trivially drop to n/2
        for (uint64_t nr = start; nr < end; nr += 2) {
            uint128 n = nr;
            
            // Strict Induction: Stop when n drops below the current batch start
            while (n >= start) {
                // Fused Step: (3n + 1) / 2
                // We know 3n+1 is always even, so we do both operations at once
                n = (n + (n << 1) + 1) >> 1;
                
                // Hardware-accelerated removal of all remaining factors of 2
                n >>= count_trailing_zeros(n);
            }
            
            // Anti-Optimization Assembly: Forces compiler to actually compute 'n'
            __asm__ volatile("" : : "r"(n)); 
        }

        // 3. MARK BATCH DONE IN CIRCULAR BUFFER
        // Allows the main thread to track the true verified frontier
        uint64_t batch_id = (start - 3) / BATCH_SIZE;
        atomic_store(&batch_results[batch_id & WINDOW_MASK], true);
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    struct timespec start_time, now_time;
    
    // Initialize circular buffer to false
    for (int i = 0; i < WINDOW_SIZE; i++) atomic_init(&batch_results[i], false);

    printf("====================================================\n");
    printf(" ULTIMATE M4 RIGOROUS COLLATZ VERIFIER\n");
    printf(" Target: 1 to 2^38 (%llu numbers)\n", TARGET_LIMIT);
    printf(" Threads: %d | Batch Size: %d\n", NUM_THREADS, BATCH_SIZE);
    printf("====================================================\n");

    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // Spawn CPU worker threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    uint64_t verified_batch_id = 0;
    while (1) {
        // FRONTIER TRACKING:
        // Advance the "Verified" count only when the absolute oldest pending batch finishes.
        // This guarantees 100% mathematical integrity (no gaps).
        while (atomic_load(&batch_results[verified_batch_id & WINDOW_MASK])) {
            atomic_store(&batch_results[verified_batch_id & WINDOW_MASK], false);
            verified_batch_id++;
            atomic_store(&verified_frontier, 3 + (verified_batch_id * BATCH_SIZE));
        }

        uint64_t current_v = atomic_load(&verified_frontier);
        clock_gettime(CLOCK_MONOTONIC, &now_time);
        
        double elapsed = (now_time.tv_sec - start_time.tv_sec) + 
                         (now_time.tv_nsec - start_time.tv_nsec) / 1e9;
        
        if (current_v >= TARGET_LIMIT) break;

        double percent = (current_v * 100.0) / (double)TARGET_LIMIT;
        double speed = (elapsed > 0) ? ((double)current_v / 1000000.0) / elapsed : 0;
        
        printf("\r[+] Proven: %llu (%.2f%%) | Time: %.1fs | Speed: %.2f M/s", 
               current_v, percent, elapsed, speed);
        fflush(stdout);
        usleep(500000); // UI update every 0.5s
    }

    // Wait for all threads to cleanly exit
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Final Time Calculation
    clock_gettime(CLOCK_MONOTONIC, &now_time);
    double final_elapsed = (now_time.tv_sec - start_time.tv_sec) + 
                           (now_time.tv_nsec - start_time.tv_nsec) / 1e9;

    printf("\n\n====================================================\n");
    printf(" DONE! Verified 100%% of numbers up to 2^38.\n");
    printf(" Final Time: %.2f seconds.\n", final_elapsed);
    printf(" Overall Average Speed: %.2f M/s.\n", ((double)TARGET_LIMIT / 1000000.0) / final_elapsed);
    printf("====================================================\n");

    return 0;
}
