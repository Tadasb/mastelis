#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

typedef unsigned __int128 uint128;

#define NUM_THREADS 10
#define BATCH_SIZE 20000000         // Didesnis paketas geresniam našumui
#define TARGET_LIMIT ((uint64_t)1 << 38)
#define WINDOW_SIZE 4096            // Žiedinis buferis (turi būti 2 galia)
#define WINDOW_MASK (WINDOW_SIZE - 1)

// Globali būsena
atomic_uint_least64_t next_batch_start = 3; 
atomic_uint_least64_t verified_frontier = 3;
atomic_bool batch_results[WINDOW_SIZE];

// M4 optimizuotas nulių skaičiavimas (Trailing Zero Count)
// Tai pakeičia lėtą 'while' ciklą viena procesoriaus instrukcija
static inline int count_trailing_zeros(uint128 n) {
    uint64_t low = (uint64_t)n;
    if (low) return __builtin_ctzll(low);
    uint64_t high = (uint64_t)(n >> 64);
    return __builtin_ctzll(high) + 64;
}

void* worker(void* arg) {
    while (1) {
        // 1. ATOMINIS DARBO PASIĖMIMAS (be Lock'ų)
        uint64_t start = atomic_fetch_add(&next_batch_start, BATCH_SIZE);
        if (start >= TARGET_LIMIT) break;

        uint64_t end = start + BATCH_SIZE;
        if (end > TARGET_LIMIT) end = TARGET_LIMIT;

        // 2. SKAIČIAVIMAS (Indukcija: n < nr)
        for (uint64_t nr = start; nr < end; nr += 2) {
            uint128 n = nr;
            while (n >= nr) {
                // (3n + 1) / 2
                n = (n + (n << 1) + 1) >> 1;
                // Iškart pašalinam visus likusius nulius naudojant M4 instrukciją
                n >>= count_trailing_zeros(n);
            }
            __asm__ volatile("" : : "r"(n)); 
        }

        // 3. ŽYMEJIMAS (Žiedinis buferis)
        uint64_t batch_id = (start - 3) / BATCH_SIZE;
        atomic_store(&batch_results[batch_id & WINDOW_MASK], true);
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    struct timespec start_time, now_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (int i = 0; i < WINDOW_SIZE; i++) atomic_init(&batch_results[i], false);

    printf("ULTIMATE M4 RIGOROUS VERIFIER (2^38)\n");

    for (int i = 0; i < NUM_THREADS; i++) pthread_create(&threads[i], NULL, worker, NULL);

    uint64_t verified_batch_id = 0;
    while (1) {
        // FRONTIER LOGIKA: Judam į priekį tik jei seniausias paketas baigtas
        while (atomic_load(&batch_results[verified_batch_id & WINDOW_MASK])) {
            atomic_store(&batch_results[verified_batch_id & WINDOW_MASK], false);
            verified_batch_id++;
            atomic_store(&verified_frontier, 3 + (verified_batch_id * BATCH_SIZE));
        }

        uint64_t current_v = atomic_load(&verified_frontier);
        clock_gettime(CLOCK_MONOTONIC, &now_time);
        double elapsed = (now_time.tv_sec - start_time.tv_sec) + (now_time.tv_nsec - start_time.tv_nsec) / 1e9;
        
        if (current_v >= TARGET_LIMIT) break;

        printf("\rProven: %llu (%.2f%%) | Time: %.1fs | Speed: %.2f M/s", 
               current_v, (current_v * 100.0) / (double)TARGET_LIMIT, elapsed, ((double)current_v / 1000000.0) / elapsed);
        fflush(stdout);
        usleep(500000);
    }

    for (int i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], NULL);
    printf("\n--- VERIFIED 100%% UP TO 2^38 ---\n");
    return 0;
}
