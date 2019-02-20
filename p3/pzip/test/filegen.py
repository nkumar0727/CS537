#! /usr/bin/env python3

import random
import string

max = 1000

for i in range(max):
    x = ''
    letter = random.choice(string.ascii_lowercase + '\n')
    if letter == '\n':
        x += letter
    else:
        for i in range(int(random.random() * 20) + 1):
            x += letter
    print(x, end='')

