#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cachesim.h"

/*
Usage:
	./cachesim -I 4096:1:2:R -D 1:4096:2:4:R:B:A -D 2:16384:4:8:L:T:N trace.txt

The -I flag sets instruction cache parameters. The parameter after looks like:
	4096:1:2:R
This means the I-cache will have 4096 blocks, 1 word per block, with 2-way
associativity.

The R means Random block replacement; L for that item would mean LRU. This
replacement scheme is ignored if the associativity == 1.

The -D flag sets data cache parameters. The parameter after looks like:
	1:4096:2:4:R:B:A

The first item is the level and must be 1, 2, or 3.

The second through fourth items are the number of blocks, words per block, and
associativity like for the I-cache. The fifth item is the replacement scheme,
just like for the I-cache.

The sixth item is the write scheme and can be:
	B for write-Back
	T for write-Through

The seventh item is the allocation scheme and can be:
	A for write-Allocate
	N for write-No-allocate

The last argument is the filename of the memory trace to read. This is a text
file where every line is of the form:
	0x00000000 R
A hexadecimal address, followed by a space and then R, W, or I for data read,
data write, or instruction fetch, respectively.
*/

/* These global variables will hold the info needed to set up your caches in
your setup_caches function. Look in cachesim.h for the description of the
CacheInfo struct for docs on what's inside it. Have a look at dump_cache_info
for an example of how to check the members. */
static CacheInfo icache_info;
static CacheInfo dcache_info[3];
static CacheSetup icache_setup, dcache_setup[3];
static CacheStats icache_stats, dcache_stats[3];

CacheBlock** icache;
CacheBlock** dcache;
CacheBlock** dcache2;
CacheBlock** dcache3;

unsigned int tag_I, tag_D[3];
int word_index_I, row_index_I, col_index_I;
int word_index_D[3], row_index_D[3], col_index_D[3];


/* power_of_two - returns what power n is with a base 2 */
int power_of_two(int n)
{
  int count = 0;
  if (n == 1)
    return 0;
  while (n != 1)
  {
    if (n%2 != 0)
      return -1;
    n = n/2;
    count++;
  }
  return count;
}
/* Updates a given block to MRU and increments age of all other blocks in icache */
void updateAge(int col)
{
  /* Update LRU ages */
  if(icache_info.replacement == Replacement_LRU)
  {
    icache[row_index_I][col].LRU_age = 1;
    int j;
    for(j = 0; j < icache_setup.num_cols; j++)
    {
      if(icache[row_index_I][j].LRU_age == 0)
      {
        /* This block and the rest have not been used yet */
        break;
      }
      else if(j != col)
      {
        icache[row_index_I][j].LRU_age++;
      }
    }
  }
}
/* Updates a given block to MRU and increments age of all other blocks in dcache */
void updateAgeD(int col, int level, CacheBlock** cache)
{
  /* Update LRU ages */
  if(dcache_info[level].replacement == Replacement_LRU)
  {
    cache[row_index_D[level]][col].LRU_age = 1;
    int j;
    for(j = 0; j < dcache_setup[level].num_cols; j++)
    {
      if(cache[row_index_D[level]][j].LRU_age == 0)
      {
        /* This block and the rest have not been used yet */
        break;
      }
      else if(j != col)
      {
        cache[row_index_D[level]][j].LRU_age++;
      }
    }
  }
}
/* Calculates number of bits used for word, row, and tag and then uses that to
  calculate the shift and mask amounts for picking apart the address */
void setup_cache(CacheInfo i, CacheSetup* s)
{
  int tag_bits, row_bits, word_bits, byte_bits;
  s->num_rows = i.num_blocks/i.associativity;
  s->num_cols = i.associativity;
  /* Number of bits for each part of the address */
	byte_bits = 2;
	word_bits = power_of_two(i.words_per_block);
	row_bits = power_of_two(s->num_rows);
	tag_bits = 32 - byte_bits - word_bits - row_bits;
  /* Calculate shift and mask  */
	s->word_shift = byte_bits;
	s->row_shift = byte_bits + word_bits;
	s->tag_shift = byte_bits + word_bits + row_bits;
	s->word_mask = (1 << word_bits) - 1;
	s->row_mask = (1 << row_bits) - 1;
	s->tag_mask = (1 << tag_bits) - 1;
}

void setup_caches()
{
	/* Setting up my caches here! */
	setup_cache(icache_info, &icache_setup);

  /* Allocate memory for i-cache array */
  icache = calloc(sizeof(void*), icache_setup.num_rows);
  int x, y;
  for (x = 0; x < icache_setup.num_rows; x++)
  {
    icache[x] = calloc(sizeof(CacheBlock), icache_setup.num_cols);
  }
  /* initialize all the cache blocks in the icache */
  //printf("numrows: %d and numcols: %d\n", icache_setup.num_rows, icache_setup.num_cols);
  for (x = 0; x < icache_setup.num_rows; x++) {
    for(y = 0; y < icache_setup.num_cols; y++) {
      icache[x][y].valid_bit = 0;
      icache[x][y].LRU_age = 0;
    }
  }
  if(dcache_info[0].num_blocks != 0)
  {
    setup_cache(dcache_info[0], &dcache_setup[0]);
    /* Allocate memory for d-cache array */
    dcache = calloc(sizeof(void*), dcache_setup[0].num_rows);
    for (x = 0; x < dcache_setup[0].num_rows; x++)
    {
      dcache[x] = calloc(sizeof(CacheBlock), dcache_setup[0].num_cols);
    }
    /* initialize all the cache blocks in the dcache */
    //printf("numrows: %d and numcols: %d\n", icache_setup.num_rows, icache_setup.num_cols);
    for (x = 0; x < dcache_setup[0].num_rows; x++) {
      for(y = 0; y < dcache_setup[0].num_cols; y++) {
        dcache[x][y].valid_bit = 0;
        dcache[x][y].dirty_bit = 0;
        dcache[x][y].LRU_age = 0;
      }
    }
    if(dcache_info[1].num_blocks != 0)
    {
      setup_cache(dcache_info[1], &dcache_setup[1]);
      /* Allocate memory for d-cache array */
      dcache2 = calloc(sizeof(void*), dcache_setup[1].num_rows);
      for (x = 0; x < dcache_setup[1].num_rows; x++)
      {
        dcache2[x] = calloc(sizeof(CacheBlock), dcache_setup[1].num_cols);
      }
      /* initialize all the cache blocks in the dcache */
      //printf("numrows: %d and numcols: %d\n", icache_setup.num_rows, icache_setup.num_cols);
      for (x = 0; x < dcache_setup[1].num_rows; x++) {
        for(y = 0; y < dcache_setup[1].num_cols; y++) {
          dcache2[x][y].valid_bit = 0;
          dcache2[x][y].dirty_bit = 0;
          dcache2[x][y].LRU_age = 0;
        }
      }
    }
    if(dcache_info[2].num_blocks != 0)
    {
      setup_cache(dcache_info[2], &dcache_setup[2]);
      /* Allocate memory for d-cache array */
      dcache3 = calloc(sizeof(void*), dcache_setup[2].num_rows);
      for (x = 0; x < dcache_setup[2].num_rows; x++)
      {
        dcache3[x] = calloc(sizeof(CacheBlock), dcache_setup[2].num_cols);
      }
      /* initialize all the cache blocks in the dcache */
      //printf("numrows: %d and numcols: %d\n", icache_setup.num_rows, icache_setup.num_cols);
      for (x = 0; x < dcache_setup[2].num_rows; x++) {
        for(y = 0; y < dcache_setup[2].num_cols; y++) {
          dcache3[x][y].valid_bit = 0;
          dcache3[x][y].dirty_bit = 0;
          dcache3[x][y].LRU_age = 0;
        }
      }
    }
  }
    /* Intializes random number generator */
    srand(1000);
    //srand((unsigned int)time(NULL));
	/* This call to dump_cache_info is just to show some debugging information
	and you may remove it. */
	//dump_cache_info();
}
void accessI(addr_t address){
	/* Picking apart the address */
	word_index_I = (address >> icache_setup.word_shift) & icache_setup.word_mask;
	row_index_I = (address >> icache_setup.row_shift) & icache_setup.row_mask;
	tag_I = (address >> icache_setup.tag_shift) & icache_setup.tag_mask;
  //printf("row index: %d\ntag_I: %d\n", row_index_I, tag_I);

	icache_stats.num_reads++;
  col_index_I = 0;
  while(1)
  {
    if(icache[row_index_I][col_index_I].valid_bit == 1)
    {
      if(icache[row_index_I][col_index_I].tag == tag_I)
      {
        /*hit*/
        updateAge(col_index_I);
        break;
      }
      else if(icache_info.associativity == 1)
    	{
        /*conflict miss*/
    		icache_stats.conflict_reads++;
    		icache_stats.words_read_mem += icache_info.words_per_block;
    		icache[row_index_I][col_index_I].tag = tag_I;
        break;
    	}
      else if(col_index_I == icache_setup.num_cols-1)
      {
        /* Reached the end of the row and need to kick out a block*/
        icache_stats.capacity_reads++;
        icache_stats.words_read_mem += icache_info.words_per_block;
        if(icache_info.replacement == Replacement_RANDOM)
        {
          /* Randomly replace a block in the row */
          icache[row_index_I][rand() % icache_setup.num_cols].tag = tag_I;
        }
        else
        {
          int j;
          int oldest = icache[row_index_I][0].LRU_age;
          int oldest_index = 0;
          /* Find oldest cache block */
          for(j = 1; j < icache_setup.num_cols; j++)
          {
            if(icache[row_index_I][j].LRU_age > oldest) {
              oldest = icache[row_index_I][j].LRU_age;
              oldest_index = j;
            }
          }
          /* Replace the LRU cache block with new data */
          icache[row_index_I][oldest_index].tag = tag_I;
          updateAge(oldest_index);
        }
        break;
      }
      else
      {
        /* keep looking through row*/
        col_index_I++;
      }
    }
    else
  	{
      /* Compulsory miss - Cache slot used to be empty */
  		icache_stats.compulsory_reads++;
  		icache_stats.words_read_mem += icache_info.words_per_block;
  		icache[row_index_I][col_index_I].valid_bit = 1;
  		icache[row_index_I][col_index_I].tag = tag_I;
      updateAge(col_index_I);
      break;
  	}

  }
}
void accessD_Read(addr_t address, int level, CacheBlock** cache){
	/* Picking apart the address */
	word_index_D[level] = (address >> dcache_setup[level].word_shift) & dcache_setup[level].word_mask;
	row_index_D[level] = (address >> dcache_setup[level].row_shift) & dcache_setup[level].row_mask;
	tag_D[level] = (address >> dcache_setup[level].tag_shift) & dcache_setup[level].tag_mask;
  //printf("row index: %d\ntag_I: %d\n", row_index_I, tag_I);

	dcache_stats[level].num_reads++;
  col_index_D[level] = 0;
  while(1)
  {
    if(cache[row_index_D[level]][col_index_D[level]].valid_bit == 1)
    {
      if(cache[row_index_D[level]][col_index_D[level]].tag == tag_D[level])
      {
        /*hit*/
        updateAgeD(col_index_D[level], level, cache);
        break;
      }
      else if(dcache_info[level].associativity == 1)
    	{
        dcache_stats[level].conflict_reads++;
        if(cache[row_index_D[level]][col_index_D[level]].dirty_bit == 1)
        {
          /* write previous data in cache block to memory */
          dcache_stats[level].words_write_mem += dcache_info[level].words_per_block;
          if(level == 0 && dcache_info[1].num_blocks != 0) {
            accessD_Write(address, 1, dcache2);
          }
          if(level == 1 && dcache_info[2].num_blocks != 0) {
            accessD_Write(address, 2, dcache3);
          }
        }
    		dcache_stats[level].words_read_mem += dcache_info[level].words_per_block;
    		cache[row_index_D[level]][col_index_D[level]].tag = tag_D[level];
        cache[row_index_D[level]][col_index_D[level]].dirty_bit = 0;
        if(level == 0 && dcache_info[1].num_blocks != 0) {
          accessD_Read(address, 1, dcache2);
        }
        if(level == 1 && dcache_info[2].num_blocks != 0) {
          accessD_Read(address, 2, dcache3);
        }
        break;
    	}
      else if(col_index_D[level] == dcache_setup[level].num_cols-1)
      {
        /* Reached the end of the row and need to kick out a block*/
        dcache_stats[level].capacity_reads++;
        dcache_stats[level].words_read_mem += dcache_info[level].words_per_block;
        if(dcache_info[level].replacement == Replacement_RANDOM)
        {
          if(cache[row_index_D[level]][col_index_D[level]].dirty_bit == 1)
          {
            /* write previous data in cache block to memory */
            dcache_stats[level].words_write_mem += dcache_info[level].words_per_block;
            if(level == 0 && dcache_info[1].num_blocks != 0) {
              accessD_Write(address, 1, dcache2);
            }
            if(level == 1 && dcache_info[2].num_blocks != 0) {
              accessD_Write(address, 2, dcache3);
            }
          }
          col_index_D[level] = rand() % dcache_setup[level].num_cols;
          cache[row_index_D[level]][col_index_D[level]].tag = tag_D[level];
          cache[row_index_D[level]][col_index_D[level]].dirty_bit = 0;
        }
        else
        {
          int j;
          int oldest = cache[row_index_D[level]][level].LRU_age;
          int oldest_index = 0;
          /* Find oldest block to replace */
          for(j = 1; j < dcache_setup[level].num_cols; j++)
          {
            if(cache[row_index_D[level]][j].LRU_age > oldest) {
              oldest = dcache[row_index_D[level]][j].LRU_age;
              oldest_index = j;
            }
          }
          if(cache[row_index_D[level]][oldest_index].dirty_bit == 1)
          {
            /* write previous data in cache block to memory */
            dcache_stats[level].words_write_mem += dcache_info[level].words_per_block;
            if(level == 0 && dcache_info[1].num_blocks != 0) {
              accessD_Write(address, 1, dcache2);
            }
            if(level == 1 && dcache_info[2].num_blocks != 0) {
              accessD_Write(address, 2, dcache3);
            }
          }
          cache[row_index_D[level]][oldest_index].tag = tag_D[level];
          cache[row_index_D[level]][oldest_index].dirty_bit = 0;
          updateAgeD(oldest_index, level, cache);
        }
        if(level == 0 && dcache_info[1].num_blocks != 0) {
          accessD_Read(address, 1, dcache2);
        }
        if(level == 1 && dcache_info[2].num_blocks != 0) {
          accessD_Read(address, 2, dcache3);
        }
        break;
      }
      else
      {
        /* keep looking*/
        col_index_D[level]++;
      }
    }
    else
  	{
  		dcache_stats[level].compulsory_reads++;
  		dcache_stats[level].words_read_mem += dcache_info[level].words_per_block;
  		cache[row_index_D[level]][col_index_D[level]].valid_bit = 1;
  		cache[row_index_D[level]][col_index_D[level]].tag = tag_D[level];
      updateAgeD(col_index_D[level], level, cache);
      if(level == 0 && dcache_info[1].num_blocks != 0) {
        accessD_Read(address, 1, dcache2);
      }
      if(level == 1 && dcache_info[2].num_blocks != 0) {
        accessD_Read(address, 2, dcache3);
      }
      break;
  	}
  }
}
void accessD_Write(addr_t address, int level, CacheBlock** cache)
{
  /* Picking apart the address */
	word_index_D[level] = (address >> dcache_setup[level].word_shift) & dcache_setup[level].word_mask;
	row_index_D[level] = (address >> dcache_setup[level].row_shift) & dcache_setup[level].row_mask;
	tag_D[level] = (address >> dcache_setup[level].tag_shift) & dcache_setup[level].tag_mask;

  dcache_stats[level].num_writes++;
  /* Write-through, write-no-allocate (aka write-around)*/
  if(dcache_info[level].write_scheme == Write_WRITE_THROUGH && dcache_info[level].allocate_scheme == Allocate_NO_ALLOCATE)
  {
    dcache_stats[level].words_write_mem++;
    if(level == 0 && dcache_info[1].num_blocks != 0) {
      accessD_Write(address, 1, dcache2);
    }
    if(level == 1 && dcache_info[2].num_blocks != 0) {
      accessD_Write(address, 2, dcache3);
    }
    col_index_D[level] = 0;
    while(1)
    {
      if(cache[row_index_D[level]][col_index_D[level]].valid_bit == 1)
      {
        if(cache[row_index_D[level]][col_index_D[level]].tag == tag_D[level])
        {
          /*hit*/
          /*data written through cache and memory*/
          updateAgeD(col_index_D[level], level, cache);
          break;
        }
        else if(dcache_info[level].associativity == 1)
      	{
      		dcache_stats[level].conflict_writes++;
          break;
      	}
        else if(col_index_D[level] == dcache_setup[level].num_cols-1)
        {
          /* Reached the end of the row and need to kick out a block*/
          dcache_stats[level].capacity_writes++;
          break;
        }
        else
        {
          /* keep looking*/
          col_index_D[level]++;
        }
      }
      else
    	{
        if(dcache_info[level].associativity == 1)
      	{
      		dcache_stats[level].conflict_writes++;
      	}
        else
        {
          dcache_stats[level].capacity_writes++;
        }
        break;
    	}
    }
  }
  /* Write-through, write-allocate */
  else if(dcache_info[level].write_scheme == Write_WRITE_THROUGH && dcache_info[level].allocate_scheme == Allocate_ALLOCATE )
  {
    col_index_D[level] = 0;
    while(1)
    {
      if(cache[row_index_D[level]][col_index_D[level]].valid_bit == 1)
      {
        if(cache[row_index_D[level]][col_index_D[level]].tag == tag_D[level])
        {
          /*hit*/
          updateAgeD(col_index_D[level], level, cache);
          break;
        }
        else if(dcache_info[level].associativity == 1)
      	{
          if(dcache_info[level].words_per_block > 1) {
            dcache_stats[level].words_read_mem += dcache_info[level].words_per_block;
            if(level == 0 && dcache_info[1].num_blocks != 0) {
              accessD_Read(address, 1, dcache2);
            }
            if(level == 1 && dcache_info[2].num_blocks != 0) {
              accessD_Read(address, 2, dcache3);
            }
          }
          cache[row_index_D[level]][col_index_D[level]].tag = tag_D[level];
          dcache_stats[level].conflict_writes++;
          break;
      	}
        else if(col_index_D[level] == dcache_setup[level].num_cols-1)
        {
          /* Reached the end of the row and need to kick out a block*/
          dcache_stats[level].capacity_writes++;
          if(dcache_info[level].words_per_block > 1) {
            dcache_stats[level].words_read_mem += dcache_info[level].words_per_block;
            if(level == 0 && dcache_info[1].num_blocks != 0) {
              accessD_Read(address, 1, dcache2);
            }
            if(level == 1 && dcache_info[2].num_blocks != 0) {
              accessD_Read(address, 2, dcache3);
            }
          }
          if(dcache_info[level].replacement == Replacement_RANDOM)
          {
            cache[row_index_D[level]][rand() % dcache_setup[level].num_cols].tag = tag_D[level];
          }
          else
          {
            int j;
            int oldest = cache[row_index_D[level]][level].LRU_age;
            int oldest_index = 0;
            for(j = 1; j < dcache_setup[level].num_cols; j++)
            {
              if(cache[row_index_D[level]][j].LRU_age > oldest) {
                oldest = cache[row_index_D[level]][j].LRU_age;
                oldest_index = j;
              }
            }
            cache[row_index_D[level]][oldest_index].tag = tag_D[level];
            updateAgeD(oldest_index, level, cache);
          }
          break;
        }
        else
        {
          /* keep looking*/
          col_index_D[level]++;
        }
      }
      else
    	{
        if(dcache_info[level].words_per_block > 1) {
          dcache_stats[level].words_read_mem += dcache_info[level].words_per_block;
          if(level == 0 && dcache_info[1].num_blocks != 0) {
            accessD_Read(address, 1, dcache2);
          }
          if(level == 1 && dcache_info[2].num_blocks != 0) {
            accessD_Read(address, 2, dcache3);
          }
        }
        cache[row_index_D[level]][col_index_D[level]].valid_bit = 1;
        cache[row_index_D[level]][col_index_D[level]].tag = tag_D[level];
        dcache_stats[level].compulsory_writes++;
        updateAgeD(col_index_D[level], level, cache);
        break;
    	}
    }
    dcache_stats[level].words_write_mem++;
    if(level == 0 && dcache_info[1].num_blocks != 0) {
      accessD_Write(address, 1, dcache2);
    }
    if(level == 1 && dcache_info[2].num_blocks != 0) {
      accessD_Write(address, 2, dcache3);
    }
  }
  /* Write Back and Write Allocate */
  else if(dcache_info[level].write_scheme == Write_WRITE_BACK && dcache_info[level].allocate_scheme == Allocate_ALLOCATE )
  {
    col_index_D[level] = 0;
    while(1)
    {
      if(cache[row_index_D[level]][col_index_D[level]].valid_bit == 1)
      {
        if(cache[row_index_D[level]][col_index_D[level]].tag == tag_D[level])
        {
          /*hit*/
          /*update cache but not memory*/
          updateAgeD(col_index_D[level], level, cache);
          cache[row_index_D[level]][col_index_D[level]].dirty_bit = 1;
          break;
        }
        else if(dcache_info[level].associativity == 1)
      	{
          /*conflict miss*/
          if(cache[row_index_D[level]][col_index_D[level]].dirty_bit == 1)
          {
            /* write previous data in cache block to memory */
            dcache_stats[level].words_write_mem += dcache_info[level].words_per_block;
            if(level == 0 && dcache_info[1].num_blocks != 0) {
              accessD_Write(address, 1, dcache2);
            }
            if(level == 1 && dcache_info[2].num_blocks != 0) {
              accessD_Write(address, 2, dcache3);
            }
          }
          /* read whole cache block from memory */
          if(dcache_info[level].words_per_block > 1) {
            dcache_stats[level].words_read_mem += dcache_info[level].words_per_block;
            if(level == 0 && dcache_info[1].num_blocks != 0) {
              accessD_Read(address, 1, dcache2);
            }
            if(level == 1 && dcache_info[2].num_blocks != 0) {
              accessD_Read(address, 2, dcache3);
            }
          }
          /* write new data to cache and update dirty bit*/
          cache[row_index_D[level]][col_index_D[level]].tag = tag_D[level];
          cache[row_index_D[level]][col_index_D[level]].dirty_bit = 1;
          dcache_stats[level].conflict_writes++;
          break;
      	}
        else if(col_index_D[level] == dcache_setup[level].num_cols-1)
        {
          /* Reached the end of the row and need to kick out a block*/
          dcache_stats[level].capacity_writes++;
          if(dcache_info[level].replacement == Replacement_RANDOM)
          {
            col_index_D[level] = rand() % dcache_setup[level].num_cols;
            if(cache[row_index_D[level]][col_index_D[level]].dirty_bit == 1)
            {
              /* write previous data in cache block to memory */
              dcache_stats[level].words_write_mem += dcache_info[level].words_per_block;
              if(level == 0 && dcache_info[1].num_blocks != 0) {
                accessD_Write(address, 1, dcache2);
              }
              if(level == 1 && dcache_info[2].num_blocks != 0) {
                accessD_Write(address, 2, dcache3);
              }
            }
            if(dcache_info[level].words_per_block > 1) {
              dcache_stats[level].words_read_mem += dcache_info[level].words_per_block;
              if(level == 0 && dcache_info[1].num_blocks != 0) {
                accessD_Read(address, 1, dcache2);
              }
              if(level == 1 && dcache_info[2].num_blocks != 0) {
                accessD_Read(address, 2, dcache3);
              }
            }
            /* write new data to cache and update dirty bit*/
            cache[row_index_D[level]][col_index_D[level]].tag = tag_D[level];
            cache[row_index_D[level]][col_index_D[level]].dirty_bit = 1;
          }
          else
          {
            int j;
            int oldest = cache[row_index_D[level]][0].LRU_age;
            int oldest_index = 0;
            /* Finds oldest cache block */
            for(j = 1; j < dcache_setup[level].num_cols; j++)
            {
              if(cache[row_index_D[level]][j].LRU_age > oldest) {
                oldest = cache[row_index_D[level]][j].LRU_age;
                oldest_index = j;
              }
            }
            if(cache[row_index_D[level]][oldest_index].dirty_bit == 1)
            {
              /* write previous data in cache block to memory */
              dcache_stats[level].words_write_mem += dcache_info[0].words_per_block;
              if(level == 0 && dcache_info[1].num_blocks != 0) {
                accessD_Write(address, 1, dcache2);
              }
              if(level == 1 && dcache_info[2].num_blocks != 0) {
                accessD_Write(address, 2, dcache3);
              }
            }
            if(dcache_info[level].words_per_block > 1) {
              dcache_stats[level].words_read_mem += dcache_info[level].words_per_block;
              if(level == 0 && dcache_info[1].num_blocks != 0) {
                accessD_Read(address, 1, dcache2);
              }
              if(level == 1 && dcache_info[2].num_blocks != 0) {
                accessD_Read(address, 2, dcache3);
              }
            }
            /* write new data to cache and update dirty bit*/
            cache[row_index_D[level]][oldest_index].tag = tag_D[level];
            cache[row_index_D[level]][oldest_index].dirty_bit = 1;
            updateAgeD(oldest_index, level, cache);
          }
          break;
        }
        else
        {
          /* keep looking*/
          col_index_D[level]++;
        }
      }
      else
    	{
        cache[row_index_D[level]][col_index_D[level]].valid_bit = 1;
        cache[row_index_D[level]][col_index_D[level]].tag = tag_D[level];
        cache[row_index_D[level]][col_index_D[level]].dirty_bit = 1;
        if(dcache_info[level].words_per_block > 1) {
          dcache_stats[level].words_read_mem += dcache_info[level].words_per_block;
          if(level == 0 && dcache_info[1].num_blocks != 0) {
            accessD_Read(address, 1, dcache2);
          }
          if(level == 1 && dcache_info[2].num_blocks != 0) {
            accessD_Read(address, 2, dcache3);
          }
        }
        dcache_stats[level].compulsory_writes++;
        updateAgeD(col_index_D[level], level, cache);
        break;
    	}
    }
  }
}
void handle_access(AccessType type, addr_t address)
{
	/* This is where all the fun stuff happens! This function is called to
	simulate a memory access. You figure out what type it is, and do all your
	fun simulation stuff from here. */
	switch(type)
	{
		case Access_I_FETCH:
			accessI(address);
			//printf("I_FETCH at %08lx\n", address);
			break;
		case Access_D_READ:
			//printf("D_READ at %08lx\n", address);
      if(dcache_info[0].num_blocks != 0)
      {
        accessD_Read(address, 0, dcache);
      }
			break;
		case Access_D_WRITE:
			//printf("D_WRITE at %08lx\n", address);
      if(dcache_info[0].num_blocks != 0)
      {
        accessD_Write(address, 0, dcache);
      }
			break;
	}
}
void print_stats_D(int level)
{
  dcache_stats[level].total_misses = dcache_stats[level].compulsory_reads + dcache_stats[level].conflict_reads + dcache_stats[level].capacity_reads;
  dcache_stats[level].miss_rate = ((double)dcache_stats[level].total_misses / (double)dcache_stats[level].num_reads) * 100;
  printf("\n\nL%d D-Cache statistics: \n", level+1);
  printf("\tNumber of reads performed: %d\n\tWords read from memory: %d\n", dcache_stats[level].num_reads,dcache_stats[level].words_read_mem);
  printf("\tNumber of writes performed: %d\n\tWords written to memory: %d\n", dcache_stats[level].num_writes, dcache_stats[level].words_write_mem);
  printf("\tRead misses:\n\t\tCompulsory misses: %d", dcache_stats[level].compulsory_reads);
  if(dcache_info[level].associativity == 1) {
    printf("\n\t\tConflict misses: %d\n", dcache_stats[level].conflict_reads);
  }
  else{
    printf("\n\t\tCapacity misses: %d\n", dcache_stats[level].capacity_reads);
  }
  printf("\t\tTotal read misses: %d\n\t\tMiss rate: %.2f%%\n", dcache_stats[level].total_misses, dcache_stats[level].miss_rate);
  printf("\t\tTotal read misses (excluding compulsory): %d\n\t\tMiss rate: %.2f%%\n", (dcache_stats[level].total_misses - dcache_stats[level].compulsory_reads), (double)(dcache_stats[level].total_misses - dcache_stats[level].compulsory_reads)/(double)dcache_stats[level].num_reads*100);
  printf("\tWrite misses:\n\t\tCompulsory misses: %d", dcache_stats[level].compulsory_writes);
  if(dcache_info[level].associativity == 1) {
    printf("\n\t\tConflict misses: %d\n", dcache_stats[level].conflict_writes);
  }
  else{
    printf("\n\t\tCapacity misses: %d\n", dcache_stats[level].capacity_writes);
  }
  dcache_stats[level].total_misses = dcache_stats[level].compulsory_writes+dcache_stats[level].conflict_writes+dcache_stats[level].capacity_writes;
  dcache_stats[level].miss_rate = ((double)dcache_stats[level].total_misses / (double)dcache_stats[level].num_writes) * 100;
  printf("\t\tTotal write misses: %d\n\t\tMiss rate: %.2f%%\n", dcache_stats[level].total_misses, dcache_stats[level].miss_rate);
  printf("\t\tTotal write misses (excluding compulsory): %d\n\t\tMiss rate: %.2f%%\n", (dcache_stats[level].conflict_writes+dcache_stats[level].capacity_writes), (double)(dcache_stats[level].conflict_writes+dcache_stats[level].capacity_writes)/(double)dcache_stats[level].num_writes*100);
}
void print_statistics()
{
	/* Finally, after all the simulation happens, you have to show what the
	results look like. Do that here.*/
  icache_stats.total_misses =  icache_stats.compulsory_reads + icache_stats.conflict_reads + icache_stats.capacity_reads;
  icache_stats.miss_rate = ((double)icache_stats.total_misses / (double)icache_stats.num_reads) * 100;
	printf("I-Cache statistics: \n");
	printf("\tNumber of reads performed: %d\n\tWords read from memory: %d\n", icache_stats.num_reads,icache_stats.words_read_mem);
	printf("\tRead misses:\n\t\tCompulsory misses: %d", icache_stats.compulsory_reads);
  if(icache_info.associativity == 1) {
    printf("\n\t\tConflict misses: %d\n", icache_stats.conflict_reads);
  }
  else{
    printf("\n\t\tCapacity misses: %d\n", icache_stats.capacity_reads);
  }
	printf("\t\tTotal read misses: %d\n\t\tMiss rate: %.2f%%\n", icache_stats.total_misses, icache_stats.miss_rate);
	printf("\t\tTotal read misses (excluding compulsory): %d\n\t\tMiss rate: %.2f%%\n", (icache_stats.conflict_reads + icache_stats.capacity_reads), (double)(icache_stats.conflict_reads + icache_stats.capacity_reads)/(double)icache_stats.num_reads*100);

  if(dcache_info[0].num_blocks != 0)
  {
    print_stats_D(0);
  }
  if(dcache_info[1].num_blocks != 0)
  {
    print_stats_D(1);
  }
  if(dcache_info[2].num_blocks != 0)
  {
    print_stats_D(2);
  }
}

/*******************************************************************************
*
*
*
* DO NOT MODIFY ANYTHING BELOW THIS LINE!
*
*
*
*******************************************************************************/

void dump_cache_info()
{
	int i;
	CacheInfo* info;

	printf("Instruction cache:\n");
	printf("\t%d blocks\n", icache_info.num_blocks);
	printf("\t%d word(s) per block\n", icache_info.words_per_block);
	printf("\t%d-way associative\n", icache_info.associativity);

	if(icache_info.associativity > 1)
	{
		printf("\treplacement: %s\n\n",
			icache_info.replacement == Replacement_LRU ? "LRU" : "Random");
	}
	else
		printf("\n");

	for(i = 0; i < 3 && dcache_info[i].num_blocks != 0; i++)
	{
		info = &dcache_info[i];

		printf("Data cache level %d:\n", i);
		printf("\t%d blocks\n", info->num_blocks);
		printf("\t%d word(s) per block\n", info->words_per_block);
		printf("\t%d-way associative\n", info->associativity);

		if(info->associativity > 1)
		{
			printf("\treplacement: %s\n", info->replacement == Replacement_LRU ?
				"LRU" : "Random");
		}

		printf("\twrite scheme: %s\n", info->write_scheme == Write_WRITE_BACK ?
			"write-back" : "write-through");

		printf("\tallocation scheme: %s\n\n",
			info->allocate_scheme == Allocate_ALLOCATE ?
			"write-allocate" : "write-no-allocate");
	}
}

void read_trace_line(FILE* trace)
{
	char line[100];
	addr_t address;
	char type;

	if(fgets(line, sizeof(line), trace) == NULL)
		return;

	if(sscanf(line, "0x%lx %c", &address, &type) < 2)
	{
		// fprintf(stderr, "Malformed trace file.\n");
		// exit(1);
    return;
	}

	switch(type)
	{
		case 'R': handle_access(Access_D_READ, address);  break;
		case 'W': handle_access(Access_D_WRITE, address); break;
		case 'I': handle_access(Access_I_FETCH, address); break;
		default:
			fprintf(stderr, "Malformed trace file: invalid access type '%c'.\n",
				type);
			exit(1);
			break;
	}
}

static void bad_params(const char* msg)
{
	fprintf(stderr, msg);
	fprintf(stderr, "\n");
	exit(1);
}

#define streq(a, b) (strcmp((a), (b)) == 0)

FILE* parse_arguments(int argc, char** argv)
{
	int i;
	int have_inst = 0;
	int have_data[3] = {};
	FILE* trace = NULL;
	int level;
	int num_blocks;
	int words_per_block;
	int associativity;
	char write_scheme;
	char alloc_scheme;
	char replace_scheme;
	int converted;

	for(i = 1; i < argc; i++)
	{
		if(streq(argv[i], "-I"))
		{
			if(i == (argc - 1))
				bad_params("Expected parameters after -I.");

			if(have_inst)
				bad_params("Duplicate I-cache parameters.");
			have_inst = 1;

			i++;
			converted = sscanf(argv[i], "%d:%d:%d:%c",
				&icache_info.num_blocks,
				&icache_info.words_per_block,
				&icache_info.associativity,
				&replace_scheme);

			if(converted < 4)
				bad_params("Invalid I-cache parameters.");

			if(icache_info.associativity > 1)
			{
				if(replace_scheme == 'R')
					icache_info.replacement = Replacement_RANDOM;
				else if(replace_scheme == 'L')
					icache_info.replacement = Replacement_LRU;
				else
					bad_params("Invalid I-cache replacement scheme.");
			}
		}
		else if(streq(argv[i], "-D"))
		{
			if(i == (argc - 1))
				bad_params("Expected parameters after -D.");

			i++;
			converted = sscanf(argv[i], "%d:%d:%d:%d:%c:%c:%c",
				&level, &num_blocks, &words_per_block, &associativity,
				&replace_scheme, &write_scheme, &alloc_scheme);

			if(converted < 7)
				bad_params("Invalid D-cache parameters.");

			if(level < 1 || level > 3)
				bad_params("Invalid D-cache level.");

			level--;
			if(have_data[level])
				bad_params("Duplicate D-cache level parameters.");

			have_data[level] = 1;

			dcache_info[level].num_blocks = num_blocks;
			dcache_info[level].words_per_block = words_per_block;
			dcache_info[level].associativity = associativity;

			if(associativity > 1)
			{
				if(replace_scheme == 'R')
					dcache_info[level].replacement = Replacement_RANDOM;
				else if(replace_scheme == 'L')
					dcache_info[level].replacement = Replacement_LRU;
				else
					bad_params("Invalid D-cache replacement scheme.");
			}

			if(write_scheme == 'B')
				dcache_info[level].write_scheme = Write_WRITE_BACK;
			else if(write_scheme == 'T')
				dcache_info[level].write_scheme = Write_WRITE_THROUGH;
			else
				bad_params("Invalid D-cache write scheme.");

			if(alloc_scheme == 'A')
				dcache_info[level].allocate_scheme = Allocate_ALLOCATE;
			else if(alloc_scheme == 'N')
				dcache_info[level].allocate_scheme = Allocate_NO_ALLOCATE;
			else
				bad_params("Invalid D-cache allocation scheme.");
		}
		else
		{
			if(i != (argc - 1))
				bad_params("Trace filename should be last argument.");

			break;
		}
	}

	if(!have_inst)
		bad_params("No I-cache parameters specified.");

	if(have_data[1] && !have_data[0])
		bad_params("L2 D-cache specified, but not L1.");

	if(have_data[2] && !have_data[1])
		bad_params("L3 D-cache specified, but not L2.");

	trace = fopen(argv[argc - 1], "r");

	if(trace == NULL)
		bad_params("Could not open trace file.");

	return trace;
}

int main(int argc, char** argv)
{
	FILE* trace = parse_arguments(argc, argv);

	setup_caches();

	while(!feof(trace))
		read_trace_line(trace);

	fclose(trace);

	print_statistics();
	return 0;
}
