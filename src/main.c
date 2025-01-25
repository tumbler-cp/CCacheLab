#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "ram_bench.h"

#define FILE_SIZE 256

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <file_name> <target> <replacement>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char *file_name = argv[1];
    int target = atoi(argv[2]);
    int replacement = atoi(argv[3]);

    int fd = open(file_name, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    off_t file_size = lseek(fd, 0, SEEK_END);
    if (file_size == 0) {
        int fs = FILE_SIZE;
        printf("File not found or is empty. Generating file of size %d MB...\n",
               fs);
        generate_file(file_name, fs, 1);
        close(fd);
        fd = open(file_name, O_RDWR, 0644);
        if (fd < 0) {
            perror("Error reopening file after generation");
            return EXIT_FAILURE;
        }
    } else {
        printf("File already exists and is of size %ld bytes.\n", file_size);
    }

    int prec = count_in_file(file_name, target);
    printf("Precontrol: %d \n", prec);

    clock_t start_time = clock();

    if (!replace_in_file(file_name, target, replacement)) {
        fprintf(stderr, "Target not replaced\n");
    } else {
        printf("Target replaced\n");
    }
    clock_t end_time = clock();
    double duration = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Execution time: %f seconds\n", duration);

    int posc = count_in_file(file_name, target);
    printf("Postcontrol: %d \n", posc);

    close(fd);
    return EXIT_SUCCESS;
}
