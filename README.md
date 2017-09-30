# Cache Simulator
## Computer architecture 
Spring 2017 
Creating a Cache Simulator
https://github.com/CS1541-2174/cs1541_2174/wiki/Project-1

Simulating two caches:

- a **read-only instruction cache**, and
- a **read-write data cache**.
- a **multi-level data cache**.

The instruction cache will support the following features:

- Any number of blocks
- Any number of words per block
- Direct-mapped, set-associative, or fully-associative
- For non-direct-mapped, either **random** or **LRU replacement**

The data cache will support all of the above features, but also:

- Write-through, write-no-allocate (aka write-around)
- Write-through, write-allocate
- Write-back, write-allocate

Assume we are simulating a **32-bit CPU**. This means:

- Addresses are 32 bits, and
- 2 bits of the address are used for byte select.
- We're also assuming that **all transfers to and from memory are one word**.
You don't need to store/simulate the data in memory or the cache. You only need to simulate the cache metadata (valid, dirty, tag bits) and logic.
