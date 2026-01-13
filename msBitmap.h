
/*
    msBitmap.h
    January 12th, 2026
    Brendan Roy

    Unified msBitmap that uses either a bitmap or a hash set, always requiring
    an indexing function.
*/

#ifndef MSBITMAP_H_
#define MSBITMAP_H_

#include <cstdint>
#include <cassert>
#include <sys/mman.h>
#include "robin_hood.h"


const static int INIT_SEEN_SIZE = 8000000;

template <typename T, typename IndexFn>
class msBitmap {
    static_assert(std::is_invocable_r_v<uint64_t, IndexFn, const T&>,
                  "IndexFn must take const T& and return uint64_t");

public:

    /************ msBitmap constructor *********
     Initializes a new msBitmap object given a size and index function

    Parameters: 
        unsigned long long numBits -
        IndexFn indexFn            - an indexing function to be called on the
                                     type T value
    Returns: void
    *********************************/
    msBitmap(uint64_t numBits, IndexFn fn)
        : toIndex(fn)
    {
        #if HAVE_16GB_RAM
            sizeBits = numBits;
            size_t words = (sizeBits + 63) / 64;
            void* ptr = mmap(nullptr, words * 8, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            assert(ptr != MAP_FAILED);
            std::memset(ptr, 0, words * 8);
            bitmap = static_cast<uint64_t*>(ptr);
        #else
            (void) numBits;
            set.reserve(INIT_SEEN_SIZE);
        #endif
    }

    /************ msBitmap destructor *********
     unmaps the bitmap
    *********************************/
    ~msBitmap() 
    {
        #if HAVE_16GB_RAM
            size_t words = (sizeBits + 63) / 64;
            munmap(bitmap, words * 8);
        #endif
    }

    /************ clear *********
     Clears all indices in the bitmap - every index is 0

    Parameters: none
    Returns: void
    Expects: 
    Notes: 
        May take a second or so to execute this function
    *********************************/
    void clear() 
    {
        #if HAVE_16GB_RAM
            std::memset(bitmap, 0, (sizeBits + 63) / 64 * 8);
        #else
            set.clear();
        #endif
    }

    /************ testAndSetBit *********
     Turns on the bit in the given bitmap at the given value's index. Returns 
     true if and only if that bit was already set to 1

    Parameters: 
        const T &value - reference to the value we add to the bitmap
    Returns: 
        A bool - true if the bitmap already contained a 1 at the index
    Expects: 
        (value.*toIndex)() index must be less than 2^37
    Notes:
        Will CRE if (value.*toIndex)() is >= 2^37
    *********************************/
    bool testAndSet(const T& value) 
    {
        uint64_t idx = (value.*toIndex)();
        #if HAVE_16GB_RAM
            assert(idx < sizeBits);
            uint64_t& word = bitmap[idx >> 6];
            uint64_t mask = 1ULL << (idx & 63);
            bool hit = word & mask;
            word |= mask;
            return hit;
        #else
            return !set.insert(idx).second;
        #endif
    }

private:
    IndexFn toIndex;

    #if HAVE_16GB_RAM
        uint64_t* bitmap = nullptr;
        uint64_t sizeBits = 0;
    #else
        robin_hood::unordered_flat_set<uint64_t> set;
    #endif
};

#endif
