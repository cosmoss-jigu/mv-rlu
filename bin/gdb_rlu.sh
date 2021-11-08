#!/bin/sh

# SPDX-FileCopyrightText: Copyright (c) 2018-2019 Virginia Tech
# SPDX-License-Identifier: Apache-2.0

gdb --arg ../src/bench-mvrlu -b 1 -d 1000 -i 5 -r 50 -u 900 -n 2

