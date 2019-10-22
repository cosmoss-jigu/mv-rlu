from random import choice
from string import ascii_uppercase

numKeys = 10000000
valueLength = 32
filename = "demo.txt"

f = open(filename, "w+")
for i in range(numKeys):
    rand_string = ''.join(choice(ascii_uppercase) for i in range(valueLength))
    f.write("%d %s\n" % (i+1, rand_string))
f.close()


