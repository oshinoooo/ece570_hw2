#include <iostream>

#include "vm_app.h"

using namespace std;

int main() {
    char* str1 = (char*)vm_extend();
    char* str2 = (char*)vm_extend();

    str1[0] = '0';
    str1[1] = '1';
    str1[2] = '2';
    str1[3] = '3';
    str1[4] = '4';

    str1[8187] = '0';
    str1[8188] = '1';
    str1[8189] = '2';
    str1[8190] = '3';
    str1[8191] = '4';

    str2[0] = '0';
    str2[1] = '1';
    str2[2] = '2';
    str2[3] = '3';
    str2[4] = '4';

    vm_syslog(str1, 5);
    vm_syslog(str1, 100);
    vm_syslog(str1, 10000);
    vm_syslog(str1, 1000000);
    vm_syslog(str1, 100000000);

    vm_syslog(str2, 5);
    vm_syslog(str2, 100);
    vm_syslog(str2, 10000);
    vm_syslog(str2, 1000000);
    vm_syslog(str2, 100000000);

    vm_syslog(str1 + 8182, 10);
    vm_syslog(str1 + 8183, 10);
    vm_syslog(str1 + 8184, 10);
    vm_syslog(str1 + 8185, 10);
}