#include <iostream>

#include "vm_app.h"

using namespace std;

int main() {
    cout << "--------------------" << endl;
    char* str = (char*)vm_extend();
    for(int i = 0; i < 0x2000; ++i){
        if(str[i] != '\0'){
            cout << "fault" << endl;
            break;
        }
    }
    cout << "--------------------" << endl;
    return 0;
}