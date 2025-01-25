#define _GNU_SOURCE

#define BLOCK_SIZE 4096
#define CACHE_COUNT 16

#include "ccache.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct cache_page {
    off_t offset;
    void *data;
    bool dirty;
    struct cache_page *prev, *next;
} cache_page_t;

typedef struct cache {
    cache_page_t *head;
    cache_page_t *tail;
    size_t capacity;
    size_t size;
    size_t block_size;
} cache_t;

typedef struct file_descriptor {
    int fd;
    off_t offset;
    cache_t *cache;
} file_descriptor_t;

#define MAX_OPEN_FILES 256
static file_descriptor_t fd_table[MAX_OPEN_FILES];

static cache_t *cache_init(size_t capacity, size_t block_size) {
    cache_t *cache = (cache_t *)malloc(sizeof(cache_t));
    if (!cache) return NULL;

    cache->head = NULL;
    cache->tail = NULL;
    cache->capacity = capacity;
    cache->size = 0;
    cache->block_size = block_size;
    return cache;
}

static void cache_destroy(cache_t *cache) {
    cache_page_t *current = cache->head;
    while (current) {
        cache_page_t *next = current->next;
        free(current->data);
        free(current);
        current = next;
    }
    free(cache);
}

static void cache_promote(cache_t *cache, cache_page_t *page) {
    if (cache->head == page) return;  // Уже на вершине

    if (page->prev) page->prev->next = page->next;
    if (page->next) page->next->prev = page->prev;
    if (cache->tail == page) cache->tail = page->prev;  // Если это хвост

    page->prev = NULL;
    page->next = cache->head;
    if (cache->head) cache->head->prev = page;
    cache->head = page;

    if (!cache->tail) cache->tail = page;  // Если список был пуст
}

static void cache_evict(cache_t *cache) {
    if (!cache->head) return;

    cache_page_t *evicted = cache->head;

    if (evicted->dirty) {
        pwrite(fd_table[0].fd, evicted->data, cache->block_size,
               evicted->offset);
    }

    if (evicted->next) {
        evicted->next->prev = NULL;
    }
    cache->head = evicted->next;

    if (!cache->head) {
        cache->tail = NULL;
    }

    free(evicted->data);
    free(evicted);
    cache->size--;
}

int lab2_open(const char *path) {
    int fd = open(path, O_RDWR | O_DIRECT);
    if (fd < 0) return -1;

    for (int i = 0; i < MAX_OPEN_FILES; ++i) {
        if (fd_table[i].fd == 0) {
            fd_table[i].fd = fd;
            fd_table[i].offset = 0;
            fd_table[i].cache = cache_init(CACHE_COUNT, BLOCK_SIZE);
            if (!fd_table[i].cache) {
                close(fd);
                return -1;
            }
            return i;
        }
    }

    close(fd);
    return -1;
}

int lab2_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || fd_table[fd].fd == 0) return -1;

    file_descriptor_t *file = &fd_table[fd];
    cache_t *cache = file->cache;

    cache_page_t *page = cache->head;
    while (page) {
        if (page->dirty) {
            pwrite(file->fd, page->data, cache->block_size, page->offset);
        }
        page = page->next;
    }

    close(file->fd);
    cache_destroy(cache);
    fd_table[fd].fd = 0;
    return 0;
}

ssize_t lab2_read(int fd, void *buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || fd_table[fd].fd == 0) return -1;

    file_descriptor_t *file = &fd_table[fd];
    cache_t *cache = file->cache;
    size_t block_size = cache->block_size;

    off_t block_offset = file->offset / block_size * block_size;
    size_t block_index = file->offset % block_size;

    cache_page_t *page = cache->head;
    while (page) {
        if (page->offset == block_offset) {
            cache_promote(cache, page);
            size_t to_copy = (count > block_size - block_index)
                                 ? block_size - block_index
                                 : count;
            memcpy(buf, page->data + block_index, to_copy);
            file->offset += to_copy;
            return to_copy;
        }
        page = page->next;
    }

    if (cache->size >= cache->capacity) cache_evict(cache);
    page = (cache_page_t *)malloc(sizeof(cache_page_t));
    if (!page) return -1;

    if (posix_memalign(&page->data, block_size, block_size) != 0) {
        free(page);
        return -1;
    }

    ssize_t read_bytes = pread(file->fd, page->data, block_size, block_offset);
    if (read_bytes < 0) {
        free(page->data);
        free(page);
        return -1;
    }

    if (read_bytes == 0) {  // Конец файла
        free(page->data);
        free(page);
        return 0;
    }

    page->offset = block_offset;
    page->dirty = false;
    page->prev = NULL;
    page->next = cache->head;
    if (cache->head) cache->head->prev = page;
    cache->head = page;
    if (!cache->tail) cache->tail = page;

    cache->size++;

    size_t to_copy =
        (count > read_bytes - block_index) ? read_bytes - block_index : count;
    memcpy(buf, page->data + block_index, to_copy);
    file->offset += to_copy;
    return to_copy;
}

ssize_t lab2_write(int fd, const void *buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || fd_table[fd].fd == 0) return -1;

    file_descriptor_t *file = &fd_table[fd];
    cache_t *cache = file->cache;
    size_t block_size = cache->block_size;

    off_t block_offset = file->offset / block_size * block_size;
    size_t block_index = file->offset % block_size;

    size_t bytes_written = 0;

    while (count > 0) {
        cache_page_t *page = cache->head;
        while (page) {
            if (page->offset == block_offset) {
                break;
            }
            page = page->next;
        }

        if (!page) {
            if (cache->size >= cache->capacity) {
                cache_evict(cache);
            }

            page = (cache_page_t *)malloc(sizeof(cache_page_t));
            if (!page) return -1;

            if (posix_memalign(&page->data, block_size, block_size) != 0) {
                free(page);
                return -1;
            }

            memset(page->data, 0, block_size);
            ssize_t read_bytes =
                pread(file->fd, page->data, block_size, block_offset);
            if (read_bytes < 0) {
                free(page->data);
                free(page);
                return read_bytes;
            }

            page->offset = block_offset;
            page->dirty = false;
            page->prev = NULL;
            page->next = cache->head;
            if (cache->head) cache->head->prev = page;
            cache->head = page;
            if (!cache->tail) cache->tail = page;

            cache->size++;
        }

        cache_promote(cache, page);

        size_t to_copy = (count > block_size - block_index)
                             ? block_size - block_index
                             : count;
        memcpy((char *)page->data + block_index, buf, to_copy);
        page->dirty =
            true;  // Устанавливаем dirty только после изменения данных

        buf = (const char *)buf + to_copy;
        count -= to_copy;
        bytes_written += to_copy;

        file->offset += to_copy;
        block_offset += block_size;
        block_index = 0;
    }

    return bytes_written;
}

off_t lab2_lseek(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || fd_table[fd].fd == 0) return -1;

    file_descriptor_t *file = &fd_table[fd];
    switch (whence) {
        case SEEK_SET:
            file->offset = offset;
            break;
        case SEEK_CUR:
            file->offset += offset;
            break;
        case SEEK_END: {
            struct stat st;
            if (fstat(file->fd, &st) < 0) return -1;
            file->offset = st.st_size + offset;
            break;
        }
        default:
            return -1;
    }
    return file->offset;
}

int lab2_fsync(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || fd_table[fd].fd == 0) return -1;

    return fsync(fd_table[fd].fd);
}
