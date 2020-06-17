#!/bin/sh
# run_stream.sh
# start stream benchmarks
# M. Bernreuther <bernreuther@hlrs.de>

d="`dirname $0`"
b="`basename $0 | sed -e 's/^run_//' -e 's/\..*$//'`"
if [ -n "$1" ]; then
	b="${b}_$1";
else
	b="`ls -t $(find . -maxdepth 1 -type f -executable -name "${b}_*" ) | head -n 1 | sed -e 's#^./##'`"
fi
if [ ! -x "$b" ]; then
	echo "ERROR: $b not executable"
	exit 1
fi

echo    "# ${b}.dat"
#HOSTNAME=`hostname`
#os_release="`lsb_release -d | sed -e 's/\S*\s*//'`"
echo    "# running $b on ${HOSTNAME} (`grep -m 1 'model name' /proc/cpuinfo | sed -e 's/.*:\s*//'`) starting at `date +'%d.%m.%Y %H:%M:%S'`"
if [ `$d/$b 1 1 | wc -w` -eq 15 ]; then
	echo -e "# bintype #PE\tbmtype\tN\tnrepeat\tstride\tNeff\tscalarsize\tDatasize\tIOsize\toverall_time\ttime_avg\ttime_iter\tFLOPs\tBandwidth"
else
	echo -e "# bintype #PE\tbmtype\tN\tnrepeat\tscalarsize\tDatasize\tIOsize\toverall_time\ttime_avg\ttime_iter\tFLOPs\tBandwidth"
fi
trailing0=''
for e in `seq 7`; do
	for m in `seq 9`; do
		n="$m${trailing0}"
		rep=$(( 1000000000/1${trailing0} ))
		$d/$b $n $rep
	done
	trailing0="${trailing0}0"
done
echo "# finished at `date +'%d.%m.%Y %H:%M:%S'`"
