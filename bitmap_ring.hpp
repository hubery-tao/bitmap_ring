/*
bitmap_ring written by Hubery Tao on Oct 27th, 2022
should function on x64 machines supporting
BSF, BTC and CMPXCHG instructions
gcc version >= 6 (defining __GCC_ASM_FLAG_OUTPUTS__) is needed
*/

#pragma once

#include <stdint.h>

// literally the bsf (bit scan forward) instruction, 
// find the lowest set bit of x, undefined if x == 0
inline __attribute__((__always_inline__))
uint64_t bsf(uint64_t x) {
    uint64_t res;
    __asm__ __volatile__ (
        "bsfq %[x], %[res]"
        : [res]"=r"(res)
        : [x]"r"(x)
    );
    return res;
}

// literally the btc (bit test and compliment) instruction, not atomic
// compliment the n-th bit of x
inline __attribute__((__always_inline__))
void btc(uint64_t& x, uint64_t n) {
    __asm__ __volatile__ (
        "btcq %1, %0"
        : "+r"(x)
        : "r"(n)
    );
}

// literally the btc instruction, atomic
// compliment the n-th bit of x
inline __attribute__((__always_inline__))
void lock_btc(volatile uint64_t& x, uint64_t n) {
    __asm__ __volatile__ (
        "lock;"
        "btcq %1, %0"
        : "+m"(x)
        : "r"(n)
    );
}

// literally the cmpxchg instruction, atomic
inline __attribute__((__always_inline__))
bool cmpxchg(volatile uint64_t& dst, uint64_t& rax, uint64_t src) {
    bool success;
    __asm__ __volatile__ (
        "lock;"
        "cmpxchgq %[src], %[dst]"
        : [dst]"+m"(dst), "+a"(rax), "=@cce"(success)
        : [src]"r"(src)
        : "memory"
    );
    return success;
}

template <typename T>
class bitmap_ring {
    T data[64] __attribute__((__aligned__(64)));
    volatile uint64_t push_free_map;
    volatile uint64_t pop_free_map;

public:

    bitmap_ring(): push_free_map(~0), pop_free_map(0) {}

    bool try_push(const T& elem) {

        uint64_t fetched_map = push_free_map;
        
        for(;;) {
            if (fetched_map == 0) {
                return false;
            }
            else {
                uint64_t lsb = bsf(fetched_map);
                uint64_t new_map = fetched_map;
                btc(new_map, lsb);
                if (cmpxchg(push_free_map, fetched_map, new_map)) {
                    data[lsb] = elem;
                    lock_btc(pop_free_map, lsb);
                    return true;
                }
            }
        }
    }

    bool try_pop(T& elem) {
        
        uint64_t fetched_map = pop_free_map;
        
        for(;;) {
            if (fetched_map == 0) {
                return false;
            }
            else {
                uint64_t lsb = bsf(fetched_map);
                uint64_t new_map = fetched_map;
                btc(new_map, lsb);
                if (cmpxchg(pop_free_map, fetched_map, new_map)) {
                    elem = data[lsb];
                    lock_btc(push_free_map, lsb);
                    return true;
                }
            }
        }
    }

    bool empty() const {
        return (pop_free_map == 0);
    }

    bool full() const {
        return (push_free_map == 0);
    }
};