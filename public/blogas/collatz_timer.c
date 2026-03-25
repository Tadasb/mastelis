#include <stdio.h>      // Standartinė įvesties/išvesties biblioteka (skirta printf)
#include <stdint.h>     // Biblioteka skirta tiksliems kintamųjų tipams (pvz., uint64_t)
#include <pthread.h>    // Gijų (threads) valdymo biblioteka lygiagrečiam skaičiavimui
#include <time.h>       // Laiko matavimo biblioteka (skirta clock_gettime)
#include <stdatomic.h>  // Atominės operacijos (saugu naudoti tarp gijų be mutex)
#include <unistd.h>     // Sisteminės funkcijos (skirta usleep)

typedef unsigned __int128 uint128; // Sukuriamas tipas 128 bitų skaičiams (M4 palaiko hardware lygmeniu)

#define BATCH_SIZE 10000000              // Kiek skaičių viena gija pasiima vienu kartu (paketas)
#define NUM_THREADS 10                   // Naudojamas gijų (CPU branduolių) skaičius
#define TARGET_LIMIT ((uint64_t)1 << 42) // Tikslas: 2^42 (apie 4.4 trilijonus skaičių)

atomic_uint_least64_t global_count = 0;   // Atominis skaitiklis, rodantis kiek iš viso patikrinta
volatile uint64_t checksum = 0;           // Kintamasis, kuris priverčia procesorių tikrai atlikti matematiką
pthread_mutex_t nr_mutex = PTHREAD_MUTEX_INITIALIZER;    // Užraktas, kad gijos nesipjautų imdamos skaičius
pthread_mutex_t check_mutex = PTHREAD_MUTEX_INITIALIZER; // Užraktas galutiniam rezultatų suvedimui

uint128 current_nr = 3; // Pradinis skaičius (1 ir 2 yra trivialūs, pradedam nuo 3)

void* worker(void* arg) { // Funkcija, kurią vykdo kiekvienas CPU branduolys
    uint64_t local_checksum = 0; // Gijos vidinis skaitiklis (kad nereikėtų nuolat rašyti į pagrindinę atmintį)
    while (1) { // Begalinis ciklas, kol pasieksime TARGET_LIMIT
        uint128 start_nr; // Kintamasis gijos paketo pradžiai saugoti
        
        pthread_mutex_lock(&nr_mutex); // Užrakiname prieigą prie bendro skaitiklio
        if (current_nr >= TARGET_LIMIT) { // Jei pasiekėme tikslą
            pthread_mutex_unlock(&nr_mutex); // Atrakiname
            break; // Nutraukiame darbą
        }
        start_nr = current_nr; // Pasiimame dabartinę reikšmę
        current_nr += BATCH_SIZE; // Padidiname bendrą skaitiklį kitai gijai
        pthread_mutex_unlock(&nr_mutex); // Atrakiname

        uint128 end_nr = start_nr + BATCH_SIZE; // Apskaičiuojame paketo pabaigą
        if (end_nr > TARGET_LIMIT) end_nr = TARGET_LIMIT; // Užtikriname, kad neviršysime tikslo

        for (uint128 nr = start_nr; nr < end_nr; nr += 2) { // Ciklas per ODD (nelyginius) skaičius (+2)
            uint128 n = nr; // Pradedame skaičiavimą nuo dabartinio skaičiaus
            while (n >= nr) { // Vykdome, kol n nenukrenta žemiau pradinio (indukcijos principas)
                n = (3 * n + 1) >> 1; // Collatz žingsnis: (3n+1) ir iškart daliname iš 2
                while (!(n & 1)) n >>= 1; // Jei skaičius vis dar lyginis, daliname iš 2 kol taps nelyginis
            }
            local_checksum += (uint64_t)n; // Pridedame rezultatą prie gijos sumos (priverčia CPU skaičiuoti)
        }
        atomic_fetch_add(&global_count, BATCH_SIZE); // Atomiškai pranešame apie užbaigtą paketą
    }
    
    pthread_mutex_lock(&check_mutex); // Baigus darbą, užrakiname galutinę sumą
    checksum += local_checksum; // Pridedame gijos indėlį prie bendros sumos
    pthread_mutex_unlock(&check_mutex); // Atrakiname
    return NULL; // Gija baigia darbą
}

int main() { // Pagrindinė programa
    pthread_t threads[NUM_THREADS]; // Masyvas gijų identifikatoriams
    struct timespec start_time, end_time; // Struktūros tiksliam laiko matavimui

    printf("Starting BRUTE FORCE M4 Benchmark: 1 to 2^42...\n"); // Informacinis pranešimas
    printf("Threads: %d | Batch Size: %d\n", NUM_THREADS, BATCH_SIZE); // Parametrai

    clock_gettime(CLOCK_MONOTONIC, &start_time); // Užfiksuojame skaičiavimo pradžios laiką

    for (int i = 0; i < NUM_THREADS; i++) { // Sukuriame nurodytą kiekį gijų
        pthread_create(&threads[i], NULL, worker, NULL); // Paleidžiame kiekvieną giją
    }

    while (1) { // Ciklas ekrano atnaujinimui (progresas)
        uint64_t total = atomic_load(&global_count); // Nuskaitome dabartinį progresą
        if (total >= TARGET_LIMIT) break; // Jei baigta, išeiname iš ciklo
        
        double percent = (total * 100.0) / TARGET_LIMIT; // Apskaičiuojame procentus
        printf("\rProgress: %.2f%% | Patikrinta: %llu", percent, total); // Spausdiname progresą toje pačioje eilutėje
        fflush(stdout); // Išvalome buferį, kad tekstas iškart pasirodytų
        usleep(500000); // Palaukiame pusę sekundės prieš kitą atnaujinimą
    }

    for (int i = 0; i < NUM_THREADS; i++) { // Laukiame, kol visos gijos baigs darbą
        pthread_join(threads[i], NULL); // Suliejame gijas su pagrindine programa
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time); // Užfiksuojame pabaigos laiką
    double elapsed = (end_time.tv_sec - start_time.tv_sec) + 
                     (end_time.tv_nsec - start_time.tv_nsec) / 1e9; // Suskaičiuojame praėjusį laiką sekundėmis
    
    printf("\n--- Result ---\n"); // Rezultatų skiltis
    printf("Total numbers checked: %llu\n", TARGET_LIMIT); // Kiek iš viso patikrinta
    printf("Time elapsed: %.2f seconds\n", elapsed); // Kiek laiko užtruko
    printf("Checksum (verification): %llu\n", checksum); // Patikros suma (įrodo, kad darbas atliktas)
    printf("TRUE Average Speed: %.2f M/s\n", ((double)TARGET_LIMIT / 1000000.0) / elapsed); // Vidutinis greitis

    return 0; // Programos pabaiga
}
