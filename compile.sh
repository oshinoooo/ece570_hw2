#!/bin/bash

submit570 1t src/thread.cc tests/*

g++ -m32 -I./src src/thread.cc tests/test1.cc lib/libinterrupt.a -ldl -o test