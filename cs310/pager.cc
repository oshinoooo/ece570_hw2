#include <iostream>
#include <queue>
#include <map>
#include <string>
#include <string.h>
#include <vector>

#include "../src/vm_pager.h"

using namespace std;

struct page {
    pid_t processid;
    unsigned int blocksize;
    unsigned int dirtybit;
    unsigned int residentbit;
    unsigned int referencebit;
    unsigned int validbit;
    unsigned long vpage;
};

struct process {
    pid_t processid;
    page_table_t* pt;
    unsigned long highindex;
    vector<page*> pageq;
};

static queue<unsigned int> freepage;
static queue<unsigned int> freeDiskBlock;
static queue<page*> clockq;
static map<pid_t ,process*> processtable;
static unsigned int pagebase;
static pid_t curprocess;

void vm_init(unsigned int memory_pages, unsigned int disk_blocks) {
    for (unsigned int i = 0; i < memory_pages; ++i) {
        freepage.push(i);
    }

    for (unsigned int i = 0; i < disk_blocks; ++i) {
        freeDiskBlock.push(i);
    }

    unsigned int num = (unsigned int)(((unsigned int)VM_ARENA_SIZE / (unsigned int)VM_PAGESIZE) + 1);
    pagebase = num;
}

void vm_create(pid_t pid) {
    unsigned long ptnum = (unsigned long)((unsigned int)VM_ARENA_SIZE / (unsigned int)VM_PAGESIZE);
    process* nprocess = new process;
    page_table_t* npt = new page_table_t ;
    page_table_entry_t* npte = new page_table_entry_t ;
    nprocess->processid = pid;
    nprocess->pt = npt;

    for (unsigned int i = 0; i < ptnum; ++i) {
        npte = &nprocess->pt->ptes[i];
        npte->ppage=pagebase;
        npte->read_enable = 0;
        npte->write_enable = 0;
    }

    nprocess->highindex = (unsigned long) VM_ARENA_BASEADDR;
    processtable[pid] = nprocess;
}

void vm_switch(pid_t pid) {
    curprocess = pid;
    page_table_base_register = processtable[pid]->pt;
}

int vm_fault(void* addr, bool write_flag) {
    unsigned long vaddrin = (unsigned long)addr;
    unsigned int pageqindex = (unsigned int)((vaddrin - (unsigned long)VM_ARENA_BASEADDR) / (unsigned long)VM_PAGESIZE);

    //go beyond the range
    if (vaddrin < (unsigned long)VM_ARENA_BASEADDR || vaddrin > processtable[curprocess]->highindex) {
        return -1;
    }

    page_table_entry_t *pte;
    pte = &processtable[curprocess]->pt->ptes[pageqindex];
    if (processtable[curprocess]->pageq[pageqindex]->residentbit != 0) {
        page* respage = processtable[curprocess]->pageq[pageqindex];
        if (!write_flag) {
            if (respage->dirtybit == 1) {
                pte->write_enable = 1;
            }
        }
        else {
            pte->write_enable = 1;
            respage->dirtybit = 1;
            respage->validbit = 1;
        }

        pte->read_enable = 1;
        respage->referencebit = 1;
    }
    else {
        page* np = processtable[curprocess]->pageq[pageqindex];
        if (freepage.empty()) {
            page* clockp;
            long evict = 0;
            page_table_entry_t * cte;
            while (evict == 0) {
                clockp = clockq.front();
                clockq.pop();
                cte = &processtable[clockp->processid]->pt->ptes[clockp->vpage];

                if (clockp->referencebit == 0) {
                    evict = 1;
                }
                else {
                    cte->write_enable = 0;
                    cte->read_enable = 0;
                    clockp->referencebit = 0;
                    clockq.push(clockp);
                }
            }

            if (clockp->dirtybit == 1) {
                clockp->dirtybit = 0;
                disk_write(clockp->blocksize,cte->ppage);
            }

            pte->ppage = cte->ppage;
            cte->ppage = pagebase;
            clockp->residentbit = 0;
            if (np->validbit == 1) {
                unsigned long pagenum = processtable[curprocess]->pt->ptes[pageqindex].ppage;
                disk_read(np->blocksize,pagenum);
            }
            else {
                unsigned long pagenum = processtable[curprocess]->pt->ptes[pageqindex].ppage;
                memset((char *)pm_physmem+(pagenum)*(unsigned long)VM_PAGESIZE,0,(unsigned long)VM_PAGESIZE);
            }

            if (write_flag) {
                pte->write_enable = 1;
                np->dirtybit = 1;
                np->validbit = 1;
            }

            pte->read_enable = 1;
            np->referencebit = 1;
            np->residentbit = 1;
        }
        else {
            pte->ppage = freepage.front();
            freepage.pop();

            if (np->validbit == 1) {
                unsigned long pagenum = processtable[curprocess]->pt->ptes[pageqindex].ppage;
                disk_read(np->blocksize,pagenum);
            }
            else {
                unsigned long pagenum = processtable[curprocess]->pt->ptes[pageqindex].ppage;
                memset(((char*)pm_physmem + ((pagenum) * (unsigned long)VM_PAGESIZE)), 0, (unsigned long)VM_PAGESIZE);
            }

            if (write_flag) {
                pte->write_enable = 1;
                np->dirtybit = 1;
                np->validbit = 1;
            }
            pte->read_enable = 1;
            np->referencebit = 1;
            np->residentbit = 1;
        }
        clockq.push(np);
    }

    return 0;
}

void vm_destroy() {
    page* np = new page;
    for (int i = 0; i < clockq.size(); ++i) {
        np = clockq.front();
        clockq.pop();
        if (np->processid != curprocess) {
            clockq.push(np);
        }
        else {
            freepage.push(processtable[curprocess]->pt->ptes[np->vpage].ppage);
        }
    }

    delete processtable[curprocess]->pt;
    for (int i = 0; i < processtable[curprocess]->pageq.size(); ++i) {
        freeDiskBlock.push(processtable[curprocess]->pageq[i]->blocksize);
        delete processtable[curprocess]->pageq[i];
    }

    delete processtable[curprocess];
}

void* vm_extend() {
    unsigned long arenaaddr = (unsigned long)VM_ARENA_BASEADDR + (unsigned long)VM_ARENA_SIZE;
    unsigned long lowestbyte;
    if (freeDiskBlock.empty()) {
        return nullptr;
    }

    if (arenaaddr < processtable[curprocess]->highindex + (unsigned long)VM_PAGESIZE) {
        return nullptr;
    }

    if (processtable[curprocess]->highindex != (unsigned long)VM_ARENA_BASEADDR) {
        lowestbyte = processtable[curprocess]->highindex + 1;
    }
    else {
        lowestbyte = (unsigned long)VM_ARENA_BASEADDR;
    }

    unsigned int b = freeDiskBlock.front();
    freeDiskBlock.pop();

    page * np = new page;
    np->blocksize = b;
    np->dirtybit = 0;
    np->processid = curprocess;
    np->residentbit = 0;
    np->referencebit = 0;
    np->validbit = 0;

    np->vpage = (unsigned int)(lowestbyte - (unsigned long)VM_ARENA_BASEADDR) / (unsigned long)VM_PAGESIZE;
    processtable[curprocess]->highindex = lowestbyte - 1 + (unsigned long)VM_PAGESIZE;
    processtable[curprocess]->pageq.push_back(np);
    return (void*)(unsigned long*)lowestbyte;
}

int vm_syslog(void* message, unsigned int len) {
    unsigned long vaddrin = (unsigned long) message;
    string str;
    //go beyond the range
    if (vaddrin < (unsigned long)VM_ARENA_BASEADDR || vaddrin + (unsigned long)len - 1 > processtable[curprocess]->highindex || len <= 0) {
        return -1;
    }

    for (unsigned int i = 0; i < len; ++i) {
        unsigned int vpnum = (vaddrin + i - (unsigned long)VM_ARENA_BASEADDR) / ((unsigned long)VM_PAGESIZE);
        unsigned int offset = (vaddrin + i - (unsigned long)VM_ARENA_BASEADDR) % ((unsigned long)VM_PAGESIZE);
        if (processtable[curprocess]->pt->ptes[vpnum].read_enable == 0) {
            if (vm_fault((void*)(vaddrin + i), false ) == -1) {
                return -1;
            }
        }

        unsigned int pnum = (unsigned int)(processtable[curprocess]->pt->ptes[vpnum].ppage);
        str = str + ((char*)pm_physmem)[pnum * (unsigned long)VM_PAGESIZE + offset];
    }

    cout << "syslog \t\t\t" << str << endl;
    return 0;
}