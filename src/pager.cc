#include <iostream>
#include <stack>
#include <queue>
#include <map>
#include <assert.h>
#include <cstring>
#include <vector>

#include "../src/vm_pager.h"

using namespace std;

struct page_status_table_entry_t {
    bool valid;
    bool dirty;
    bool resident;
    bool reference;
};

struct page_status_table_t {
    page_status_table_entry_t pstes[VM_ARENA_SIZE/VM_PAGESIZE];
};

struct process_information {
    page_status_table_t page_status_table;
    page_table_t page_table;
};

static unsigned int num_memory_pages;
static unsigned int num_disk_blocks;

static map<pid_t, process_information> process_map;

void vm_init(unsigned int memory_pages, unsigned int disk_blocks) {
    num_memory_pages = memory_pages;
    num_disk_blocks = disk_blocks;

    page_table_base_register = nullptr;
}

void vm_create(pid_t pid) {

}

void vm_switch(pid_t pid) {

}

void* vm_extend() {

}

int vm_fault(void* addr, bool write_flag) {

}

void vm_destroy() {

}

int vm_syslog(void* message, unsigned int len) {

}