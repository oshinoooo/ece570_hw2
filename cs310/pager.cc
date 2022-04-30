#include <iostream>
#include <queue>
#include <map>
#include <string>
#include <string.h>
#include <vector>

#include "../src/vm_pager.h"

using namespace std;

struct page {
    pid_t pid;
    unsigned int block_size;
    unsigned long virtual_page;

    unsigned int dirty;
    unsigned int resident;
    unsigned int reference;
    unsigned int valid;
};

struct process {
    pid_t pid;
    page_table_t* page_table;
    unsigned long highindex;
    vector<page*> pages;
};

static queue<unsigned int> free_memory_pages;
static queue<unsigned int> free_disk_blocks;

static map<pid_t, process*> processtable;
static queue<page*> my_clock;

static pid_t running_process_id;
static unsigned int pagebase;

void vm_init(unsigned int memory_pages, unsigned int disk_blocks) {
    pagebase = (unsigned int)(((unsigned int)VM_ARENA_SIZE / (unsigned int)VM_PAGESIZE) + 1);

    for (unsigned int i = 0; i < memory_pages; ++i) {
        free_memory_pages.push(i);
    }

    for (unsigned int i = 0; i < disk_blocks; ++i) {
        free_disk_blocks.push(i);
    }
}

void vm_create(pid_t pid) {
    unsigned long ptnum = (unsigned long)((unsigned int)VM_ARENA_SIZE / (unsigned int)VM_PAGESIZE);
    process* nprocess = new process;
    page_table_t* npt = new page_table_t ;
    page_table_entry_t* npte = new page_table_entry_t ;
    nprocess->pid = pid;
    nprocess->page_table = npt;

    for (unsigned int i = 0; i < ptnum; ++i) {
        npte = &nprocess->page_table->ptes[i];
        npte->ppage=pagebase;
        npte->read_enable = 0;
        npte->write_enable = 0;
    }

    nprocess->highindex = (unsigned long) VM_ARENA_BASEADDR;
    processtable[pid] = nprocess;
}

void vm_switch(pid_t pid) {
    running_process_id = pid;
    page_table_base_register = processtable[pid]->page_table;
}

int vm_fault(void* addr, bool write_flag) {
    unsigned long vaddrin = (unsigned long)addr;
    unsigned int pageqindex = (unsigned int)((vaddrin - (unsigned long)VM_ARENA_BASEADDR) / (unsigned long)VM_PAGESIZE);

    //go beyond the range
    if (vaddrin < (unsigned long)VM_ARENA_BASEADDR || vaddrin > processtable[running_process_id]->highindex) {
        return -1;
    }

    page_table_entry_t *pte;
    pte = &processtable[running_process_id]->page_table->ptes[pageqindex];
    if (processtable[running_process_id]->pages[pageqindex]->resident != 0) {
        page* respage = processtable[running_process_id]->pages[pageqindex];
        if (!write_flag) {
            if (respage->dirty == 1) {
                pte->write_enable = 1;
            }
        }
        else {
            pte->write_enable = 1;
            respage->dirty = 1;
            respage->valid = 1;
        }

        pte->read_enable = 1;
        respage->reference = 1;
    }
    else {
        page* np = processtable[running_process_id]->pages[pageqindex];
        if (free_memory_pages.empty()) {
            page* clockp;
            long evict = 0;
            page_table_entry_t * cte;
            while (evict == 0) {
                clockp = my_clock.front();
                my_clock.pop();
                cte = &processtable[clockp->pid]->page_table->ptes[clockp->virtual_page];

                if (clockp->reference == 0) {
                    evict = 1;
                }
                else {
                    cte->write_enable = 0;
                    cte->read_enable = 0;
                    clockp->reference = 0;
                    my_clock.push(clockp);
                }
            }

            if (clockp->dirty == 1) {
                clockp->dirty = 0;
                disk_write(clockp->block_size, cte->ppage);
            }

            pte->ppage = cte->ppage;
            cte->ppage = pagebase;
            clockp->resident = 0;
            if (np->valid == 1) {
                unsigned long pagenum = processtable[running_process_id]->page_table->ptes[pageqindex].ppage;
                disk_read(np->block_size, pagenum);
            }
            else {
                unsigned long pagenum = processtable[running_process_id]->page_table->ptes[pageqindex].ppage;
                memset((char *)pm_physmem+(pagenum)*(unsigned long)VM_PAGESIZE,0,(unsigned long)VM_PAGESIZE);
            }

            if (write_flag) {
                pte->write_enable = 1;
                np->dirty = 1;
                np->valid = 1;
            }

            pte->read_enable = 1;
            np->reference = 1;
            np->resident = 1;
        }
        else {
            pte->ppage = free_memory_pages.front();
            free_memory_pages.pop();

            if (np->valid == 1) {
                unsigned long pagenum = processtable[running_process_id]->page_table->ptes[pageqindex].ppage;
                disk_read(np->block_size, pagenum);
            }
            else {
                unsigned long pagenum = processtable[running_process_id]->page_table->ptes[pageqindex].ppage;
                memset(((char*)pm_physmem + ((pagenum) * (unsigned long)VM_PAGESIZE)), 0, (unsigned long)VM_PAGESIZE);
            }

            if (write_flag) {
                pte->write_enable = 1;
                np->dirty = 1;
                np->valid = 1;
            }
            pte->read_enable = 1;
            np->reference = 1;
            np->resident = 1;
        }
        my_clock.push(np);
    }

    return 0;
}

void vm_destroy() {
    page* np = new page;
    for (int i = 0; i < my_clock.size(); ++i) {
        np = my_clock.front();
        my_clock.pop();
        if (np->pid != running_process_id) {
            my_clock.push(np);
        }
        else {
            free_memory_pages.push(processtable[running_process_id]->page_table->ptes[np->virtual_page].ppage);
        }
    }

    delete processtable[running_process_id]->page_table;
    for (int i = 0; i < processtable[running_process_id]->pages.size(); ++i) {
        free_disk_blocks.push(processtable[running_process_id]->pages[i]->block_size);
        delete processtable[running_process_id]->pages[i];
    }

    delete processtable[running_process_id];
}

void* vm_extend() {
    unsigned long arenaaddr = (unsigned long)VM_ARENA_BASEADDR + (unsigned long)VM_ARENA_SIZE;
    unsigned long lowestbyte;
    if (free_disk_blocks.empty()) {
        return nullptr;
    }

    if (arenaaddr < processtable[running_process_id]->highindex + (unsigned long)VM_PAGESIZE) {
        return nullptr;
    }

    if (processtable[running_process_id]->highindex != (unsigned long)VM_ARENA_BASEADDR) {
        lowestbyte = processtable[running_process_id]->highindex + 1;
    }
    else {
        lowestbyte = (unsigned long)VM_ARENA_BASEADDR;
    }

    unsigned int b = free_disk_blocks.front();
    free_disk_blocks.pop();

    page * np = new page;
    np->block_size = b;
    np->dirty = 0;
    np->pid = running_process_id;
    np->resident = 0;
    np->reference = 0;
    np->valid = 0;

    np->virtual_page = (unsigned int)(lowestbyte - (unsigned long)VM_ARENA_BASEADDR) / (unsigned long)VM_PAGESIZE;
    processtable[running_process_id]->highindex = lowestbyte - 1 + (unsigned long)VM_PAGESIZE;
    processtable[running_process_id]->pages.push_back(np);
    return (void*)(unsigned long*)lowestbyte;
}

int vm_syslog(void* message, unsigned int len) {
    if ((unsigned long long)message < (unsigned long long)VM_ARENA_BASEADDR ||
        (unsigned long long)message > processtable[running_process_id]->highindex - len + 1 ||
        len <= 0) {
        return -1;
    }

    string str;

    for (unsigned int i = 0; i < len; ++i) {
        unsigned int page_number = ((unsigned long long)message + i - (unsigned long)VM_ARENA_BASEADDR) / ((unsigned long)VM_PAGESIZE);
        unsigned int page_offset = ((unsigned long long)message + i - (unsigned long)VM_ARENA_BASEADDR) % ((unsigned long)VM_PAGESIZE);
        unsigned int physical_page = (unsigned int)(processtable[running_process_id]->page_table->ptes[page_number].ppage);

        if (processtable[running_process_id]->page_table->ptes[page_number].read_enable == 0) {
            if (vm_fault((void*)((unsigned long long)message + i), false) == -1) {
                return -1;
            }
        }

        str = str + ((char*)pm_physmem)[physical_page * (unsigned long)VM_PAGESIZE + page_offset];
    }

    cout << "syslog \t\t\t" << str << endl;
    return 0;
}