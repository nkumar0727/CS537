import sys
import string
import random

f_small = "f_small.txt"
f_med = "f_med.txt"
f_large = "f_large.txt"

MAX_BUF = 128
MAX_LINE_T = 5
MAX_LINE_S = 500 
MAX_LINE_M = 17000 
MAX_LINE_L = 1600000

def gen_test_file(size, filename):
    f = open(filename, "w+")
    c = 0
    line_sz = 0
    while c < size: 
        line_sz = random.randint(0, MAX_BUF)
        st = ""
        c2 = 0
        while c2 < line_sz:
            sp = random.randint(0,4)
            if sp == 0:
                st += " "
            else:
                st += random.choice(string.ascii_lowercase)
            c2 += 1
        f.write(st)		
        f.write("\n")
        c += 1
    f.close()

def is_sorted(filename):
    sort = True 
    f = open(filename, "r")
    base = ""
    for line in iter(f):
        if base > line:
            sort = False
            break
    base = str(line)
    f.close()
    return sort

def main(argv):
    #gen_test_file(MAX_LINE_S, f_small)
    gen_test_file(MAX_LINE_M, f_med)
    #gen_test_file(MAX_LINE_L, f_large)

if __name__ == "__main__":
    main(sys.argv)
