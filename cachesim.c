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

CacheBlock** icache;
unsigned int tag;
int word_index, row_index, col_index;
int num_rows, num_cols;

int tag_bits, row_bits, word_bits, byte_bits;
int word_shift, row_shift, tag_shift;
int word_mask, row_mask, tag_mask;

int num_reads=0 , words_read_mem=0, num_writes=0, words_write_mem=0;
int compulsory_reads=0, conflict_reads=0, capacity_reads=0;
int total_misses=0;
double miss_rate=0;
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
int binary_to_decimal(int binary) {
	int decimal = 0, base = 0, remainder;
	while (binary > 0)
  {
        remainder = binary % 10;
				decimal += remainder * base;
        binary /= 10;
        base *= 2;
  }
	return decimal;
}
void updateAge(int col)
{
  /* Update LRU ages */
  if(icache_info.replacement == Replacement_LRU)
  {
    icache[row_index][col].LRU_age = 1;
    int j;
    for(j = 0; j < num_cols; j++)
    {
      if(icache[row_index][j].LRU_age == 0)
      {
        /* This block and the rest have not been used yet */
        break;
      }
      else if(j != col)
      {
        icache[row_index][j].LRU_age++;
      }
    }
  }
}
void setup_caches()
{
	/* Setting up my caches here! */
	/* Number of bits for each part of the address */
	byte_bits = 2;
	word_bits = power_of_two(icache_info.words_per_block);
	num_rows = icache_info.num_blocks/icache_info.associativity;
  num_cols = icache_info.associativity;
	row_bits = power_of_two(num_rows);
	tag_bits = 32 - byte_bits - word_bits - row_bits;
	/* Calculate shift and mask  */
	word_shift = byte_bits;
	row_shift = byte_bits + word_bits;
	tag_shift = byte_bits + word_bits + row_bits;
	word_mask = (1 << word_bits) - 1;
	row_mask = (1 << row_bits) - 1;
	tag_mask = (1 << tag_bits) - 1;
  /* Allocate memory for cache array */
  icache = calloc(sizeof(void*), num_rows);
	int x, y;
	for (x = 0; x < num_rows; x++)
  {
    icache[x] = calloc(sizeof(CacheBlock), num_cols);
  }
  /* initialize all the cache blocks in the cache */
  printf("numrows: %d and numcols: %d\n", num_rows, num_cols);
  for (x = 0; x < num_rows; x++) {
    for(y = 0; y < num_cols; y++) {
      icache[x][y].valid_bit = 0;
      icache[x][y].dirty_bit = 0;
      icache[x][y].LRU_age = 0;
    }
  }
  if(icache_info.replacement == Replacement_RANDOM) {
    /* Intializes random number generator */
    srand(1000);
    //srand((unsigned int)time(NULL));
  }

	/* This call to dump_cache_info is just to show some debugging information
	and you may remove it. */
	dump_cache_info();
}
void accessI(addr_t address){
	/* Picking apart the address */
	word_index = (address >> word_shift) & word_mask;
	row_index = (address >> row_shift) & row_mask;
	tag = (address >> tag_shift) & tag_mask;
  printf("row index: %d\ntag: %d\n", row_index, tag);

	num_reads++;
  col_index = 0;
  while(1)
  {
    if(icache[row_index][col_index].valid_bit == 1)
    {
      if(icache[row_index][col_index].tag == tag)
      {
        /*hit*/
        updateAge(col_index);
        break;
      }
      else if(icache_info.associativity == 1)
    	{
    		conflict_reads++;
    		words_read_mem += icache_info.words_per_block;
    		icache[row_index][col_index].tag = tag;
        break;
    	}
      else if(col_index == num_cols-1)
      {
        /* Reached the end of the row and need to kick out a block*/
        capacity_reads++;
        words_read_mem += icache_info.words_per_block;
        if(icache_info.replacement == Replacement_RANDOM)
        {
          icache[row_index][rand() % num_cols].tag = tag;
        }
        else
        {
          int j;
          int oldest = icache[row_index][0].LRU_age;
          int oldest_index = 0;
          for(j = 1; j < num_cols; j++)
          {
            if(icache[row_index][j].LRU_age > oldest) {
              oldest = icache[row_index][j].LRU_age;
              oldest_index = j;
            }
          }
          icache[row_index][oldest_index].tag = tag;
          updateAge(oldest_index);
        }
        break;
      }
      else
      {
        /* keep looking*/
        col_index++;
      }
    }
    else
  	{
  		compulsory_reads++;
  		words_read_mem += icache_info.words_per_block;
  		icache[row_index][col_index].valid_bit = 1;
  		icache[row_index][col_index].tag = tag;
      updateAge(col_index);
      break;
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
			break;
		case Access_D_WRITE:
			//printf("D_WRITE at %08lx\n", address);
			break;
	}
}

void print_statistics()
{
	/* Finally, after all the simulation happens, you have to show what the
	results look like. Do that here.*/
	total_misses = compulsory_reads + conflict_reads + capacity_reads;
	miss_rate = ((double)total_misses / (double)num_reads) * 100;
	printf("I-Cache statistics: \n");
	printf("\tNumber of reads performed: %d\n\tWords read from memory: %d\n", num_reads,words_read_mem) ;
	printf("\tCompulsory misses: %d\n\tConflict misses: %d\n\tCapacity misses: %d\n", compulsory_reads, conflict_reads, capacity_reads);
	printf("\tTotal read misses: %d\n\tMiss rate: %.2f%%\n", total_misses, miss_rate);
	printf("\tTotal read misses (excluding compulsory): %d\n\tMiss rate: %.2f%%\n", (total_misses- compulsory_reads), (double)(total_misses- compulsory_reads)/(double)num_reads*100);
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
				bad_params("Inalid D-cache level.");

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
