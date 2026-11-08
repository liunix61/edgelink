/**
 * @file shm_event_queue.h
 * @brief 跨进程共享内存事件队列（MPMC 无锁，B4 的 /dev/shm 版）
 *
 * webhmi（协议栈进程）与 edgelink（workflow 进程）通过共享内存通信。
 * 布局（/dev/shm/webhmi_events）：
 *   [header: magic, capacity, prod_idx, cons_idx, seq]
 *   [slots[N]: { seq, event }]
 * Vyukov 无锁 MPMC（序列号法），跨进程 __atomic（seq_cst）。
 */
#ifndef WEBHMI_SHM_EVENT_QUEUE_H
#define WEBHMI_SHM_EVENT_QUEUE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHM_EQ_MAGIC 0x574D5155u /* "WMQU" */
#define SHM_EQ_NAME "/webhmi_events"
#define SHM_EQ_DEFAULT_CAP 256

/* 事件结构（与 event_queue.h 对齐） */
typedef struct {
    uint32_t protocol;    /* 协议枚举 */
    uint32_t op;          /* 操作枚举 */
    uint32_t slave_id;    /* 从站地址 */
    uint32_t index;       /* 寄存器/OD 索引 */
    uint32_t sub;         /* 子索引 */
    uint32_t value;       /* 值 */
    uint64_t ts_ns;       /* 时间戳 ns */
    int32_t  error;       /* 错误码，0 成功 */
} shm_event_t;

/* 槽位 */
typedef struct {
    uint64_t seq;         /* 序列号（Vyukov） */
    shm_event_t event;
} shm_eq_slot_t;

/* 队列头（共享内存首部） */
typedef struct {
    uint32_t magic;
    uint32_t capacity;
    uint64_t prod_idx;
    uint64_t cons_idx;
} shm_eq_header_t;

/**
 * @brief 创建/打开共享内存事件队列（webhmi 侧）
 * @param cap 容量（2 的幂，默认 256）
 * @param create 1=创建（ftruncate+mmap），0=仅打开
 * @return 队列指针（mmap 基址），失败 NULL
 */
shm_eq_header_t *shm_event_queue_open(uint32_t cap, int create);

/**
 * @brief 关闭（munmap）
 */
void shm_event_queue_close(shm_eq_header_t *q);

/**
 * @brief 推入事件（生产者，webhmi 协议栈线程）
 * @return 0 成功，-1 满
 */
int shm_event_queue_push(shm_eq_header_t *q, const shm_event_t *ev);

/**
 * @brief 弹出事件（消费者，edgelink SourceNode 协程）
 * @return 0 成功，-1 空
 */
int shm_event_queue_pop(shm_eq_header_t *q, shm_event_t *ev);

#ifdef __cplusplus
}
#endif

#endif /* WEBHMI_SHM_EVENT_QUEUE_H */
