#include <iostream>
#include <vector>

#include "vm_pager.h"

using namespace std;

/*
 * ****************************************************
 * * Interface for student portion of Project 2 pager *
 * ****************************************************
 */

/*
 * vm_init
 *
 * Called when the pager starts.  It should set up any internal data structures
 * needed by the pager, e.g. physical page bookkeeping, process table, disk
 * usage table, etc.
 *
 * vm_init is passed both the number of physical memory pages and the number
 * of disk blocks in the raw disk.
 */
void vm_init(unsigned int memory_pages, unsigned int disk_blocks);

/*
 * vm_create
 *
 * Called when a new process, with process identifier "pid", is added to the
 * system.  It should create whatever new elements are required for each of
 * your data structures.  The new process will only run when it's switched
 * to via vm_switch().
 */
void vm_create(pid_t pid);

/*
 * vm_switch
 *
 * Called when the kernel is switching to a new process, with process
 * identifier "pid".  This allows the pager to do any bookkeeping needed to
 * register the new process.
 */
void vm_switch(pid_t pid);

/*
 * vm_fault
 *
 * Called when current process has a fault at virtual address addr.  write_flag
 * is true if the access that caused the fault is a write.
 * Should return 0 on success, -1 on failure.
 */
int vm_fault(void *addr, bool write_flag);

/*
 * vm_destroy
 *
 * Called when current process exits.  It should deallocate all resources
 * held by the current process (page table, physical pages, disk blocks, etc.)
 */
void vm_destroy();

/*
 * vm_extend
 *
 * A request by current process to declare as valid the lowest invalid virtual
 * page in the arena.  It should return the lowest-numbered byte of the new
 * valid virtual page.  E.g., if the valid part of the arena before calling
 * vm_extend is 0x60000000-0x60003fff, the return value will be 0x60004000,
 * and the resulting valid part of the arena will be 0x60000000-0x60005fff.
 * vm_extend should return NULL on error, e.g., if the disk is out of swap
 * space.
 */
void * vm_extend();

/*
 * vm_syslog
 *
 * A request by current process to log a message that is stored in the process'
 * arena at address "message" and is of length "len".
 *
 * Should return 0 on success, -1 on failure.
 */
int vm_syslog(void *message, unsigned int len);


/*
 * *********************************************
 * * Public interface for the disk abstraction *
 * *********************************************
 *
 * Disk blocks are numbered from 0 to (disk_blocks-1), where disk_blocks
 * is the parameter passed in vm_init().
 */

/*
 * disk_read
 *
 * read block "block" from the disk into physical memory page "ppage".
 */
void disk_read(unsigned int block, unsigned int ppage);

/*
 * disk_write
 *
 * write the contents of physical memory page "ppage" onto disk block "block".
 */
void disk_write(unsigned int block, unsigned int ppage);

/*
 * ********************************************************
 * * Public interface for the physical memory abstraction *
 * ********************************************************
 *
 * Physical memory pages are numbered from 0 to (memory_pages-1), where
 * memory_pages is the parameter passed in vm_init().
 *
 * Your pager accesses the data in physical memory through the variable
 * pm_physmem, e.g. ((char *)pm_physmem)[5] is byte 5 in physical memory.
 */
void * pm_physmem;