#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "common.h"

// Function to map the file to memory and return the starting address of the mapped memory
char *map_file_to_memory(const char *filename)
{
    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Get file size
    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        perror("Error getting file size");
        exit(EXIT_FAILURE);
    }

    // Map the file to memory
    char *file_map = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_map == MAP_FAILED)
    {
        perror("Error mapping file to memory");
        exit(EXIT_FAILURE);
    }

    close(fd);
    return file_map;
}

// Function to get the file size in bytes
size_t get_file_size(const char *filename)
{
    struct stat sb;
    if (stat(filename, &sb) == -1)
    {
        perror("Error getting file size");
        exit(EXIT_FAILURE);
    }

    return sb.st_size;
}

// Function to unmap the memory-mapped file
void unmap_file_from_memory(char *file_map, size_t file_size)
{
    if (munmap(file_map, file_size) == -1)
    {
        perror("Error unmapping file from memory");
        exit(EXIT_FAILURE);
    }
}

// Block size
#define BLOCK_SIZE 4096

// Necessary thread data
typedef struct
{
    unsigned int thread_id;
    int left_child;
    int right_child;
    unsigned int n;
    unsigned int m;
    unsigned int block_count;
    unsigned int num_threads;
    char *file_map;
    uint32_t computed_hash;
    pthread_t *threads;
    uint32_t left_child_hash;  // Add this line
    uint32_t right_child_hash; // Add this line
    size_t file_size;
    pthread_barrier_t *barrier;
} thread_data_t;

uint32_t jenkins_one_at_a_time_hash(const char *key, size_t length, uint32_t initial_hash)
{
    uint32_t hash, i;
    for (hash = initial_hash, i = 0; i < length; ++i)
    {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

// Mutex and condition variables for synchronizing the access of shared resources between threads
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;
unsigned int finished_threads = 0;

// Thread function to compute the hash value for a portion of the file and combine results from child threads
void *compute_hash(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    uint32_t hash = 0;

    unsigned int start_block = data->thread_id * data->n / data->m;
    unsigned int end_block = (data->thread_id + 1) * data->n / data->m;

    // Compute the hash for the assigned blocks
    for (unsigned int i = start_block; i < end_block; i++)
    {
        size_t current_block_size = BLOCK_SIZE;

        // Adjust the block size if it exceeds the file size
        if ((i + 1) * BLOCK_SIZE > data->file_size)
        {
            current_block_size = data->file_size - i * BLOCK_SIZE;
        }

        // Calculate the hash for the current block
        if (i * BLOCK_SIZE < data->file_size)
        {
            hash = jenkins_one_at_a_time_hash(data->file_map + i * BLOCK_SIZE, current_block_size, hash);
        }
    }

    // If the current thread has a left child wait for it to finish and retrieve its hash
    if (data->left_child != -1)
    {
        pthread_join(data->threads[data->left_child], (void **)&data->left_child_hash);

        if (data->right_child != -1)
        {
            pthread_join(data->threads[data->right_child], (void **)&data->right_child_hash);

            // Combine the current thread's hash with the left and right children's hashes
            char concat_str[64];
            sprintf(concat_str, "%u%u%u", hash, data->left_child_hash, data->right_child_hash);
            hash = jenkins_one_at_a_time_hash(concat_str, strlen(concat_str), 0);
        }
        else
        {
            // Combine the current thread's hash with the left child's hash
            char concat_str[64];
            sprintf(concat_str, "%u%u", hash, data->left_child_hash);
            hash = jenkins_one_at_a_time_hash(concat_str, strlen(concat_str), 0);
        }
    }

    // Store the computed hash in the thread data struct and exit the thread
    data->computed_hash = hash;
    pthread_exit((void *)(uintptr_t)hash);
}

// Helper function to create threads in descending order based on their ID, starting with the largest ID
void create_threads_in_descending_order(thread_data_t *thread_data_array, pthread_t *threads, int thread_id)
{
    if (thread_id < 0)
        return;

    create_threads_in_descending_order(thread_data_array, threads, thread_data_array[thread_id].left_child);
    create_threads_in_descending_order(thread_data_array, threads, thread_data_array[thread_id].right_child);

    pthread_create(&threads[thread_id], NULL, compute_hash, (void *)&thread_data_array[thread_id]);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <filename> <num_threads>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    unsigned int num_threads = atoi(argv[2]);
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, num_threads);

    if (num_threads < 1)
    {
        fprintf(stderr, "Number of threads must be at least 1\n");
        exit(EXIT_FAILURE);
    }

    // Read file and map it to memory
    char *file_map = map_file_to_memory(argv[1]);

    // Get file size
    size_t file_size = get_file_size(argv[1]);

    // Allocate memory for threads and thread data
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    thread_data_t *thread_data_array = malloc(num_threads * sizeof(thread_data_t));

    // Assign values to thread_data
    unsigned int block_count = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Calculate blocks per thread
    unsigned int blocks_per_thread = (block_count + num_threads - 1) / num_threads;

    // Print number of threads and blocks per thread
    printf("num Threads = %u\n", num_threads);
    printf("Blocks per Thread = %u\n", blocks_per_thread);

    for (int i = 0; i < num_threads; i++)
    {
        thread_data_array[i].thread_id = i;
        thread_data_array[i].left_child = 2 * i + 1 < num_threads ? 2 * i + 1 : -1;
        thread_data_array[i].right_child = 2 * i + 2 < num_threads ? 2 * i + 2 : -1;
        thread_data_array[i].n = block_count; // Assign block_count to n
        thread_data_array[i].num_threads = num_threads;
        thread_data_array[i].m = num_threads; // Assign num_threads to m
        thread_data_array[i].file_map = file_map;
        thread_data_array[i].threads = threads;
        thread_data_array[i].file_size = file_size;
        thread_data_array[i].barrier = &barrier;
    }
    
    // Record the start time for performance measurement
    double start = GetTime();

    // for (int i = num_threads - 1; i >= 0; i--)
    // {
    //     if ((i * block_count) / num_threads < file_size)
    //     {
    //         pthread_create(&threads[i], NULL, compute_hash, (void *)&thread_data_array[i]);
    //     }
    // }

    create_threads_in_descending_order(thread_data_array, threads, num_threads - 1);

    // Wait for the root thread to finish
    uint32_t root_hash;
    pthread_join(threads[0], (void **)&root_hash);

    double end = GetTime();

    // Print result
    printf("hash value = %u\n", root_hash);
    printf("time taken = %f\n", (end - start));
    
    // Clean up
    free(thread_data_array);
    free(threads);
    unmap_file_from_memory(file_map, file_size);
    pthread_barrier_destroy(&barrier);

    return 0;
}
