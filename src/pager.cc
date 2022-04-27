#include <iostream>
#include <queue>
#include <map>
#include <cstring>

#include "vm_pager.h"

using namespace std;

struct page_status_table_entry_t {
    bool valid;
    bool dirty;
    bool resident;
    bool reference;
    bool written_to;
    unsigned int disk_block;

    page_status_table_entry_t() : valid(false), dirty(false), resident(false), reference(false), written_to(false), disk_block(-1) {}
};

struct page_status_table_t {
    page_status_table_entry_t pstes[VM_ARENA_SIZE / VM_PAGESIZE];
};

struct process_information {
    int top_address_index;

    page_status_table_t page_status_table;
    page_table_t page_table;

    process_information() : top_address_index(-1) {}
};

static unsigned int num_memory_pages;
static unsigned int num_disk_blocks;

static pid_t running_process_id;

static map<pid_t, process_information> process_map;

static queue<unsigned int> free_memory_pages;
static queue<unsigned int> free_disk_blocks;
static queue<pair<process_information*, unsigned int>> my_clock;

static void evict() {
    process_information* pi = my_clock.front().first;
    unsigned int index = my_clock.front().second;
    page_status_table_entry_t* page_status_entry = &pi->page_status_table.pstes[index];
    page_table_entry_t* page_entry = &pi->page_table.ptes[index];

    while (page_status_entry->reference) {
        page_status_entry->reference = false;
        page_entry->read_enable = 0;
        page_entry->write_enable = 0;

        my_clock.pop();
        my_clock.push({pi, index});

        pi = my_clock.front().first;
        index = my_clock.front().second;
        page_status_entry = &pi->page_status_table.pstes[index];
        page_entry = &pi->page_table.ptes[index];
    }

    if(page_status_entry->dirty && page_status_entry->written_to) {
        disk_write(page_status_entry->disk_block, page_entry->ppage);
    }

    page_entry->read_enable = 0;
    page_entry->write_enable = 0;
    page_status_entry->resident = false;

    free_memory_pages.push(page_entry->ppage);
    my_clock.pop();
}

void vm_init(unsigned int memory_pages, unsigned int disk_blocks) {
    num_memory_pages = memory_pages;
    num_disk_blocks = disk_blocks;

    running_process_id = -1;
    page_table_base_register = nullptr;

    for (unsigned int i = 0; i < num_memory_pages; ++i) {
        free_memory_pages.push(i);
    }

    for (unsigned int i = 0; i < num_disk_blocks; ++i) {
        free_disk_blocks.push(i);
    }
}

void vm_create(pid_t pid) {
    if (process_map.count(pid)) {
        return;
    }

    process_map.insert({pid, process_information()});
}

void vm_switch(pid_t pid) {
    if (!process_map.count(pid)) {
        return;
    }

    running_process_id = pid;
    page_table_base_register = &(process_map[pid].page_table);
}

void* vm_extend() {
    process_information* running_process_information = &process_map[running_process_id];

    if ((VM_ARENA_SIZE / VM_PAGESIZE) <= (running_process_information->top_address_index + 1)) {
        return nullptr;
    }

    if (free_disk_blocks.empty()) {
        return nullptr;
    }

    int top_index = ++(running_process_information->top_address_index);

    page_status_table_entry_t* page_status_entry = &running_process_information->page_status_table.pstes[top_index];

    page_status_entry->disk_block = free_disk_blocks.front();
    free_disk_blocks.pop();

    page_status_entry->valid = true;

    page_table_entry_t* page_entry = &running_process_information->page_table.ptes[top_index];
    page_entry->read_enable = 0;
    page_entry->write_enable = 0;

    return (void*)((unsigned long long)VM_ARENA_BASEADDR + top_index * VM_PAGESIZE);
}

int vm_fault(void* addr, bool write_flag) {
    process_information* running_process_information = &process_map[running_process_id];

    int top_index = running_process_information->top_address_index;

    if ((unsigned long long)VM_ARENA_BASEADDR + (top_index + 1) * VM_PAGESIZE <= (unsigned long long)addr) {
        return -1;
    }

    int cur_index = ((unsigned long long)addr - (unsigned long long)VM_ARENA_BASEADDR) / VM_PAGESIZE;
    page_status_table_entry_t* page_status_entry = &running_process_information->page_status_table.pstes[cur_index];
    page_table_entry_t* page_entry = &running_process_information->page_table.ptes[cur_index];

    page_status_entry->reference = true;

    if (write_flag) {
        if (!page_status_entry->resident) {
            if (free_memory_pages.empty()) {
                evict();
            }

            page_entry->ppage = free_memory_pages.front();
            free_memory_pages.pop();

            if (!page_status_entry->written_to) {
                memset((char*)pm_physmem + page_entry->ppage * VM_PAGESIZE, 0, VM_PAGESIZE);
                page_status_entry->written_to = true;
            }
            else {
                disk_read(page_status_entry->disk_block,page_entry->ppage);
            }

            my_clock.push({running_process_information, cur_index});
            page_status_entry->resident = true;
        }

        page_entry->write_enable = 1;
        page_entry->read_enable = 1;
        page_status_entry->dirty = true;
    }
    else {
        if (!page_status_entry->resident) {
            if (free_memory_pages.empty()) {
                evict();
            }

            page_entry->ppage = free_memory_pages.front();
            free_memory_pages.pop();

            if (!page_status_entry->written_to) {
                memset((char*)pm_physmem + page_entry->ppage * VM_PAGESIZE, 0, VM_PAGESIZE);
                page_status_entry->dirty = false;
            }
            else {
                disk_read(page_status_entry->disk_block, page_entry->ppage);
                page_status_entry->dirty = false;
            }

            my_clock.push({running_process_information, cur_index});
            page_status_entry->resident = true;
        }

        if(page_status_entry->dirty) {
            page_entry->write_enable = 1;
        }
        else {
            page_entry->write_enable = 0;
        }

        page_entry->read_enable = 1;
        page_status_entry->reference = true;
    }

    return 0;
}

void vm_destroy() {
    process_information* running_process_information = &process_map[running_process_id];

    int top_index = running_process_information->top_address_index;

    for (unsigned int i = 0; i <= top_index; ++i) {
        page_status_table_entry_t* page_status_entry = &running_process_information->page_status_table.pstes[i];
        page_table_entry_t* page_entry = &running_process_information->page_table.ptes[i];

        if (page_status_entry->resident) {
            free_memory_pages.push(page_entry->ppage);

            process_information* tmp_pi = my_clock.front().first;
            unsigned int tmp_index = my_clock.front().second;

            while (tmp_pi != running_process_information || tmp_index != i) {
                my_clock.pop();
                my_clock.push({tmp_pi, tmp_index});

                tmp_pi = my_clock.front().first;
                tmp_index = my_clock.front().second;
            }

            my_clock.pop();
        }

        free_disk_blocks.push(page_status_entry->disk_block);
        page_status_entry->valid = false;
    }

    process_map.erase(running_process_id);

    running_process_id = -1;
    page_table_base_register = nullptr;
}

int vm_syslog(void* message, unsigned int len) {
    process_information* running_process_information = &process_map[running_process_id];

    int top_index = running_process_information->top_address_index;

    unsigned long long top_address = (top_index + 1) * VM_PAGESIZE + (unsigned long long)VM_ARENA_BASEADDR;

    if (((unsigned long long)message < (unsigned long long)VM_ARENA_BASEADDR) ||
        (top_address <= (unsigned long long)message + len) ||
        len <= 0) {
        return -1;
    }

    string s;

    for (unsigned int i = 0; i < len; ++i) {
        unsigned int page_number = ((unsigned long long)message - (unsigned long long)VM_ARENA_BASEADDR + i) / VM_PAGESIZE;
        unsigned int page_offset = ((unsigned long long)message - (unsigned long long)VM_ARENA_BASEADDR + i) % VM_PAGESIZE;

        page_status_table_entry_t* page_status_entry = &running_process_information->page_status_table.pstes[page_number];
        page_table_entry_t* page_entry = &running_process_information->page_table.ptes[page_number];

        unsigned int physical_page = page_entry->ppage;

        if (page_entry->read_enable == 0 || !page_status_entry->resident) {
            if (vm_fault((void*)((unsigned long long)message + i), false)) {
                return -1;
            }

            physical_page = page_entry->ppage;
        }

        page_status_entry->reference = true;
        s.push_back(*((char*)pm_physmem + physical_page * VM_PAGESIZE + page_offset));
    }

    cout << "syslog \t\t\t" << s << endl;
    return 0;
}