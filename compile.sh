#!/bin/bash

submit570 1t src/thread.cc tests/*

g++ -m32 -I./src ./src/pager.cc lib/libvm_app.a lib/libvm_pager.a -ldl -o pager