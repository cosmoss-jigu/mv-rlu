#!/bin/sh

gdb --arg ../src/bench-mvrlu -b 1 -d 1000 -i 5 -r 50 -u 900 -n 2

