/*
 * Small benchmark of Anonymous Memory Mmaps.
 *
 * Author: Rainer Keller, HS Esslingen
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/wait.h>

#include "bench_utils.h"

#define SLEEP_TIME 1

// #define USE_MEMSET
// #define USE_COPY_BUFFER

int main(int argc, char *argv[])
{
    const int sizes[] = {
        128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768,
        65536, 131072, 262144, 524288, 1048576, 2097152,
        4194304, 8388608, 16777216, 33554432, 67108864};
    const int sizes_num = sizeof(sizes) / sizeof(sizes[0]);
#define MAX_SIZE sizes[sizes_num - 1]
    pid_t pid = getpid();
    pid_t pid_child;
    char *anon;
    int i;

    /**
     * Anlegen einer Anonymen, gesharten Memory-Map, in die gelesen und geschrieben wird.
     *
     * Aufruf:
     *  void * mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
     */
    anon = mmap(NULL, MAX_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);

    if (anon == MAP_FAILED)
        ERROR("mmap anon", errno);

    int ret = pid_child = fork();

    if (-1 == ret)
        ERROR("fork", errno);

    if (0 == ret)
    {
        /* CHILD */
        char *buffer;
        buffer = malloc(MAX_SIZE);
        if (NULL == buffer)
            ERROR("malloc", ENOMEM);
        memset(buffer, 0, MAX_SIZE);
        pid_child = getpid();
        memcpy(anon, buffer, MAX_SIZE);
        pause();
        printf("PID %d (CHILD): COPY DONE\n", pid_child);
        return (EXIT_SUCCESS);
    }

    int *ticks;
    ticks = malloc(MEASUREMENTS * sizeof(int));
    if (NULL == ticks)
        ERROR("malloc", ENOMEM);
    memset(ticks, 0, MEASUREMENTS * sizeof(int));

    int page_size = 0;
    if (1 < argc)
        page_size = strtol(argv[1], NULL, 10);
    else
        page_size = getpagesize();

    char *copy_buffer = malloc(page_size);
    if (NULL == copy_buffer)
        ERROR("malloc", ENOMEM);

    /* PARENT: measure the writing into the buffer */
    for (i = 0; i < sizes_num; i++)
    {
        memset(copy_buffer, (rand() % ('z' - 'a')) + 'a', page_size);
        int current_size = sizes[i];
        int j;
        int min_ticks;
        int max_ticks;
        long long ticks_all;
        struct timeval tv_start;
        struct timeval tv_stop;
        double time_delta_sec;

        sleep(SLEEP_TIME);

        gettimeofday(&tv_start, NULL);
        for (j = 0; j < MEASUREMENTS; j++)
        {
            unsigned long long start;
            unsigned long long stop;
            start = getrdtsc();
#if defined(USE_MEMSET)
            memset(anon, 'a', current_size);
#elif defined(USE_COPY_BUFFER)
            {
                int k = 0;
                while (k < current_size)
                {
                    int bytes_to_copy = current_size > page_size ? page_size : current_size;
                    memcpy(anon + k, copy_buffer, bytes_to_copy);
                    k += bytes_to_copy;
                }
            }
#else
            {
                int k;
                for (k = 0; k < current_size; k++)
                    anon[k] = 'a';
            }
#endif
            stop = getrdtsc();
            ticks[j] = stop - start;
        }
        gettimeofday(&tv_stop, NULL);

        min_ticks = INT_MAX;
        max_ticks = INT_MIN;
        ticks_all = 0;
        for (j = 0; j < MEASUREMENTS; j++)
        {
            if (min_ticks > ticks[j])
                min_ticks = ticks[j];
            if (max_ticks < ticks[j])
                max_ticks = ticks[j];
            ticks_all += ticks[j];
        }
        ticks_all -= min_ticks;
        ticks_all -= max_ticks;

        time_delta_sec = ((tv_stop.tv_sec - tv_start.tv_sec) + ((tv_stop.tv_usec - tv_start.tv_usec) / (1000.0 * 1000.0)));

        printf("PID:%d time: min:%d max:%d Ticks Avg without min/max:%f Ticks (for %d measurements) for %d Bytes (%.2f MB/s)\n",
               pid, min_ticks, max_ticks,
               (double)ticks_all / (MEASUREMENTS - 2.0), MEASUREMENTS, current_size,
               ((double)current_size * MEASUREMENTS) / (1024.0 * 1024.0 * time_delta_sec));
    }

    kill(pid_child, SIGTERM);
    wait(NULL);
    munmap(anon, MAX_SIZE);

    return (EXIT_SUCCESS);
}
