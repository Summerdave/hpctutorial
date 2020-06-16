/* streamlist.c
   vector streaming benchmarks - C reference implementation
   M. Bernreuther <bernreuther@hlrs.de>

	gcc -Wall -g -lm streamlist.c -o streamlist
	gcc -march=corei7-avx -O3 -ftree-vectorizer-verbose=3 -lm streamlist.c -o streamlist
	//   march=sandybridge,ivybridge,haswell,...

	icc -Wall -fast -opt-report -lm streamlist.c -o streamlist.icc
	icc -fast -opt-report -parallel -par-report -vec_report3 -guide-par -guide-vec streamlist.c -lm -o streamlist.icc
	// -fast = -O3 -ipo -static -xHOST -no-prec-div
	// -xhost: optimize for "this" machine
	// alternatively define explicitly (e.g. on AMD machines) -xSSE2, -xAVX or SSE3, SSSE3, SSE4.1, SSE4.2
	// -opt-streaming-stores always|auto|never

	pgcc -fast -lm streamlist.c -o streamlist.pgi
	craycc -O3 -hfp3 streamlist.c -o streamlist.cray


	streamlist [<N>] [<nrepeat>]

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

/*            extra payload at the end of a list link node [bytes] */
#define LIST_NEXTRA	0

#define USE_GETTIMEOFDAY
/*#define USE_CLOCK*/

/*#define USE_IACA*/

#define DEBUG

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


struct listnode {
	struct listnode	*next;
	Tfloat	data;
#if (LIST_NEXTRA>=1)
	char	extra[LIST_NEXTRA];
#endif
};

const Tindex sizeoflistnode=sizeof(struct listnode);


struct listnode* initvec(Tindex N)
{	/* initialize vector (allocate&touch memory) */
	Tindex arraysize=N*sizeoflistnode;
	struct listnode *l=(struct listnode*)malloc(arraysize);
	if(l)
	{	/* initialize */
		Tindex i;
		for(i=0;i<N;i+=STRIDE)
		{
			l[i].next=&l[i+1];
			l[i].data=(Tfloat)i;
		}
		l[i-STRIDE].next=NULL;
	}
	/*
	else
	{
		printf("initvec: ERROR allocating memory\n");
		exit(1);
	}
	*/
	return l;
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
	struct listnode *a=initvec(N);
	if(!a)
	{
		printf("ERROR allocating memory for a\n");
		exit(1);
	}
	struct listnode *pa=a;
	
	struct listnode *b=initvec(N);
	if(!b)
	{
		printf("ERROR allocating memory for b\n");
		free(a);
		exit(1);
	}
	struct listnode *pb=b;
	
#if defined(DO_ADD) || defined(DO_sTRIAD) || defined(DO_vTRIAD)
	struct listnode *c=initvec(N);
	if(!c)
	{
		printf("ERROR allocating memory for c\n");
		free(b);
		free(a);
		exit(1);
	}
	struct listnode *pc=c;
#endif
#ifdef DO_vTRIAD
	struct listnode *d=initvec(N);
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
	struct listnode *pd=d;
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
		pa=a;
		pb=b;
		while(pa)
		{
			IACA_START
			pa->data=pb->data;
			pa=pa->next;
			pb=pb->next;
		}
		IACA_END
#endif
#ifdef DO_ADD
		/* a=b+c */
		pa=a;
		pb=b;
		pc=c;
		while(pa)
		{
			IACA_START
			pa->data=pb->data+pc->data;
			pa=pa->next;
			pb=pb->next;
			pc=pc->next;
		}
		IACA_END
#endif
#ifdef DO_sTRIAD
		/* a=b+s*c */
		pa=a;
		pb=b;
		pc=c;
		while(pa)
		{
			IACA_START
			pa->data=s*pb->data+pc->data; 
			pa=pa->next;
			pb=pb->next;
			pc=pc->next;
		}
		IACA_END
#endif
#ifdef DO_vTRIAD
		/* a=b+c*d */
		pa=a;
		pb=b;
		pc=c;
		pd=d;
		while(pa)
		{
			IACA_START
			pa->data=pb->data*pc->data+pd->data;
			pa=pa->next;
			pb=pb->next;
			pc=pc->next;
			pd=pd->next;
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
	pa=a;
	pb=b;
#if defined(DO_ADD) || defined(DO_sTRIAD) || defined(DO_vTRIAD)
	pc=c;
#endif
#ifdef DO_vTRIAD
	pd=d;
#endif
	while(pa)
	{
#ifdef DO_COPY
		error=pa->data-pb->data;
#endif
#ifdef DO_ADD
		error=pa->data-(pb->data+pc->data);
#endif
#ifdef DO_sTRIAD
		error=pa->data-(s*pb->data+pc->data);
#endif
#ifdef DO_vTRIAD
		error=pa->data-(pb->data*pc->data+pd->data);
#endif
		/*printf("%lu\t%g\t%g\n",i,a[i],error);*/
		error=fabs(error);
		if(error>maxerror) maxerror=error;
		pa=pa->next;
		pb=pb->next;
#if defined(DO_ADD) || defined(DO_sTRIAD) || defined(DO_vTRIAD)
		pc=pc->next;
#endif
#ifdef DO_vTRIAD
		pd=pd->next;
#endif
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
