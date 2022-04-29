#include <iostream>
#include <stack>
#include <queue>
#include <map>
#include <assert.h>
#include <cstring>

#include "../src/vm_pager.h"

using namespace std;

struct page_status_table_entry_t {
    bool valid;
    bool dirty;
    bool resident;
    bool reference;
    bool written;
    unsigned int disk_block;
    page_table_entry_t* pte_ptr;

    page_status_table_entry_t() {
        valid = false;
        dirty = false;
        resident = false;
        reference = false;
        written = false;
        disk_block = 0;
        pte_ptr = nullptr;
    }
};

struct process_information {
    int top_address_index;
    page_status_table_entry_t** pages;
    page_table_t page_table;

    process_information() : top_address_index(-1) {}
};

static stack<unsigned int> free_memory_pages;
static stack<unsigned int> free_disk_blocks;

static map<pid_t, process_information*> process_map;
static queue<page_status_table_entry_t*> my_clock;

static pid_t running_process_id;
static process_information* running_process_info;

static void swap_out() {
    page_status_table_entry_t* temp = my_clock.front();

    assert(temp->valid);

    while (temp->reference) {
        temp->reference = false;
        //reset read_enable so that the next read can be registered
        temp->pte_ptr->read_enable = 0;
        temp->pte_ptr->write_enable = 0;

        my_clock.pop();
        my_clock.push(temp);
        temp = my_clock.front();
    }

    if(temp->dirty && temp->written) {
        disk_write(temp->disk_block, temp->pte_ptr->ppage);
    }

    //make page_status_table_entry_t non-resident
    temp->pte_ptr->read_enable = 0;
    temp->pte_ptr->write_enable = 0;
    temp->resident = false;

    // add it back to the stack
    free_memory_pages.push(temp->pte_ptr->ppage);
    my_clock.pop();
}

static void remove(page_status_table_entry_t* p) {
    page_status_table_entry_t* rm = my_clock.front();

    while (rm != p) {
        my_clock.pop();
        my_clock.push(rm);
        rm = my_clock.front();
    }

    //delete
    my_clock.pop();
    rm = nullptr;
}

void vm_init(unsigned int memory_pages, unsigned int disk_blocks) {
    page_table_base_register = nullptr;

    for (unsigned int i = 0; i < memory_pages; ++i) {
        free_memory_pages.push(i);
    }

    for (unsigned int i = 0; i < disk_blocks; ++i) {
        free_disk_blocks.push(i);
    }
}

void vm_create(pid_t pid) {
    process_information* process = new process_information();
    process->pages = new page_status_table_entry_t*[VM_ARENA_SIZE / VM_PAGESIZE];
    process->top_address_index = -1;
    process_map[pid] = process;
}

void vm_switch(pid_t pid) {
    if (!process_map.count(pid)) {
        return;
    }

    running_process_id = pid;
    running_process_info = process_map[pid];
    page_table_base_register = &(running_process_info->page_table);
}

void* vm_extend() {
    if ((running_process_info->top_address_index + 1) >= VM_ARENA_SIZE / VM_PAGESIZE) {
        return nullptr;
    }

    if (free_disk_blocks.empty()) {
        return nullptr;
    }

    running_process_info->top_address_index++;

    page_status_table_entry_t* p = new page_status_table_entry_t;

    //init virtual pages
    p->pte_ptr = &(page_table_base_register->ptes[running_process_info->top_address_index]);

    //allocate disk_block
    p->disk_block = free_disk_blocks.top();
    free_disk_blocks.pop();

    //make non-resident
    p->pte_ptr->read_enable = 0;
    p->pte_ptr->write_enable = 0;

    p->reference = false;
    p->resident = false;
    p->written = false;
    p->valid = true;
    p->dirty = false;

    //PF and disk block allocation delayed to vm_fault

    running_process_info->pages[running_process_info->top_address_index] = p;

    return (void*)((unsigned long long)VM_ARENA_BASEADDR + running_process_info->top_address_index * VM_PAGESIZE);
}

int vm_fault(void* addr, bool write_flag) {
    if ((unsigned long long)VM_ARENA_BASEADDR + (running_process_info->top_address_index + 1) * VM_PAGESIZE <= (unsigned long long)addr) {
        return -1;
    }

    page_status_table_entry_t* page = running_process_info->pages[((unsigned long long)addr - (unsigned long long)VM_ARENA_BASEADDR) / VM_PAGESIZE];

    page->reference = true;

    if (write_flag) {
        if (!page->resident) {
            if (free_memory_pages.empty()) {
                swap_out();
            }

            page->pte_ptr->ppage = free_memory_pages.top();
            free_memory_pages.pop();

            if(!page->written) {
                memset(((char*)pm_physmem) + page->pte_ptr->ppage * VM_PAGESIZE, 0, VM_PAGESIZE);
                page->written = true;
            }
            else {
                disk_read(page->disk_block, page->pte_ptr->ppage);
            }

            my_clock.push(page);
            page->resident = true;
        }

        page->pte_ptr->write_enable = 1;
        page->pte_ptr->read_enable = 1;
        page->dirty = true;
    }
    else {
        if (!page->resident) {
            if (free_memory_pages.empty()) {
                swap_out();
            }

            page->pte_ptr->ppage = free_memory_pages.top();
            free_memory_pages.pop();

            if(!page->written) {
                memset(((char*)pm_physmem) + page->pte_ptr->ppage * VM_PAGESIZE, 0, VM_PAGESIZE);
            }
            else {
                disk_read(page->disk_block, page->pte_ptr->ppage);
            }

            page->dirty = false;

            my_clock.push(page);
            page->resident = true;
        }

        if(page->dirty) {
            page->pte_ptr->write_enable = 1;
        }
        else {
            page->pte_ptr->write_enable = 0;
        }

        page->pte_ptr->read_enable = 1;
        page->reference = true;
    }

    return 0;
}

void vm_destroy() {
    for (unsigned int i = 0; i <= running_process_info->top_address_index; ++i) {
        page_status_table_entry_t* p = running_process_info->pages[i];

        if (p->resident) {
            free_memory_pages.push(p->pte_ptr->ppage);
            remove(p);
        }

        free_disk_blocks.push(p->disk_block);
        p->valid = false;
        delete p;
    }

    delete running_process_info;
    process_map.erase(running_process_id);

    running_process_info = nullptr;
    page_table_base_register = nullptr;
}

int vm_syslog(void* message, unsigned int len) {
    unsigned long long top_address = (running_process_info->top_address_index + 1) * VM_PAGESIZE + (unsigned long long)VM_ARENA_BASEADDR;

    if (((unsigned long long)message >= top_address - len) ||
        ((unsigned long long)message >= top_address) ||
        ((unsigned long long)message < (unsigned long long)VM_ARENA_BASEADDR) ||
        len <= 0) {
        return -1;
    }

    string s;

    for (unsigned int i = 0; i < len; ++i) {
        unsigned int page_number = ((unsigned long long)message - (unsigned long long)VM_ARENA_BASEADDR + i) / VM_PAGESIZE;
        unsigned int page_offset = ((unsigned long long)message - (unsigned long long)VM_ARENA_BASEADDR + i) % VM_PAGESIZE;
        unsigned int physical_page = page_table_base_register->ptes[page_number].ppage;

        if (page_table_base_register->ptes[page_number].read_enable == 0 || !running_process_info->pages[page_number]->resident) {
            if (vm_fault((void*)((unsigned long long)message + i), false)) {
                return -1;
            }

            physical_page = page_table_base_register->ptes[page_number].ppage;
        }

        running_process_info->pages[page_number]->reference = true;
        s.append((char*)pm_physmem + physical_page * VM_PAGESIZE + page_offset, 1);
    }

    cout << "syslog \t\t\t" << s << endl;
    return 0;
}