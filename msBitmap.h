
#include <cstdint>
#include <sys/mman.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <assert.h>
#include "configuration.h"


#ifndef MSBITMAP_H_
#define MSBITMAP_H_

template <typename T, typename IndexFn>
class msBitmap {
  public:
    msBitmap(unsigned long long numBits, IndexFn indexFn) : 
            sizeBits(numBits), toIndex(indexFn) 
    {
        size_t words = (numBits + 63) / 64;

        void* ptr = mmap(
            nullptr,               
            words * 8,             
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, 
            -1,                     
            0                       
        );
        assert(ptr != MAP_FAILED); 


        std::memset(ptr, 0, words * 8); 

        bitmap = static_cast<uint64_t*>(ptr); 

    } 
    void clear() {
        std::memset((void *)bitmap, 0, (sizeBits + 63) / 64 * 8); 
    }
    ~msBitmap() 
    {
        size_t words = (sizeBits + 63) / 64;
        munmap(bitmap, words * 8);
    }

    /************ testAndSetBit *********
     Turns on the bit in the given bitmap at the given index. Returns true
    if and only if that bit was already set to 1

    Parameters: 
    uint64_t *bitmap - the bitmap
    uint64_t index   - the index of the bitmap to set and test
    Returns: 
    A bool - true if the bitmap already contained a 1 at the index
    Expects: 
    index must be less than 2^37
    Notes:
    Will CRE if index >= 2^37
    *********************************/
    inline bool testAndSetBit(const T &value)
    {
        uint64_t idx = (value.*toIndex)();
        assert(idx < sizeBits);

        uint64_t &word = bitmap[idx >> 6];
        uint64_t mask = 1ULL << (idx & 63);
        bool hit = word & mask;
        word |= mask;
        return hit;
    };
    inline void setBit(const T &value) 
    {
        uint64_t idx = (value.*toIndex)();
        bitmap[idx >> 6] |= 1ULL << (idx & 63);
    }

    inline bool testBit(const T &value) 
    {
        uint64_t idx = (value.*toIndex)();
        return (bitmap[idx >> 6] >> (idx & 63)) & 1ULL;
    }


  private:
    unsigned long long sizeBits;
    IndexFn toIndex;
    uint64_t *bitmap;

};

#endif