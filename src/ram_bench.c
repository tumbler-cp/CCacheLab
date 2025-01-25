#define _GNU_SOURCE
#define USE_CUSTOM_LIB

#include "ram_bench.h"
#ifdef USE_CUSTOM_LIB
#include "ccache.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define CHUNK_SIZE 4096

#ifdef USE_CUSTOM_LIB
#define OPEN(path, flags, mode) lab2_open(path)
#define CLOSE(fd) lab2_close(fd)
#define READ(fd, buf, count) lab2_read(fd, buf, count)
#define WRITE(fd, buf, count) lab2_write(fd, buf, count)
#define LSEEK(fd, offset, whence) lab2_lseek(fd, offset, whence)
#define FSYNC(fd) lab2_fsync(fd)
#else
#define OPEN(path, flags, mode) open(path, flags, mode)
#define CLOSE(fd) close(fd)
#define READ(fd, buf, count) read(fd, buf, count)
#define WRITE(fd, buf, count) write(fd, buf, count)
#define LSEEK(fd, offset, whence) lseek(fd, offset, whence)
#define FSYNC(fd) fsync(fd)
#endif

int replace_in_file(const char *file_name, int target, int replacement) {
    int fd = OPEN(file_name, O_RDWR | O_DIRECT, 0644);
    if (fd < 0) {
        perror("Error opening file");
        return 0;
    }

    char buffer[CHUNK_SIZE];
    int replaced = 0;

    while (1) {
        ssize_t bytes_read = READ(fd, buffer, CHUNK_SIZE);
        if (bytes_read < 0) {
            perror("Error reading file");
            CLOSE(fd);
            return 0;
        }
        if (bytes_read == 0) {
            break; 
        }

        for (size_t i = 0; i < (size_t)bytes_read; i += sizeof(int)) {
            if (bytes_read - i >= (ssize_t)sizeof(int)) {
                int value;
                memcpy(&value, &buffer[i], sizeof(int));
                if (value == target) {
                    memcpy(&buffer[i], &replacement, sizeof(int));
                    replaced = 1;
                }
            }
        }

        LSEEK(fd, -bytes_read, SEEK_CUR);
        ssize_t bytes_written = WRITE(fd, buffer, bytes_read);
        if (bytes_written < 0) {
            perror("Error writing to file");
            CLOSE(fd);
            return 0;
        }
    }

    CLOSE(fd);
    return replaced;
}

void generate_file(const char *filename, size_t file_size_mb, int seed) {
    size_t total_bytes = file_size_mb * 1024 * 1024;
    int buffer[CHUNK_SIZE / sizeof(int)]; 

    srand(seed);  

    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    size_t bytes_written = 0;
    while (bytes_written < total_bytes) {
        size_t to_write = total_bytes - bytes_written < sizeof(buffer) ? total_bytes - bytes_written : sizeof(buffer);

        size_t num_ints_to_write = to_write / sizeof(int);
        for (size_t i = 0; i < num_ints_to_write; ++i) {
        	buffer[i] = rand() % 100;
				}

        ssize_t written = write(fd, buffer, to_write);
        if (written < 0) {
            perror("Error writing to file");
            close(fd);
            exit(EXIT_FAILURE);
        }
        bytes_written += written;
    }

    printf("File %s of size %zu MB successfully generated.\n", filename, file_size_mb);
    close(fd);
}
