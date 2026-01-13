    /* 
       If your computer has 16GiB of RAM available to use, set this to 1. The 
       typical speedup with it enabled is about 1.75x, but set it 0 if space is 
       unavailable. That said, there is a small overhead cost of allocating
       the memory of ~0.5s. 
    */
    #define HAVE_16GB_RAM 0


    /* 
      The _pext_u64 instruction only exists on intel's x86 architecture - 
      it enables about a 15% speedup when available
    */
    #ifdef __BMI2__
        #define HAVE_PEXT 1
    #else
        #define HAVE_PEXT 0
    #endif 