/**
 * @file shm_command_queue.h
 * @brief 跨进程共享内存命令队列（C2：edgelink workflow → webhmi 协议栈）
 *
 * 布局与 shm_event_queue 一致（Vyukov MPMC + CAS 预留槽位），
 * 但存命令条目。edgelink（workflow 进程）push，webhmi（协议栈进程）pop。
 */
#ifndef WEBHMI_SHM_COMMAND_QUEUE_H
#define WEBHMI_SHM_COMMAND_QUEUE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHM_CMD_MAGIC 0x574D4355u /* "WMCU" */
#define SHM_CMD_NAME "/webhmi_commands"
#define SHM_CMD_DEFAULT_CAP 128

/* 命令条目（与 webhmi command_queue_entry_t 对齐） */
typedef struct {
    uint32_t protocol;    /* 协议枚举 */
    uint32_t op;          /* 操作枚举 */
    uint32_t slave_id;    /* 从站地址 */
    uint32_t index;       /* 寄存器/OD 索引 */
    uint32_t sub;         /* 子索引 */
    uint32_t value;       /* 值 */
    uint32_t timeout_ms;  /* 超时 ms */
} shm_command_t;

typedef struct {
    uint64_t seq;
    shm_command_t cmd;
} shm_cmd_slot_t;

typedef struct {
    uint32_t magic;
    uint32_t capacity;
    uint64_t prod_idx;
    uint64_t cons_idx;
} shm_cmd_header_t;

shm_cmd_header_t *shm_command_queue_open(uint32_t cap, int create);
void shm_command_queue_close(shm_cmd_header_t *q);
int shm_command_queue_push(shm_cmd_header_t *q, const shm_command_t *cmd);
int shm_command_queue_pop(shm_cmd_header_t *q, shm_command_t *cmd);

#ifdef __cplusplus
}
#endif

#endif /* WEBHMI_SHM_COMMAND_QUEUE_H */
