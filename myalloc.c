#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct block 
{
	size_t size;
	struct block *next;
	struct block *prev;
}block_t;

#define MMAP_THRESHOLD 128000
#define BLOCK_SIZE sizeof(block_t)
#define MIN_ALLOC 3 * sysconf(_SC_PAGESIZE)
#define MIN_DEALLOC 1 * sysconf(_SC_PAGESIZE)


//head of the list of free memory blocks
static block_t *head = NULL;


// adds a block to the end of the linked list of free blocks
void add_free(block_t *blk)
{
	if(!head)
		head = blk;
	else
	{
		block_t *iter = head;
		while(iter->next)
			iter = iter->next;

		blk->next = iter->next;
		iter->next = blk;
	}
}

//deletes a block from the list
void del_free(block_t *blk)
{
	if(!blk->prev)
	{
		if(blk->next)
			head = blk->next;
		else
			head = NULL;
	}
	else
		blk->prev->next = blk->next;

	if(blk->next)
		blk->next->prev = blk->prev;
}

block_t *memreq(size_t req)	//this call might not have the right parameters
{	
	block_t *new = sbrk(req);  
	if(new == (void*) -1)
	{	
		printf("sbrk error");
		return NULL;
	}

	new->size = req - BLOCK_SIZE;

	return new;
}

//cut size from the given block
block_t *split(block_t *blk, size_t size)
{	
	//pointer to the memory address of the block
	void *blkmem = blk + 1; 
	//new block created after size bytes
	block_t	*newblk = (block_t *) ((unsigned long)blkmem + size);
	
	newblk->size = blk->size - size - BLOCK_SIZE;
	blk->size = size;

	return newblk;
}


//searches for a block of that size and if it finds a bigger one splits it
block_t *find(size_t size)
{
	block_t *iter = head;
	while(iter)
	{
		if(iter->size >= size + BLOCK_SIZE)//found a free block of appropriate size
		{
			return iter;
		}
		else
			iter = iter->next;
	}
	return NULL;
}

void *myalloc(size_t n)
{
	if(n <= 0)
		return NULL;

	if(n > MMAP_THRESHOLD)
	{
		size_t len = n + sizeof(n);	
		size_t *lenptr = mmap(0, len, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		if(lenptr == (void *) -1)
		{
			printf("mmap failure");
			exit(1);
		}

		*lenptr = len;
		return (void *)(&lenptr[1]);		
	}
	else
	{
		size_t alloc_size;	//always allocate at least a fixed amount, so as to reduce the amount of sbrk() calls to the OS
		if(n >= MIN_ALLOC)
			alloc_size = n + BLOCK_SIZE;
		else
			alloc_size = MIN_ALLOC; 


		block_t *block;
		if(!head) //first call
		{	
			block = memreq(n);
			if(!block)
				return NULL;

			head = block;
		}
		else
		{
			if(block = find(n))
			{
				del_free(block);
				if(block->size == n)	//if it has found a block of the exact size
					return block + 1;

				block_t *splitblk = split(block, n);	
				add_free(splitblk);
				return block + 1;	
				//return a pointer to the region just after block_t
				//+1 increments the address by one sizeof(block_t)	
			}
			else
			{
				block = memreq(alloc_size);
				if(!block)
					return NULL;
				if(alloc_size > n + BLOCK_SIZE)
				{
					block_t *newblk = split(block, n);
					add_free(newblk);
				}
			}
		}
		return (block + 1);		
	}
}

void myfree(void *ptr)
{
	if(ptr)
	{
		size_t *plen = (size_t *) ptr;

		plen--;
		size_t len = *plen;

		size_t unm = munmap((void *) plen, len);
		if(unm < 0)
		{
			printf("munmap failure");
			exit(1);
		}
	}
}


int main()
{
	char *str = (char *) myalloc(15);
	strcpy(str, "nadragi");

	printf("%s", str);

	return 0;
}
