/**
 * @file shm_event_queue.c
 * @brief 跨进程共享内存事件队列实现（Vyukov 无锁 MPMC）
 */
#include "shm_event_queue.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* 队列完整布局（共享内存） */
typedef struct {
    shm_eq_header_t header;
    /* 槽位紧随 header */
} shm_eq_layout_t;

static inline shm_eq_slot_t *
slots_of(shm_eq_header_t *q)
{
    return (shm_eq_slot_t *)((char *)q + sizeof(shm_eq_header_t));
}

/* 计算容量对应的槽位偏移（2 的幂） */
static size_t
slot_offset(uint32_t cap, uint64_t idx)
{
    return (size_t)(idx & (cap - 1));
}

shm_eq_header_t *
shm_event_queue_open(uint32_t cap, int create)
{
    if (cap == 0) {
        cap = SHM_EQ_DEFAULT_CAP;
    }
    /* 确保 2 的幂 */
    uint32_t c = 1;
    while (c < cap) {
        c <<= 1;
    }
    cap = c;

    size_t total = sizeof(shm_eq_header_t) + (size_t)cap * sizeof(shm_eq_slot_t);

    int fd;
    if (create) {
        fd = shm_open(SHM_EQ_NAME, O_CREAT | O_RDWR, 0666);
    } else {
        fd = shm_open(SHM_EQ_NAME, O_RDWR, 0666);
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
    shm_eq_header_t *q = (shm_eq_header_t *)addr;
    if (create) {
        /* 初始化 */
        memset(addr, 0, total);
        q->magic = SHM_EQ_MAGIC;
        q->capacity = cap;
        q->prod_idx = 0;
        q->cons_idx = 0;
        shm_eq_slot_t *slots = slots_of(q);
        for (uint32_t i = 0; i < cap; i++) {
            slots[i].seq = (uint64_t)i; /* 初始 seq = i */
        }
    }
    return q;
}

void
shm_event_queue_close(shm_eq_header_t *q)
{
    if (q == NULL) {
        return;
    }
    size_t total = sizeof(shm_eq_header_t) + (size_t)q->capacity * sizeof(shm_eq_slot_t);
    munmap(q, total);
}

int
shm_event_queue_push(shm_eq_header_t *q, const shm_event_t *ev)
{
    if (q == NULL || q->magic != SHM_EQ_MAGIC || ev == NULL) {
        return -1;
    }
    shm_eq_slot_t *slots = slots_of(q);
    uint64_t prod;
    for (;;) {
        prod = __atomic_load_n(&q->prod_idx, __ATOMIC_RELAXED);
        size_t pos = slot_offset(q->capacity, prod);
        shm_eq_slot_t *slot = &slots[pos];
        uint64_t seq = __atomic_load_n(&slot->seq, __ATOMIC_ACQUIRE);
        uint64_t seq_expected = prod;
        if (seq != seq_expected) {
            return -1; /* 满 */
        }
        /* CAS 预留槽位（多生产者互斥） */
        if (__atomic_compare_exchange_n(&q->prod_idx, &prod, prod + 1, false,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
            /* 已预留，写数据 */
            slot->event = *ev;
            __atomic_store_n(&slot->seq, seq_expected + 1, __ATOMIC_RELEASE);
            return 0;
        }
        /* CAS 失败：重试 */
    }
}

int
shm_event_queue_pop(shm_eq_header_t *q, shm_event_t *ev)
{
    if (q == NULL || q->magic != SHM_EQ_MAGIC || ev == NULL) {
        return -1;
    }
    shm_eq_slot_t *slots = slots_of(q);
    uint64_t cons;
    for (;;) {
        cons = __atomic_load_n(&q->cons_idx, __ATOMIC_RELAXED);
        size_t pos = slot_offset(q->capacity, cons);
        shm_eq_slot_t *slot = &slots[pos];
        uint64_t seq = __atomic_load_n(&slot->seq, __ATOMIC_ACQUIRE);
        uint64_t seq_expected = cons + 1;
        if (seq != seq_expected) {
            return -1; /* 空 */
        }
        /* CAS 预留槽位（多消费者互斥） */
        if (__atomic_compare_exchange_n(&q->cons_idx, &cons, cons + 1, false,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
            *ev = slot->event;
            __atomic_store_n(&slot->seq, seq_expected + q->capacity - 1, __ATOMIC_RELEASE);
            return 0;
        }
        /* CAS 失败：重试 */
    }
}
