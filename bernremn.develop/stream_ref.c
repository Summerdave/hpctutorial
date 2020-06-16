/* stream_ref.c
   vector streaming benchmarks - C reference implementation
   M. Bernreuther <bernreuther@hlrs.de>

	gcc -Wall -g -lm stream_ref.c -o stream_ref
	gcc -march=corei7-avx -O3 -ftree-vectorizer-verbose=3 -lm stream_ref.c -o stream_ref
	//   march=sandybridge,ivybridge,haswell,...

	icc -Wall -fast -opt-report -lm stream_ref.c -o stream_ref.icc
	icc -fast -opt-report -parallel -par-report -vec_report3 -guide-par -guide-vec stream_ref.c -lm -o stream_ref.icc
	// -fast = -O3 -ipo -static -xHOST -no-prec-div
	// -xhost: optimize for "this" machine
	// alternatively define explicitly (e.g. on AMD machines) -xSSE2, -xAVX or SSE3, SSSE3, SSE4.1, SSE4.2
	// -opt-streaming-stores always|auto|never

	pgcc -fast -lm stream_ref.c -o stream_ref.pgi
	craycc -O3 -hfp3 stream_ref.c -o stream_ref.cray


	stream_ref [<N>] [<nrepeat>]

*/

/* Defaults for command line arguments ===========================================================*/
#define DEFAULT_N 100000
#define DEFAULT_NREPEAT 10
#define STRIDE 1
/*================================================================================================*/

/*#define DO_COPY*/
/*#define DO_ADD*/
/*#define DO_sTRIAD*/
#define DO_vTRIAD


#define USE_GETTIMEOFDAY
/*#define USE_CLOCK*/

/*#define USE_IACA*/

/*#define DEBUG*/

/*================================================================================================*/

#include<stdlib.h>	/* malloc() */
#include<stdio.h>	/* printf() */
#include<math.h>	/* labs(), ceil() */

#ifdef USE_CLOCK
#include <time.h>	/* clock() */
#endif

#ifdef USE_GETTIMEOFDAY
#include <sys/time.h>	/* gettimeofday() */
#include <sys/types.h>
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

const Tindex sizeofTfloat=sizeof(Tfloat);



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
			v[i]=(Tfloat)i;
		}
	}
	/*
	else
	{
		printf("initvec: ERROR allocating memory (%ul Bytes)\n",arraysize);
		exit(1);
	}
	*/
	return v;
}


int main(int argc, char *argv[])
{
#ifdef USE_CLOCK
	clock_t clk_start, clk_end;
#endif
#ifdef USE_GETTIMEOFDAY
	struct timeval tod_start,tod_end;
#endif
	double time_diff=0.,time_avg,time_iter;
	char bmtypes[]="----";
	unsigned short iternumfloat=0;
	unsigned short iternumfloatread=0;
	unsigned short iternumfloatwrite=0;
	unsigned short iternumflop=0;
	Tindex datasize, rwsize, Neff;
	double flops, bandwidth;
	Tindex i,j;
	
	Tindex N=DEFAULT_N;
	Tindex nrepeat=DEFAULT_NREPEAT;
	
/* iternumfloatread +1 for write-allocate */
#ifdef DO_COPY
	bmtypes[0]='C';
	iternumfloat=MAX(iternumfloat,2);
	iternumfloatread+=1+1;
	iternumfloatwrite+=1;
#endif
#ifdef DO_ADD
	bmtypes[1]='A';
	iternumfloat=MAX(iternumfloat,3);
	iternumfloatread+=2+1;
	iternumfloatwrite+=1;
	iternumflop+=1;
#endif
#ifdef DO_sTRIAD
	bmtypes[2]='t';
	iternumfloat=MAX(iternumfloat,3);
	iternumfloatread+=2+1;
	iternumfloatwrite+=1;
	iternumflop+=2;
#endif
#ifdef DO_vTRIAD
	bmtypes[3]='T';
	iternumfloat=MAX(iternumfloat,4);
	iternumfloatread+=3+1;
	iternumfloatwrite+=1;
	iternumflop+=2;
#endif
	
	if (argc>1)
	{
		N=labs(atol(argv[1]));
	}
	if(N<1) N=1;
	if (argc>2)
	{
		nrepeat=labs(atol(argv[2]));
	}
	if(nrepeat<1) nrepeat=1;
	
	Neff=(Tindex)(ceil(N/STRIDE));
	rwsize=Neff*sizeofTfloat*(iternumfloatread+iternumfloatwrite);
	datasize=Neff*sizeofTfloat*iternumfloat;
	
	/* initialize vector (allocate&touch memory) */
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
	
#ifdef USE_GETTIMEOFDAY
	gettimeofday( &tod_end, NULL );
#endif
#ifdef USE_CLOCK
	clk_end = clock();
#endif
/*================================================================================================*/
	printf("ref");
	printf(" 1");	/* #PEs */
	printf("\t%s\t%lu\t%lu\t%u\t%lu\t%zu\t%lu\t%lu",bmtypes,N,nrepeat,STRIDE,Neff,sizeof(Tfloat),datasize,rwsize);
#ifdef USE_CLOCK
	time_diff = (double)(clk_end-clk_start)/CLOCKS_PER_SEC;
	printf("\t\t%g",time_diff);
#endif
#ifdef USE_GETTIMEOFDAY
	time_diff=(double)(tod_end.tv_sec-tod_start.tv_sec)+(double)(tod_end.tv_usec-tod_start.tv_usec)/1.E6;
	printf("\t\t%g",time_diff);
#endif
	time_avg=time_diff/nrepeat;
	time_iter=time_avg/Neff;
	printf("\t\t%g\t%g",time_avg,time_iter);
	flops=iternumflop/time_iter;
	bandwidth=rwsize/time_avg;
	printf("\t\t%g\t%g",flops,bandwidth);
	printf("\n");
	
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
