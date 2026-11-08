/**
 * @file shm_command_queue.cpp
 * @brief 跨进程共享内存命令队列实现（Vyukov MPMC + CAS）
 */
#include "shm_command_queue.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

static inline shm_cmd_slot_t *
slots_of(shm_cmd_header_t *q)
{
    return (shm_cmd_slot_t *)((char *)q + sizeof(shm_cmd_header_t));
}

static size_t
slot_offset(uint32_t cap, uint64_t idx)
{
    return (size_t)(idx & (cap - 1));
}

shm_cmd_header_t *
shm_command_queue_open(uint32_t cap, int create)
{
    if (cap == 0) {
        cap = SHM_CMD_DEFAULT_CAP;
    }
    uint32_t c = 1;
    while (c < cap) {
        c <<= 1;
    }
    cap = c;

    size_t total = sizeof(shm_cmd_header_t) + (size_t)cap * sizeof(shm_cmd_slot_t);

    int fd;
    if (create) {
        fd = shm_open(SHM_CMD_NAME, O_CREAT | O_RDWR, 0666);
    } else {
        fd = shm_open(SHM_CMD_NAME, O_RDWR, 0666);
    }
    if (fd < 0) {
        return NULL;
    }
    if (create) {
        if (ftruncate(fd, (off_t)total) != 0) {
            close(fd);
            return NULL;
        }
    }
    void *addr = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (addr == MAP_FAILED) {
        return NULL;
    }
    shm_cmd_header_t *q = (shm_cmd_header_t *)addr;
    if (create) {
        memset(addr, 0, total);
        q->magic = SHM_CMD_MAGIC;
        q->capacity = cap;
        q->prod_idx = 0;
        q->cons_idx = 0;
        shm_cmd_slot_t *slots = slots_of(q);
        for (uint32_t i = 0; i < cap; i++) {
            slots[i].seq = (uint64_t)i;
        }
    }
    return q;
}

void
shm_command_queue_close(shm_cmd_header_t *q)
{
    if (q == NULL) {
        return;
    }
    size_t total = sizeof(shm_cmd_header_t) + (size_t)q->capacity * sizeof(shm_cmd_slot_t);
    munmap(q, total);
}

int
shm_command_queue_push(shm_cmd_header_t *q, const shm_command_t *cmd)
{
    if (q == NULL || q->magic != SHM_CMD_MAGIC || cmd == NULL) {
        return -1;
    }
    shm_cmd_slot_t *slots = slots_of(q);
    uint64_t prod;
    for (;;) {
        prod = __atomic_load_n(&q->prod_idx, __ATOMIC_RELAXED);
        size_t pos = slot_offset(q->capacity, prod);
        shm_cmd_slot_t *slot = &slots[pos];
        uint64_t seq = __atomic_load_n(&slot->seq, __ATOMIC_ACQUIRE);
        if (seq != prod) {
            return -1; /* 满 */
        }
        if (__atomic_compare_exchange_n(&q->prod_idx, &prod, prod + 1, false,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
            slot->cmd = *cmd;
            __atomic_store_n(&slot->seq, prod + 1, __ATOMIC_RELEASE);
            return 0;
        }
    }
}

int
shm_command_queue_pop(shm_cmd_header_t *q, shm_command_t *cmd)
{
    if (q == NULL || q->magic != SHM_CMD_MAGIC || cmd == NULL) {
        return -1;
    }
    shm_cmd_slot_t *slots = slots_of(q);
    uint64_t cons;
    for (;;) {
        cons = __atomic_load_n(&q->cons_idx, __ATOMIC_RELAXED);
        size_t pos = slot_offset(q->capacity, cons);
        shm_cmd_slot_t *slot = &slots[pos];
        uint64_t seq = __atomic_load_n(&slot->seq, __ATOMIC_ACQUIRE);
        if (seq != cons + 1) {
            return -1; /* 空 */
        }
        if (__atomic_compare_exchange_n(&q->cons_idx, &cons, cons + 1, false,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
            *cmd = slot->cmd;
            __atomic_store_n(&slot->seq, cons + q->capacity, __ATOMIC_RELEASE);
            return 0;
        }
    }
}
