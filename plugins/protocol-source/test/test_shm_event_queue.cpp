// test_shm_event_queue.cpp
// 跨进程共享内存事件队列测试：open/push/pop + 无锁并发（独立可执行，无框架依赖）
#include "shm_event_queue.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <thread>
#include <vector>

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL: %s (line %d)\n", #cond, __LINE__); g_fail++; } } while (0)

static void test_basic() {
    shm_eq_header_t *q = shm_event_queue_open(16, 1);
    CHECK(q != nullptr);
    if (!q) return;

    shm_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.protocol = 3; ev.op = 1; ev.slave_id = 1; ev.index = 0x100; ev.value = 42;
    CHECK(shm_event_queue_push(q, &ev) == 0);

    shm_event_t out;
    CHECK(shm_event_queue_pop(q, &out) == 0);
    CHECK(out.protocol == 3);
    CHECK(out.op == 1);
    CHECK(out.slave_id == 1);
    CHECK(out.index == 0x100);
    CHECK(out.value == 42);

    CHECK(shm_event_queue_pop(q, &out) == -1);  // 空

    shm_event_queue_close(q);
    shm_unlink(SHM_EQ_NAME);
    printf("basic: %s\n", g_fail ? "FAIL" : "OK");
}

static void test_full() {
    shm_eq_header_t *q = shm_event_queue_open(4, 1);
    CHECK(q != nullptr);
    if (!q) return;
    shm_event_t ev;
    memset(&ev, 0, sizeof(ev));
    for (int i = 0; i < 4; i++) { ev.value = i; CHECK(shm_event_queue_push(q, &ev) == 0); }
    CHECK(shm_event_queue_push(q, &ev) == -1);  // 满
    shm_event_queue_close(q);
    shm_unlink(SHM_EQ_NAME);
    printf("full: %s\n", g_fail ? "FAIL" : "OK");
}

static void test_mpmc() {
    const int PRODUCERS = 4, CONSUMERS = 4, PER = 1000;
    shm_eq_header_t *q = shm_event_queue_open(256, 1);
    CHECK(q != nullptr);
    if (!q) return;
    std::atomic<bool> done{false};
    std::atomic<long> total{0};
    std::vector<std::thread> producers, consumers;
    for (int p = 0; p < PRODUCERS; p++) {
        producers.emplace_back([q, p]() {
            for (int i = 0; i < 1000; i++) {
                shm_event_t ev; memset(&ev, 0, sizeof(ev));
                ev.protocol = 3; ev.value = (uint32_t)(p * 100000 + i);
                while (shm_event_queue_push(q, &ev) != 0) {}  // spin
            }
        });
    }
    for (int c = 0; c < CONSUMERS; c++) {
        consumers.emplace_back([q, &done, &total]() {
            while (!done.load()) {
                shm_event_t ev;
                if (shm_event_queue_pop(q, &ev) == 0) total.fetch_add(1);
            }
        });
    }
    for (auto& t : producers) t.join();
    done.store(true);
    for (auto& t : consumers) t.join();
    shm_event_t ev;
    while (shm_event_queue_pop(q, &ev) == 0) total.fetch_add(1);
    CHECK(total.load() == PRODUCERS * PER);
    shm_event_queue_close(q);
    shm_unlink(SHM_EQ_NAME);
    printf("mpmc: total=%ld %s\n", total.load(), total.load() == 4000 ? "OK" : "FAIL");
}

int main() {
    test_basic();
    test_full();
    test_mpmc();
    printf(g_fail ? "\n=== %d FAILURES ===\n" : "\n=== ALL PASSED ===\n", g_fail);
    return g_fail ? 1 : 0;
}
