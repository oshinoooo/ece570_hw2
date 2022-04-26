#!/bin/bash

submit570 2 ./src/pager.cc ./tests/*

g++ -m32 -I./src ./cs310/pager.cpp lib/libvm_pager.a -o pager
g++ -m32 -I./src ./src/pager.cc lib/libvm_pager.a -o pager
g++ -m32 -I./tests ./tests/test1.2.cc lib/libvm_app.a -o test