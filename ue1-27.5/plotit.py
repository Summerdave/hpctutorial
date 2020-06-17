import matplotlib.pyplot as plt
with open("output.txt", "r") as myfile:
    Ns = []
    flopsarr = []
    lines = myfile.readlines()
    for line in lines:
        values = line.split(" ")
        flops = float(values[2])
        N = int(values[0])
        Ns.append(N)
        flopsarr.append(flops)
    plt.plot(Ns, flopsarr)
    plt.xscale("log")
    plt.show()
