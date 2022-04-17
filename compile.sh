#!/bin/bash

#submit570 2 ./src/pager.cc ./tests/test1.cc ./tests/test2.cc

rm pager
g++ -m32 -I./src ./src/pager.cc lib/libvm_pager.a -o pager

rm test
g++ -m32 -I./tests ./tests/test1.cc lib/libvm_app.a -o test