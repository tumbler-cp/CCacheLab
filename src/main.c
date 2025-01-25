#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "ram_bench.h"



int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <file_name> <target> <replacement>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *file_name = argv[1];
    int target = atoi(argv[2]);
    int replacement = atoi(argv[3]);

    int fd = open(file_name, O_RDONLY);
    if (fd < 0) {
        printf("File not found. Generating file of size 2048 MB...\n");
        generate_file(file_name, 2048, 1);
    } else {
        close(fd);
    }

    clock_t start_time = clock();

    if (!replace_in_file(file_name, target, replacement)) {
        fprintf(stderr, "Target not replaced\n");
    } else {
        printf("Target replaced\n");
    }

    clock_t end_time = clock();
    double duration = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Execution time: %f seconds\n", duration);

    return EXIT_SUCCESS;
}
