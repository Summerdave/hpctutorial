/* stream_ref.c
   vector streaming benchmarks - C reference implementation
   M. Bernreuther <bernreuther@hlrs.de>

	gcc -Wall -g -lm stream_ref.c -o stream_ref.gcc
	gcc -march=native -O3 -ftree-vectorizer-verbose=3 -lm stream_ref.c -o stream_ref.gcc
	//  (march=corei7-avx,sandybridge,ivybridge,haswell,... for older gcc versions)
	//  -mavx -mavx2

	clang -Wall -g -lm stream_ref.c -o stream_ref.clang
	clang -mavx2 -Ofast -lm stream_ref.c -o stream_ref.clang

	icc -Wall -fast -opt-report -lm stream_ref.c -o stream_ref.icc
	icc -fast -opt-report -parallel -par-report -vec_report3 -guide-par -guide-vec stream_ref.c -lm -o stream_ref.icc
	// -fast = -O3 -ipo -static -xHOST -no-prec-div
	// -xhost: optimize for "this" machine
	// alternatively define explicitly (e.g. on AMD machines) -xSSE2, -xAVX or SSE3, SSSE3, SSE4.1, SSE4.2
	// -opt-streaming-stores always|auto|never

	pgcc -fast -lm stream_ref.c -o stream_ref.pgi
	craycc -O3 -hfp3 stream_ref.c -o stream_ref.cray


	//USE_PAPI: add -lpapi


	stream_ref [<N>] [<nrepeat>]

*/

/* Defaults for command line arguments ===========================================================*/
#define DEFAULT_N 100000
#define DEFAULT_NREPEAT 1
#define STRIDE 1
/*================================================================================================*/

/*#define DO_COPY*/
/*#define DO_ADD*/
/*#define DO_sTRIAD*/
#define DO_vTRIAD


/*#define USE_CLOCK*/
#define USE_GETTIMEOFDAY
/*#define USE_CLOCKGETTIME*/

/*#define USE_PAPI*/
/*#define USE_IACA*/

/*#define DEBUG*/

/*================================================================================================*/

#include<stdlib.h>	/* malloc(),free(),labs(), atol() */
#include<stdio.h>	/* printf() */
#include<math.h>	/* ceil() */
#include<string.h>	/* strcmp() */

#if defined(USE_CLOCK) || defined(USE_CLOCKGETTIME)
#include <time.h>	/* clock(),clock_gettime(),clock_getres() */
#endif

#ifdef USE_GETTIMEOFDAY
#include <sys/time.h>	/* gettimeofday() */
#endif

#ifdef USE_PAPI
#include <papi.h>
#endif

#ifdef USE_IACA
#include <iacaMarks.h>
#else
#define IACA_START
#define IACA_END
#endif

#define MAX(a,b) (((a)>(b))?(a):(b))

typedef unsigned long Tindex;
/*================================================================================================*/
typedef double Tfloat;
/*================================================================================================*/

const size_t sizeofTfloat=sizeof(Tfloat);



Tfloat* initvec(Tindex N)
{	/* initialize vector (allocate&touch memory) */
	Tindex arraysize=N*sizeofTfloat;
	Tfloat *v=(Tfloat*)malloc(arraysize);
	/*Tfloat *restrict v=(Tfloat*)malloc(arraysize);*/
	if(v)
	{	/* initialize */
		Tindex i;
		for(i=0;i<N;i+=STRIDE)
		{
			v[i]=(Tfloat)i;	/* first touch */
		}
	}
	else
	{
		printf("initvec: ERROR allocating memory (%lu Bytes)\n",arraysize);
		exit(1);
	}
	return v;
}


int main(int argc, char *argv[])
{
	double time_res=0;
#ifdef USE_CLOCK
	clock_t clk_start, clk_end;
#endif
#ifdef USE_GETTIMEOFDAY
	struct timeval tod_start,tod_end;
#endif
#ifdef USE_CLOCKGETTIME
	clockid_t clkt_id=CLOCK_MONOTONIC;	/* CLOCK_REALTIME,CLOCK_MONOTONIC,CLOCK_PROCESS_CPUTIME_ID,CLOCK_THREAD_CPUTIME_ID */
	struct timespec clkt_start,clkt_end,clkt_res;
	clock_getres(clkt_id,&clkt_res);
	time_res=clkt_res.tv_sec+(double)clkt_res.tv_nsec/1.E9;
#endif
	double time_diff=0.,time_avg,time_iter;
	char bmtypes[]="----";
	unsigned short iternumfloat=0;
	unsigned short iternumfloatread=0;
	unsigned short iternumfloatwrite=0;
	unsigned short iternumfloatrfo=0;	/* Read For Ownership (write-back allocate for inclusive L1<->L2<->L3) */
	unsigned short iternumflop=0;
	Tindex datasize, Neff;
	double flops;
	double bandwidth;
	Tindex i,j;
#ifdef USE_PAPI
	int papi_retval;
	float papi_realtime, papi_proctime, papi_mflops;
	long long papi_flpins;
#endif
	
	Tindex N=DEFAULT_N;
	Tindex nrepeat=DEFAULT_NREPEAT;
	
#ifdef DO_COPY
	bmtypes[0]='C';
	iternumfloat=MAX(iternumfloat,2);
	iternumfloatread+=1;
	iternumfloatwrite+=1;
	iternumfloatrfo+=1;
#endif
#ifdef DO_ADD
	bmtypes[1]='A';
	iternumfloat=MAX(iternumfloat,3);
	iternumfloatread+=2;
	iternumfloatwrite+=1;
	iternumfloatrfo+=1;
	iternumflop+=1;
#endif
#ifdef DO_sTRIAD
	bmtypes[2]='t';
	iternumfloat=MAX(iternumfloat,3);
	iternumfloatread+=2;
	iternumfloatwrite+=1;
	iternumfloatrfo+=1;
	iternumflop+=2;
#endif
#ifdef DO_vTRIAD
	bmtypes[3]='T';
	iternumfloat=MAX(iternumfloat,4);
	iternumfloatread+=3;
	iternumfloatwrite+=1;
	iternumfloatrfo+=1;
	iternumflop+=2;
#endif
	
	if (argc>1)
	{
		if (!strcmp(argv[1],"-h") || !strcmp(argv[1],"--help"))
		{
			printf("usage: %s [<N>] [<nrepeat>]\n",argv[0]);
			exit(0);
		}
		N=labs(atol(argv[1]));
	}
	if(N<1) N=1;
	if (argc>2)
	{
		nrepeat=labs(atol(argv[2]));
	}
	if(nrepeat<1) nrepeat=1;
	
	Neff=(Tindex)(ceil(N/STRIDE));
	datasize=Neff*sizeofTfloat*iternumfloat;
	
	/* initialize vector (allocate&touch memory) */
	/* C99: might use "restrict" for a,b,c */
	Tfloat *a=initvec(N);
	if(!a)
	{
		printf("ERROR allocating memory for a\n");
		exit(1);
	}
	
	Tfloat *b=initvec(N);
	if(!b)
	{
		printf("ERROR allocating memory for b\n");
		free(a);
		exit(1);
	}
	
#if defined(DO_ADD) || defined(DO_sTRIAD) || defined(DO_vTRIAD)
	Tfloat *c=initvec(N);
	if(!c)
	{
		printf("ERROR allocating memory for c\n");
		free(b);
		free(a);
		exit(1);
	}
#endif
#ifdef DO_vTRIAD
	Tfloat *d=initvec(N);
	if(!d)
	{
		printf("ERROR allocating memory for d\n");
#if defined(DO_ADD) || defined(DO_sTRIAD)
		free(c);
#endif
		free(b);
		free(a);
		exit(1);
	}
#endif
#ifdef DO_sTRIAD
	Tfloat s=1.23;
#endif
	
/*================================================================================================*/
#ifdef USE_CLOCK
	clk_start = clock();
#endif
#ifdef USE_GETTIMEOFDAY
	gettimeofday( &tod_start, NULL );
#endif
#ifdef USE_CLOCKGETTIME
	clock_gettime(clkt_id,&clkt_start);
#endif
#ifdef USE_PAPI
	papi_retval=PAPI_flops(&papi_realtime,&papi_proctime,&papi_flpins,&papi_mflops);
#endif
	
	for(j=0;j<nrepeat;++j)
	{
#ifdef DO_COPY
		/* a=b */
		/*pa=a; pb=b;*/
		for(i=0;i<N;i+=STRIDE)
		{
			IACA_START
			a[i]=b[i];
			/* *pa=*pb;  pa+=STRIDE; pb+=STRIDE; */
		}
		IACA_END
#endif
#ifdef DO_ADD
		/* a=b+c */
		/*pa=a; pb=b; pc=c;*/
		for(i=0;i<N;i+=STRIDE)
		{
			IACA_START
			a[i]=b[i]+c[i];
			/* *pa=*pb+ *pc;  pa+=STRIDE; pb+=STRIDE; pc+=STRIDE; */
		}
		IACA_END
#endif
#ifdef DO_sTRIAD
		/* a=s*b+c */
		/*pa=a; pb=b; pc=c;*/
		for(i=0;i<N;i+=STRIDE)
		{
			IACA_START
			a[i]=s*b[i]+c[i];
			/* *pa=s* *pb+ *pc;  pa+=STRIDE; pb+=STRIDE; pc+=STRIDE; */
		}
		IACA_END
#endif
#ifdef DO_vTRIAD
		/* a=b*c+d */
		/*pa=a; pb=b; pc=c; pd=d;*/
		for(i=0;i<N;i+=STRIDE)
		{
			IACA_START
			a[i]=b[i]*c[i]+d[i];
			/* *pa=*pb* *pc+ *pd;  pa+=STRIDE; pb+=STRIDE; pc+=STRIDE; pd+=STRIDE; */
		}
		IACA_END
#endif
	} /* for-loop 0<=j<nrepeat */
	
#ifdef USE_PAPI
	papi_retval=PAPI_flops(&papi_realtime,&papi_proctime,&papi_flpins,&papi_mflops);
#endif
#ifdef USE_CLOCKGETTIME
	clock_gettime(clkt_id,&clkt_end);
#endif
#ifdef USE_GETTIMEOFDAY
	gettimeofday( &tod_end, NULL );
#endif
#ifdef USE_CLOCK
	clk_end = clock();
#endif
/*================================================================================================*/
	printf("ref");
	printf(" 1");	/* #PEs */
	printf("\t%s\t%lu\t%lu\t%u\t%lu\t%zu\t%lu",bmtypes,N,nrepeat,STRIDE,Neff
	                                          ,sizeofTfloat,datasize);
#ifdef USE_CLOCK
	time_diff = (double)(clk_end-clk_start)/CLOCKS_PER_SEC;
	printf("\t\t%g",time_diff);
#endif
#ifdef USE_GETTIMEOFDAY
	time_diff=(double)(tod_end.tv_sec-tod_start.tv_sec)+(double)(tod_end.tv_usec-tod_start.tv_usec)/1.E6;
	printf("\t\t%g",time_diff);
#endif
#ifdef USE_CLOCKGETTIME
	time_diff=(double)(clkt_end.tv_sec-clkt_start.tv_sec)+(double)(clkt_end.tv_nsec-clkt_start.tv_nsec)/1.E9;
	printf("\t\t%g",time_diff);
#endif
	time_avg=time_diff/nrepeat;
	time_iter=time_avg/Neff;
	printf("\t\t%g\t%g",time_avg,time_iter);
	flops=iternumflop/time_iter;
	bandwidth=datasize/time_avg;
	printf("\t\t%u\t%g",iternumflop,flops);
	printf("\t%g",bandwidth);
	printf("\n");
#ifdef USE_PAPI
	printf("#PAPI\tReal_time: %f\tProc_time: %f\tTotal flpins: %lld\tMFLOPS: %f\n",papi_realtime, papi_proctime, papi_flpins, papi_mflops);
	PAPI_shutdown();
#endif
	
#ifdef DEBUG
	/* check (last) results (for DEBUGGING purposes) */
	Tfloat error,maxerror=0;
	for(i=0;i<N;i+=STRIDE)
	{
#ifdef DO_COPY
		error=a[i]-b[i];
#endif
#ifdef DO_ADD
		error=a[i]-(b[i]+c[i]);
#endif
#ifdef DO_sTRIAD
		error=a[i]-(s*b[i]+c[i]);
#endif
#ifdef DO_vTRIAD
		error=a[i]-(b[i]*c[i]+d[i]);
#endif
		/*printf("%lu\t%g\t%g\n",i,a[i],error);*/
		error=fabs(error);
		if(error>maxerror) maxerror=error;
	}
	printf("maxerror=%g\n",maxerror);
#endif
	
	/* free memory*/	
#ifdef DO_vTRIAD
	free(d);
#endif
#if defined(DO_ADD) || defined(DO_sTRIAD) || defined(DO_vTRIAD)
	free(c);
#endif
	free(b);
	free(a);
	
	return 0;
}
