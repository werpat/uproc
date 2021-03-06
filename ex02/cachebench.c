#define X_STRIDE 1024
#define Y_STRIDE 4096
#define CACHE_LINE_ARR_LEN (X_STRIDE * Y_STRIDE)
#define MAX_STRIDE 256

/*
 * Uncomment to use linear shift feedback register for random access generation.
 * Comment out to use stride method with n-1 as stride value.
 * */
#define CACHE_SIZE_METHOD_LSFR

#define CACHE_LINE_LEN 64
/* N*CACHE_LINE_LEN results in 32 MiB */
#define CACHE_SIZE_ARR_LEN (1024*512)
/* if CACHE_SIZE_ARR_LEN > 2^20 - 1 this must be adjusted to a huger linear shift feedback polynom */
#define LSFR_NEXT(lsfr) {lsfr = ((((lsfr >> 0) ^ (lsfr >> 3)) & 1) << 19) | (lsfr >> 1);}


#define ITERATIONS 50

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//defines RDTSCP_SUPPORTED if supported, generated by the Makefile
#include "options.h"

static inline uint64_t cycle_count(){
    uint32_t hi, lo;
#ifdef RDTSCP_SUPPORTED
    //read performance counter in EDX:EAX, ECX contains a identifier (->get clobbed)
    asm volatile(   "rdtscp" : "=a" (lo), "=d" (hi): :"ecx");
#else
    //execute lfence to serialize the instruction stream(proposed by intel docu), then perform a rdtsc -> same as rdtscp without serialization
    asm volatile(   "lfence \n"
                    "rdtsc  \n"
                    : "=a" (lo), "=d" (hi) : : "ecx");
#endif
    return ((uint64_t) hi << 32) | lo;
}

static inline void flush_cache(volatile void *ptr, size_t len){
    for(size_t i = 0; i < len; i++, ptr++)
        asm volatile ("clflush (%0)" : : "r"(ptr));
}

uint64_t stride(volatile uint8_t *arr, size_t len, size_t i) {
    uint64_t start, end;
    flush_cache(arr, len);

    start = cycle_count();

    //compiler doesn't touch volatile
    for(size_t x = 0; x < len; x += i)
        arr[x];

    end = cycle_count();
    return end - start;
}

int init_arr(void *mem, size_t block_len, size_t num_blocks){
    void **ptr = mem;
    //gcd(n, n-1) = 1 is always true -> suitable for stride
    size_t stride = num_blocks - 1;
    int z_accesses = 0;

    for(size_t block = 0; block < num_blocks; block++){
        size_t next_block;
#ifdef CACHE_SIZE_METHOD_LSFR
/* Linear shift feedback register, which leaves out all values bigger than num_blocks.
 * It takes block + 1 as last state/register value. +1 is used, as zero in lsfr remains zero.
 */
        uint32_t reg = block + 1;
        do {
            LSFR_NEXT(reg);
        } while (reg > num_blocks);
        next_block = reg - 1;
#else
        next_block = (block + stride) % num_blocks;
#endif
        if(next_block == 0)
            z_accesses++;
        for(size_t b = 0; b < block_len / sizeof(void *); b++){
            *ptr++ = (void*) (mem + next_block * block_len );
        }
    }
    return z_accesses;
}


uint64_t cache_size(void *arr, size_t num_blocks){
    uint64_t start, end;

    //get everything into cache if it fits in
    for(size_t i = 0; i < num_blocks; i++){
        asm volatile("mov (%0), %0" : "=r"(arr): "r"(arr));
    }

    start = cycle_count();
    for(size_t i = 0; i<1000000; i++){
        asm volatile("mov (%0), %0" : "=r"(arr): "r"(arr));
    }
    end = cycle_count();
    return end - start;
}

int main(int argc, char **argv){

    FILE *cache_line_file = fopen("cache_line", "w");
    uint8_t *cache_line_arr = (uint8_t *) malloc(CACHE_LINE_ARR_LEN);
    memset(cache_line_arr, 0, CACHE_LINE_ARR_LEN);

    for (size_t i = 1; i <= MAX_STRIDE; i++){
        for(size_t j = 0; j < ITERATIONS; j++){
            fprintf(cache_line_file, "%lu,%e\n", i, stride(cache_line_arr, CACHE_LINE_ARR_LEN, i) / (double) (CACHE_LINE_ARR_LEN / i));
        }
    }
    fclose(cache_line_file);


    FILE *cache_size_file = fopen("cache_size", "w");
    void *cache_size_arr = (void *) malloc(CACHE_SIZE_ARR_LEN * CACHE_LINE_LEN);
    memset(cache_size_arr, 0, CACHE_SIZE_ARR_LEN * CACHE_LINE_LEN);

    // i+=MAX(...) -> hohe aufloesung/dichte bei kleinen i's, niedrige dichte/aufloesung bei grossen i's
    for (size_t i = 1; i < CACHE_SIZE_ARR_LEN; i += MAX(i / 64, 1)){
        if(init_arr(cache_size_arr, CACHE_LINE_LEN, i) == 1){
            for(size_t j = 0; j < ITERATIONS; j++){
                fprintf(cache_size_file, "%lu,%e\n", i, cache_size(cache_size_arr, i) / 1e6);
            }
        }else{
            printf("with num_blocks %lu, there are multiple accesses to arr[0] in the first %lu rounds\n", i, i);
        }
    }
    fclose(cache_size_file);

    free(cache_size_arr);
    free(cache_line_arr);

    return 0;
}
