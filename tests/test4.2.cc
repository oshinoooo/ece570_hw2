#include <iostream>

#include "vm_app.h"

using namespace std;

int main() {
    vm_extend();
    vm_extend();

    char* str1 = (char*)vm_extend();
    str1[3] = '3';
    vm_syslog(str1, 5);

    char* str2 = (char*)vm_extend();
    str2[3] = '3';
    vm_syslog(str2, 5);

    char* str3 = (char*)vm_extend();
    str3[3] = '3';
    vm_syslog(str3, 5);

    str1[8192] = '?';
}