// test_shm_command_queue.cpp
// 跨进程共享内存命令队列测试：open/push/pop + 无锁并发（独立可执行）
#include "shm_command_queue.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <thread>
#include <vector>

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL: %s (line %d)\n", #cond, __LINE__); g_fail++; } } while (0)

static void test_basic() {
    shm_cmd_header_t *q = shm_command_queue_open(16, 1);
    CHECK(q != nullptr);
    if (!q) return;
    shm_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.protocol = 3; cmd.op = 1; cmd.slave_id = 1; cmd.index = 0x100; cmd.value = 42;
    CHECK(shm_command_queue_push(q, &cmd) == 0);
    shm_command_t out;
    CHECK(shm_command_queue_pop(q, &out) == 0);
    CHECK(out.protocol == 3);
    CHECK(out.slave_id == 1);
    CHECK(out.index == 0x100);
    CHECK(out.value == 42);
    CHECK(shm_command_queue_pop(q, &out) == -1);
    shm_command_queue_close(q);
    shm_unlink(SHM_CMD_NAME);
    printf("basic: %s\n", g_fail ? "FAIL" : "OK");
}

static void test_full() {
    shm_cmd_header_t *q = shm_command_queue_open(4, 1);
    CHECK(q != nullptr);
    if (!q) return;
    shm_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    for (int i = 0; i < 4; i++) { cmd.value = i; CHECK(shm_command_queue_push(q, &cmd) == 0); }
    CHECK(shm_command_queue_push(q, &cmd) == -1);
    shm_command_queue_close(q);
    shm_unlink(SHM_CMD_NAME);
    printf("full: %s\n", g_fail ? "FAIL" : "OK");
}

static void test_mpmc() {
    const int PRODUCERS = 4, CONSUMERS = 4, PER = 1000;
    shm_cmd_header_t *q = shm_command_queue_open(256, 1);
    CHECK(q != nullptr);
    if (!q) return;
    std::atomic<bool> done{false};
    std::atomic<long> total{0};
    std::vector<std::thread> producers, consumers;
    for (int p = 0; p < PRODUCERS; p++) {
        producers.emplace_back([q, p]() {
            for (int i = 0; i < PER; i++) {
                shm_command_t cmd; memset(&cmd, 0, sizeof(cmd));
                cmd.protocol = 3; cmd.value = (uint32_t)(p * 100000 + i);
                while (shm_command_queue_push(q, &cmd) != 0) {}
            }
        });
    }
    for (int c = 0; c < CONSUMERS; c++) {
        consumers.emplace_back([q, &done, &total]() {
            while (!done.load()) {
                shm_command_t cmd;
                if (shm_command_queue_pop(q, &cmd) == 0) total.fetch_add(1);
            }
        });
    }
    for (auto& t : producers) t.join();
    done.store(true);
    for (auto& t : consumers) t.join();
    shm_command_t cmd;
    while (shm_command_queue_pop(q, &cmd) == 0) total.fetch_add(1);
    CHECK(total.load() == PRODUCERS * PER);
    shm_command_queue_close(q);
    shm_unlink(SHM_CMD_NAME);
    printf("mpmc: total=%ld %s\n", total.load(), total.load() == 4000 ? "OK" : "FAIL");
}

int main() {
    test_basic();
    test_full();
    test_mpmc();
    printf(g_fail ? "\n=== %d FAILURES ===\n" : "\n=== ALL PASSED ===\n", g_fail);
    return g_fail ? 1 : 0;
}
