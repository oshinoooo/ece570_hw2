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
    unsigned int disk_block_number;
    unsigned long virtual_page_number;

    unsigned int dirty;
    unsigned int resident;
    unsigned int reference;
    unsigned int valid;
};

struct process {
    pid_t pid;
    unsigned long top_address;
    page_table_t* page_table;
    vector<page*> pages;
};

static queue<unsigned int> free_memory_pages;
static queue<unsigned int> free_disk_blocks;

static map<pid_t, process*> process_map;
static queue<page*> my_clock;

static pid_t running_pid;
static unsigned int page_base;

void vm_init(unsigned int memory_pages, unsigned int disk_blocks) {
    page_table_base_register = nullptr;
    page_base = (unsigned int)(((unsigned int)VM_ARENA_SIZE / (unsigned int)VM_PAGESIZE) + 1);

    for (unsigned int i = 0; i < memory_pages; ++i) {
        free_memory_pages.push(i);
    }

    for (unsigned int i = 0; i < disk_blocks; ++i) {
        free_disk_blocks.push(i);
    }
}

void vm_create(pid_t pid) {
    process_map[pid] = new process;
    process_map[pid]->pid = pid;
    process_map[pid]->page_table = new page_table_t;
    process_map[pid]->top_address = (unsigned long)VM_ARENA_BASEADDR;

    unsigned long total_page_num = (unsigned long)((unsigned int)VM_ARENA_SIZE / (unsigned int)VM_PAGESIZE);
    for (unsigned int i = 0; i < total_page_num; ++i) {
        page_table_entry_t* tmp_entry = &process_map[pid]->page_table->ptes[i];
        tmp_entry->ppage = page_base;
        tmp_entry->read_enable = 0;
        tmp_entry->write_enable = 0;
    }
}

void vm_switch(pid_t pid) {
    running_pid = pid;
    page_table_base_register = process_map[pid]->page_table;
}

int vm_fault(void* addr, bool write_flag) {
    unsigned int page_number = (unsigned int)(((unsigned long)addr - (unsigned long)VM_ARENA_BASEADDR) / (unsigned long)VM_PAGESIZE);

    if ((unsigned long)addr < (unsigned long)VM_ARENA_BASEADDR || (unsigned long)addr > process_map[running_pid]->top_address) {
        return -1;
    }

    page_table_entry_t* cur_pte = &process_map[running_pid]->page_table->ptes[page_number];
    page* cur_page = process_map[running_pid]->pages[page_number];

    if (cur_page->resident != 0) {
        if (!write_flag) {
            if (cur_page->dirty == 1) {
                cur_pte->write_enable = 1;
            }
        }
        else {
            cur_pte->write_enable = 1;
            cur_page->dirty = 1;
            cur_page->valid = 1;
        }

        cur_pte->read_enable = 1;
        cur_page->reference = 1;
    }
    else {
        if (free_memory_pages.empty()) {
            page* tmp_page;
            page_table_entry_t* tmp_pte;

            while (true) {
                tmp_page = my_clock.front();
                tmp_pte = &process_map[tmp_page->pid]->page_table->ptes[tmp_page->virtual_page_number];
                my_clock.pop();

                if (tmp_page->reference == 0) {
                    break;
                }
                else {
                    tmp_pte->write_enable = 0;
                    tmp_pte->read_enable = 0;
                    tmp_page->reference = 0;
                    my_clock.push(tmp_page);
                }
            }

            if (tmp_page->dirty == 1) {
                tmp_page->dirty = 0;
                disk_write(tmp_page->disk_block_number, tmp_pte->ppage);
            }

            cur_pte->ppage = tmp_pte->ppage;
            tmp_pte->ppage = page_base;
            tmp_page->resident = 0;

            if (cur_page->valid == 1) {
                disk_read(cur_page->disk_block_number, cur_pte->ppage);
            }
            else {
                memset((char*)pm_physmem + (cur_pte->ppage) * (unsigned long)VM_PAGESIZE, 0, (unsigned long)VM_PAGESIZE);
            }

            if (write_flag) {
                cur_pte->write_enable = 1;
                cur_page->dirty = 1;
                cur_page->valid = 1;
            }

            cur_pte->read_enable = 1;
            cur_page->reference = 1;
            cur_page->resident = 1;
        }
        else {
            cur_pte->ppage = free_memory_pages.front();
            free_memory_pages.pop();

            if (cur_page->valid == 1) {
                disk_read(cur_page->disk_block_number, cur_pte->ppage);
            }
            else {
                memset(((char*)pm_physmem + ((cur_pte->ppage) * (unsigned long)VM_PAGESIZE)), 0, (unsigned long)VM_PAGESIZE);
            }

            if (write_flag) {
                cur_pte->write_enable = 1;
                cur_page->dirty = 1;
                cur_page->valid = 1;
            }

            cur_pte->read_enable = 1;
            cur_page->reference = 1;
            cur_page->resident = 1;
        }

        my_clock.push(cur_page);
    }

    return 0;
}

void vm_destroy() {
    for (int i = 0; i < my_clock.size(); ++i) {
        page* tmp_page = my_clock.front();
        my_clock.pop();

        if (tmp_page->pid != running_pid) {
            my_clock.push(tmp_page);
        }
        else {
            free_memory_pages.push(process_map[running_pid]->page_table->ptes[tmp_page->virtual_page_number].ppage);
        }
    }

    delete process_map[running_pid]->page_table;

    for (int i = 0; i < process_map[running_pid]->pages.size(); ++i) {
        free_disk_blocks.push(process_map[running_pid]->pages[i]->disk_block_number);
        delete process_map[running_pid]->pages[i];
    }

    delete process_map[running_pid];
    process_map.erase(running_pid);
}

void* vm_extend() {
    if (free_disk_blocks.empty()) {
        return nullptr;
    }

    if ((unsigned long)VM_ARENA_BASEADDR + (unsigned long)VM_ARENA_SIZE < process_map[running_pid]->top_address + (unsigned long)VM_PAGESIZE) {
        return nullptr;
    }

    unsigned long lowest_byte = (unsigned long)VM_ARENA_BASEADDR;
    if (process_map[running_pid]->top_address != (unsigned long)VM_ARENA_BASEADDR) {
        lowest_byte = process_map[running_pid]->top_address + 1;
    }

    page* new_page = new page;

    new_page->pid = running_pid;
    new_page->disk_block_number = free_disk_blocks.front();
    free_disk_blocks.pop();

    new_page->dirty = 0;
    new_page->resident = 0;
    new_page->reference = 0;
    new_page->valid = 0;
    new_page->virtual_page_number = (unsigned int)(lowest_byte - (unsigned long)VM_ARENA_BASEADDR) / (unsigned long)VM_PAGESIZE;

    process_map[running_pid]->top_address = lowest_byte - 1 + (unsigned long)VM_PAGESIZE;
    process_map[running_pid]->pages.push_back(new_page);

    return (void*)(unsigned long*)lowest_byte;
}

int vm_syslog(void* message, unsigned int len) {
    if ((unsigned long long)message < (unsigned long long)VM_ARENA_BASEADDR ||
        (unsigned long long)message > process_map[running_pid]->top_address - len + 1 ||
        len <= 0) {
        return -1;
    }

    string str;

    for (unsigned int i = 0; i < len; ++i) {
        unsigned int page_number = ((unsigned long long)message + i - (unsigned long)VM_ARENA_BASEADDR) / ((unsigned long)VM_PAGESIZE);
        unsigned int page_offset = ((unsigned long long)message + i - (unsigned long)VM_ARENA_BASEADDR) % ((unsigned long)VM_PAGESIZE);
        unsigned int physical_page = (unsigned int)(process_map[running_pid]->page_table->ptes[page_number].ppage);

        if (process_map[running_pid]->page_table->ptes[page_number].read_enable == 0) {
            if (vm_fault((void*)((unsigned long long)message + i), false) == -1) {
                return -1;
            }
        }

        str = str + ((char*)pm_physmem)[physical_page * (unsigned long)VM_PAGESIZE + page_offset];
    }

    cout << "syslog \t\t\t" << str << endl;
    return 0;
}