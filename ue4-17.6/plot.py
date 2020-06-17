from matplotlib import pyplot as plot
import numpy as np
import sys

data = []
if len(sys.argv) > 1:
    data = np.genfromtxt(sys.argv[1], delimiter='\t', names=map(str, range(2)))

plot.ylabel('')
plot.xlabel('')
#plot.xscale('log')
#plot.yscale('log')

plot.plot(data['1'])
plot.show()

