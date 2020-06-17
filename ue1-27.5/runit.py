import os

for base in [1000, 10000, 100000, 1000000]:
    for j in range(1, 10):
        i = base * j
        os.system("./a.out "+ str(i))
