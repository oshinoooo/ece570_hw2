#include <iostream>

#include "vm_app.h"

using namespace std;

int main() {
    char* strs[10];
    for (int i = 0; i < 10; ++i) {
        strs[i] = (char*)vm_extend();
        strs[i][0] = i;
        vm_syslog(strs[i], 1);
    }

    strs[3][0] = '?';
    strs[3][1] = '?';
    strs[3][2] = '?';
    strs[3][3] = '?';
    strs[3][4] = '?';

    char* str1 = (char*)vm_extend();
    str1[0] = '!';

    char* str2 = (char*)vm_extend();
    str2[0] = '!';

    for(int i = 0; i < 10; ++i)
        vm_syslog(strs[i], 1);

    char* str3 = (char*)vm_extend();
    str3[0] = '!';

    char* str4 = (char*)vm_extend();
    str4[0] = '!';

    char* str5 = (char*)vm_extend();
    str5[0] = '!';

    return 0;
}