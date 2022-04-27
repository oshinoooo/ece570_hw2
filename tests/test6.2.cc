#include <iostream>

#include "vm_app.h"

using namespace std;

int main() {
    char* str1 = (char*)vm_extend();
    char* str2 = (char*)vm_extend();
    char* str3 = (char*)vm_extend();
    char* str4 = (char*)vm_extend();
    char* str5 = (char*)vm_extend();
    char* str6 = (char*)vm_extend();

    str1[0] = '0';
    str2[0] = '0';
    str3[0] = '0';
    str4[0] = '0';
    str5[0] = '0';
    str6[0] = '0';

    vm_yield();

    for (int i = 0; i < 100; ++i) {
        vm_syslog(str1, i);
        vm_syslog(str2, i);
        vm_yield();
        vm_syslog(str3, i);
        vm_syslog(str4, i);
        vm_yield();
        vm_syslog(str5, i);
        vm_syslog(str6, i);
    }

    vm_yield();
}