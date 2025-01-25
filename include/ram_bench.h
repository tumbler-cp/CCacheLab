#ifndef RAM_BENCH_H
#define RAM_BENCH_H

#include <stddef.h>

int count_in_file(const char* file_name, int target);
int replace_in_file(const char *file_name, int target, int replacement);
void load_memory(const char *file_name, int target, int replacement, size_t iterations);
void generate_file(const char *filename, size_t file_size_mb, int seed);

#endif // RAM_BENCH_H
