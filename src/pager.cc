#include <iostream>
#include <queue>
#include <map>

#include "../src/vm_pager.h"

using namespace std;

/*
struct page_table_entry_t {
    unsigned long ppage : 20;
    unsigned int read_enable : 1;
    unsigned int write_enable : 1;
};

struct page_table_t {
    page_table_entry_t ptes[VM_ARENA_SIZE/VM_PAGESIZE];
};
*/

struct page_status_table_entry_t {
    bool valid;
    bool dirty;
    bool resident;
    bool reference;

    page_status_table_entry_t() : valid(false), dirty(false), resident(false), reference(false) {}
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
    int max_num_page = VM_ARENA_SIZE / VM_PAGESIZE;
    process_information* running_process_information = &process_map[running_process_id];

    if (max_num_page <= (running_process_information->top_address_index + 1)) {
        return nullptr;
    }

    if (free_disk_blocks.empty()) {
        return nullptr;
    }

    if (free_memory_pages.empty()) {
        return nullptr;
    }

    running_process_information->top_address_index++;

    running_process_information->page_table;

    running_process_information->page_status_table;
}

int vm_fault(void* addr, bool write_flag) {

}

void vm_destroy() {

}

int vm_syslog(void* message, unsigned int len) {

}