#!/bin/bash
# run.sh
# start stream benchmarks, e.g. with
#  ./run.sh stream_ref |tee stream_ref.dat
# runtime on visgs about 1min
#
# M. Bernreuther <bernreuther@hlrs.de>

#cpumask=
cpumask='0x1'

rep=25

#d="`dirname $0`"
##d="$(readlink -f "$d")"
#cd $d
scriptname="`basename $0`"

if [ -n "$1" ]; then
	#b="${b}_$1";
	b="$1";
else
	#b="`ls -t $(find . -maxdepth 1 -type f -executable -name "${b}_*" ) | head -n 1 | sed -e 's#^./##'`"
	b="$(find . -maxdepth 1 -type f -executable | sed -e "s/${scriptname}//" -e 's#^\./##')"
	if [ -n "$b" ]; then
		b="`ls -t $b | head -n 1`"
	fi
fi
b="$(readlink -f "$b")"
if [ ! -x "$b" ]; then
	echo "ERROR: no executable $b"
	exit 1
fi

echo    "# ${b}.dat"
#HOSTNAME=`hostname`
#os_release="`lsb_release -d | sed -e 's/\S*\s*//'`"
echo    "# $b (`stat -c "size: %s mod.:%y" "$b"`)"
echo    "# running $b on ${HOSTNAME} (`grep -m 1 'model name' /proc/cpuinfo | sed -e 's/.*:\s*//' -e 's/(R)//' -e 's/(TM)//'`)"
starttime=`date +%s`
echo    "# starting at `date -d @${starttime} +'%d.%m.%Y %H:%M:%S'`"
echo -e "# #PE (repetitions)\truntime(median,mean,min,max)"
trailing0=''
for e in `seq 7`; do
	for m in `seq 9`; do
		n="$m${trailing0}"
		#rep=$(( 100000000/1${trailing0} ))
		runtimes=""
		for i in `seq ${rep}`; do
			if [ -n "${cpumask}" ]; then
				#bout="$(taskset ${cpumask} $b $n $rep)"
				bout="$(taskset ${cpumask} $b $n)"
			else
				#bout="$($b $n $rep)"
				bout="$($b $n)"
			fi
			#echo "${bout}"
			runtime="$(echo "${bout}" | awk '{print $3}')"
			runtimes="${runtimes# } ${runtime}"
		done
		rtmedavgminmax="$(echo "${runtimes}" | Rscript -e 'print(summary(scan("stdin")));' 2>/dev/null | tail -n 1 | awk '{print $3,$4,$1,$6}')"
		runtime="$(echo "${rtmedavgminmax}" | awk '{print $1}')"
		flops="$(echo "${runtime}" | sed -e "s/[eE]+*/*10^/;s#^#$n*2/(#;s/$/)/" | bc -l )"
		echo -e "$n (${rep})\t${rtmedavgminmax}\t${flops}"
	done
	trailing0="${trailing0}0"
done
endtime=`date +%s`
echo "# finished at `date -d @${endtime} +'%d.%m.%Y %H:%M:%S'`"
difftime=$(( ${endtime}-${starttime} ))
printf "# running for %d sec (%d:%02d:%02d)\n" ${difftime} $((${difftime}/3600)) $((${difftime}/60 % 60)) $((${difftime} % 60))
#gnuplot -e "datfile_ref='stream_ref.dat'" stream.gplt
