#include <iostream>

#include "vm_app.h"

using namespace std;

int main() {
    char* str;
    for(int i; i < 1000; ++i)
        str = (char*)vm_extend();

    str[0] = '0';
    str[1] = '1';
    str[2] = '2';
    str[3] = '3';
    str[4] = '4';

    vm_syslog(str, 5);

    char* str3 = (char*)vm_extend();
    str3[0] = '!';

    char* str4 = (char*)vm_extend();
    str4[0] = '!';
}