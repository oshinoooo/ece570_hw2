#include <iostream>
#include <stack>
#include <queue>
#include <map>
#include <assert.h>
#include <cstring>

#include "../src/vm_pager.h"

using namespace std;

struct page {
    page_table_entry_t* pte_ptr;
    bool written_to;
    bool dirty;
    bool resident;
    bool reference;
    bool valid;
    unsigned int disk_block;
};

struct process_info {
    page_table_t* ptbl_ptr;
    page** pages;
    int top_valid_index;
};

static unsigned int num_pages;
static unsigned int num_blocks;

static stack<unsigned int> free_pages;
static stack<unsigned int> free_disk_blocks;

static map<pid_t, process_info*> page_tables;
static queue<page*> clock_q;

static pid_t current_id;
static process_info* current_process;

static void evict() {
    page* temp= clock_q.front();

    assert(temp->valid);
    while (temp->reference == true) {
        temp->reference = false;
        //reset read_enable so that the next read can be registered
        temp->pte_ptr->read_enable = 0;
        temp->pte_ptr->write_enable = 0;

        clock_q.pop();
        clock_q.push(temp);
        temp=clock_q.front();

    }
    if(temp->dirty == true && temp->written_to == true)
    {
        disk_write(temp->disk_block,temp->pte_ptr->ppage);
    }

    //make page non-resident
    temp->pte_ptr->read_enable=0;
    temp->pte_ptr->write_enable=0;
    temp->resident=false;

    // add it back to the stack
    free_pages.push(temp->pte_ptr->ppage);
    clock_q.pop();
}

static void remove(page* p) {
    page* rm=clock_q.front();
    while (rm!=p) {
        clock_q.pop();
        clock_q.push(rm);
        rm=clock_q.front();
    }

    //delete
    clock_q.pop();
    rm=NULL;
}

void vm_init(unsigned int memory_pages, unsigned int disk_blocks) {
    num_pages = memory_pages;
    num_blocks = disk_blocks;

    page_table_base_register = nullptr;

    for (int i = 0; i < memory_pages; ++i)
        free_pages.push(i);

    for (int i = 0; i < disk_blocks; ++i)
        free_disk_blocks.push(i);
}

void vm_create(pid_t pid) {
    process_info* process = new process_info();

    //create page table
    process->ptbl_ptr = new page_table_t;
    process->pages = new page*[VM_ARENA_SIZE / VM_PAGESIZE];

    //initially no pte in page table is valid
    process->top_valid_index = -1;

    page_tables[pid] = process;
}

void vm_switch(pid_t pid) {
    map<pid_t, process_info*>::const_iterator i = page_tables.find(pid);
    if (i != page_tables.end()) {
        current_id = pid;
        current_process = (*i).second;
        page_table_base_register = current_process->ptbl_ptr;
    }
}

void* vm_extend() {

    //If top valid index is exceeds the bounds of the arena, return NULL
    if ((current_process->top_valid_index+1) >= VM_ARENA_SIZE / VM_PAGESIZE)
        return NULL;
    //If there are no free disk blocks, return NULL (Eager allocation)
    if (free_disk_blocks.empty())
        return NULL;

    current_process->top_valid_index++;

    page* p = new page;

    //init virtual pages
    p->pte_ptr = &(page_table_base_register->ptes[current_process->top_valid_index]);

    //allocate disk_block
    p->disk_block = free_disk_blocks.top();
    free_disk_blocks.pop();

    //make non-resident
    p->pte_ptr->read_enable = 0;
    p->pte_ptr->write_enable = 0;

    p->reference = false;
    p->resident = false;
    p->written_to = false;
    p->valid = true;
    p->dirty = false;

    //PF and disk block allocation delayed to vm_fault

    current_process->pages[current_process->top_valid_index] = p;

    return (void *) ((unsigned long long) VM_ARENA_BASEADDR + current_process->top_valid_index * VM_PAGESIZE);
}

int vm_fault(void *addr, bool write_flag) {
    //error checking
    //outside of arena
    if (((unsigned long long)addr - (unsigned long long)VM_ARENA_BASEADDR) >= (current_process->top_valid_index+1)*VM_PAGESIZE)
        return -1;

    //page number
    page* p = current_process->pages[((unsigned long long)addr - (unsigned long long)VM_ARENA_BASEADDR) / VM_PAGESIZE];

    p->reference = true;

    //Write
    if (write_flag==true) {
        if (p->resident == false) {
            if (free_pages.empty()) {
                evict();
            }
            p->pte_ptr->ppage = free_pages.top();
            free_pages.pop();

            if(p->written_to==false)
            {
//                for(unsigned int i = 0; i < VM_PAGESIZE;i++)
//                {
//                    *(((char *)pm_physmem)+i+p->pte_ptr->ppage*VM_PAGESIZE) = 0;
//                }
                memset(((char *) pm_physmem) + p->pte_ptr->ppage * VM_PAGESIZE, 0,VM_PAGESIZE);
                p->written_to = true;
            }
            else
            {
                disk_read(p->disk_block,p->pte_ptr->ppage);
            }

            clock_q.push(p);
            p->resident = true;
        }

        p->pte_ptr->write_enable = 1;
        p->pte_ptr->read_enable = 1;
        p->dirty = true;
    }
        //Read
    else {
        if (p->resident==false) {
            if (free_pages.empty()) {
                evict();
            }

            p->pte_ptr->ppage = free_pages.top();
            free_pages.pop();

            if(p->written_to==false)
            {
//                for(unsigned int i = 0; i < VM_PAGESIZE;i++)
//                {
//                    *(((char *)pm_physmem)+i+p->pte_ptr->ppage*VM_PAGESIZE) = 0;
//                }
                memset(((char *) pm_physmem) + p->pte_ptr->ppage * VM_PAGESIZE, 0,VM_PAGESIZE);
                p->dirty=false;
            }
            else
            {
                disk_read(p->disk_block, p->pte_ptr->ppage);
                p->dirty=false;
            }
            clock_q.push(p);
            p->resident = true;
        }

        if(p->dirty==true)
        {
            p->pte_ptr->write_enable = 1;
        }
        else
        {
            p->pte_ptr->write_enable = 0;
        }
        p->pte_ptr->read_enable = 1;
        p->reference = true;
    }
    p=NULL;
    return 0;
}

void vm_destroy() {
    for (unsigned int i = 0; i <=current_process->top_valid_index; i++) {
        page* p = current_process->pages[i];
        //if page in physmem
        if (p->resident==true) {
            free_pages.push(p->pte_ptr->ppage);
            remove(p);
        }
        free_disk_blocks.push(p->disk_block);
        p->valid= false;
        delete p;
        p=NULL;
    }
    //delete current_process->ptbl_ptr;
    //delete [] current_process->pages;
    delete current_process;
    page_tables.erase(current_id);

    current_process=NULL;
    page_table_base_register=NULL;
}

int vm_syslog(void *message, unsigned int len) {
    //if not all of message is within the arena, return error
    //if len = 0, return error
    if (
            ((unsigned long long) message - (unsigned long long) VM_ARENA_BASEADDR + len) >= (current_process->top_valid_index + 1) * VM_PAGESIZE ||
            ((unsigned long long) message - (unsigned long long) VM_ARENA_BASEADDR) >= (current_process->top_valid_index + 1) * VM_PAGESIZE ||
            ((unsigned long long) message < (unsigned long long) VM_ARENA_BASEADDR) ||
            len <= 0
            )
        return -1;

    string s;

    for (unsigned int i = 0; i < len; i++) {
        //get the virtual page number from the address
        unsigned int page_num = ((unsigned long long) message - (unsigned long long) VM_ARENA_BASEADDR + i) / VM_PAGESIZE;
        unsigned int page_offset = ((unsigned long long) message - (unsigned long long) VM_ARENA_BASEADDR + i) % VM_PAGESIZE;
        unsigned int pf = page_table_base_register->ptes[page_num].ppage;
        if (page_table_base_register->ptes[page_num].read_enable == 0
            || current_process->pages[page_num]->resident==false) {
            if (vm_fault((void *) ((unsigned long long) message + i), false)) {
                return -1;
            }
            pf = page_table_base_register->ptes[page_num].ppage;
        }
        current_process->pages[page_num]->reference=true;
        s.append((char *)pm_physmem+pf * VM_PAGESIZE+ page_offset,1);
    }
    cout << "syslog\t\t\t" << s << endl;
    return 0;
}