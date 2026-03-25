#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>

typedef unsigned __int128 uint128;

// Statiniam testui: 1 iki 2^41
#define NUM_THREADS 10
#define TARGET_LIMIT ((uint64_t)1 << 41) // 2,199,023,255,552
#define QUOTA_PER_THREAD (TARGET_LIMIT / NUM_THREADS)

atomic_uint_least64_t global_count = 0;

void* worker(void* arg) {
    uint64_t id = (uintptr_t)arg;
    uint128 start = QUOTA_PER_THREAD * id;
    uint128 end = QUOTA_PER_THREAD * (id + 1);
    uint128 nr = start | 1;
    uint64_t processed_in_batch = 0;

    for (; nr < end; nr += 2) {
        uint128 n = nr;
        while (n >= nr) {
            n = (3 * n + 1) >> 1;
            while (!(n & 1)) n >>= 1;
        }
        __asm__ volatile("" : : "r"(n)); 
        processed_in_batch += 2;
        if (processed_in_batch >= 100000000) {
            atomic_fetch_add(&global_count, processed_in_batch);
            processed_in_batch = 0;
        }
    }
    atomic_fetch_add(&global_count, processed_in_batch);
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    struct timespec start_time, now_time;
    
    printf("STATINIS M4 TESTAS: 1 iki 2^41 (%llu skaičių)\n", TARGET_LIMIT);
    printf("Gijos: %d | Rėžis per giją: %llu\n", NUM_THREADS, QUOTA_PER_THREAD);

    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (uintptr_t i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, (void*)i);
    }

    // Progresas ekrane su realaus laiko greičiu ir trukme
    while (1) {
        uint64_t total = atomic_load(&global_count);
        clock_gettime(CLOCK_MONOTONIC, &now_time);
        
        double elapsed = (now_time.tv_sec - start_time.tv_sec) + 
                         (now_time.tv_nsec - start_time.tv_nsec) / 1e9;
        
        if (total >= TARGET_LIMIT) break;
        
        double percent = (total * 100.0) / TARGET_LIMIT;
        double speed = (elapsed > 0) ? ((double)total / 1000000.0) / elapsed : 0;
        
        printf("\rProgresas: %.2f%% | Laikas: %.0fs | Greitis: %.2f M/s | Patikrinta: %llu", 
               percent, elapsed, speed, total);
        fflush(stdout);
        sleep(1);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &now_time);
    double final_elapsed = (now_time.tv_sec - start_time.tv_sec) + 
                           (now_time.tv_nsec - start_time.tv_nsec) / 1e9;
    
    printf("\n--- REZULTATAI ---\n");
    printf("Viso užtruko: %.2f sekundės\n", final_elapsed);
    printf("Galutinis vidutinis greitis: %.2f M/s\n", ((double)TARGET_LIMIT / 1000000.0) / final_elapsed);

    return 0;
}
