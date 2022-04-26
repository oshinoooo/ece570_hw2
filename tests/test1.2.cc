#include <iostream>

#include "vm_app.h"

using namespace std;

int main() {
    char* str = (char*)vm_extend();
    str[0] = '0';
    str[1] = '1';
    str[2] = '2';
    str[3] = '3';
    str[4] = '4';

    vm_syslog(str, 0);
    vm_syslog(str, 5);
    vm_syslog(str, 8192);
    vm_syslog(str, 8193);
}