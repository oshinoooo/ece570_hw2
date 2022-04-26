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

    vm_syslog(str1, 5);
    vm_syslog(str2, 5);
    vm_syslog(str3, 5);
    vm_syslog(str4, 5);
    vm_syslog(str5, 5);
    vm_syslog(str6, 5);
}