// SPDX-License-Identifier: MIT
// Deterministically model a lower-priority holder that only runs after a timed sleep.
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
static thread_local uint32_t *release_on_sleep;
static thread_local unsigned fast_yields, timed_sleeps;
static void simulated_sleep(int64_t ns) {
    if(ns==0) ++fast_yields;
    else if(ns==100000) ++timed_sleeps;
    else std::abort();
    if(release_on_sleep) {
        if(ns>0) __atomic_store_n(release_on_sleep,0u,__ATOMIC_RELEASE);
    } else std::this_thread::yield();
}
#define NX2_SPIN_SLEEP(ns) simulated_sleep(ns)
#include "nx2link/upstream_spinlock.h"
static void check(bool ok) { if(!ok) { std::cerr<<"priority-lock check failed\n"; std::exit(1); } }
int main() {
    uint32_t lock=0;
    nx2_spin_lock(&lock);check(lock==1 && fast_yields==0 && timed_sleeps==0);
    nx2_spin_unlock(&lock);check(lock==0);
    lock=1;release_on_sleep=&lock;
    nx2_spin_lock(&lock);
    check(lock==1 && fast_yields==16 && timed_sleeps==1);
    nx2_spin_unlock(&lock);release_on_sleep=nullptr;
    uint32_t spins=0;fast_yields=timed_sleeps=0;
    for(unsigned i=0;i<100;i++) nx2_spin_backoff(&spins);
    check(spins==16 && fast_yields==16 && timed_sleeps==84);
    uint64_t count=0;std::vector<std::thread> workers;
    for(int i=0;i<4;i++) workers.emplace_back([&] {
        for(int k=0;k<25000;k++) { nx2_spin_lock(&lock);++count;nx2_spin_unlock(&lock); }
    });
    for(auto& t:workers)t.join();
    check(count==100000 && lock==0);
    std::cout<<"PASS priority locks: uncontended, bounded priority yield, saturation, 100000 concurrent increments\n";
}
