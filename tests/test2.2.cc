#include <iostream>

#include "vm_app.h"

using namespace std;

int main() {
    char* str1 = (char*)vm_extend();
    str1[0] = '0';
    vm_syslog(str1, 1);

    char* str2 = (char*)vm_extend();
    str2[0] = '0';
    vm_syslog(str2, 1);

    char* str3 = (char*)vm_extend();
    str3[0] = '0';
    vm_syslog(str3, 1);
}