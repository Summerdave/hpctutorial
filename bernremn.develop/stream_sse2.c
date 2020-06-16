/* stream_sse2.c
   vector streaming benchmarks - intel SSE2 SIMD extension version
   M. Bernreuther <bernreuther@hlrs.de>

	gcc -msse2 -Wall -g -lm stream_sse2.c -o stream_sse2
	// SSE2 should be available on all recent x86 platforms
	gcc -march=corei7-avx -O3 -ftree-vectorizer-verbose=3 -lm stream_sse2.c -o stream_sse2
	//   march=sandybridge,ivybridge,haswell,...
	// see e.g. https://gcc.gnu.org/onlinedocs/gcc-6.1.0/gcc/x86-Options.html#x86-Options

	icc -Wall -fast -opt-report stream_sse2.c -o stream_sse2_icc
	icc -fast -opt-report -parallel -par-report -vec_report3 -guide-par -guide-vec stream_sse2.c -lm -o stream_sse2.icc
	// -fast = -O3 -ipo -static -xHOST -no-prec-div
	// -xhost: optimize for "this" machine
	// alternatively define explicitly (e.g. on AMD machines) -xSSE2, -xAVX or SSE3, SSSE3, SSE4.1, SSE4.2
	// -opt-streaming-stores always|auto|never


	stream_sse2 [<N>] [<nrepeat>]

https://software.intel.com/sites/landingpage/IntrinsicsGuide/
*/

/* Defaults for command line arguments ===========================================================*/
#define DEFAULT_N 100000
#define DEFAULT_NREPEAT 10
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


/*
#ifdef __MMX__
#include <mmintrin.h>
#endif
 
#ifdef __SSE__
#include <xmmintrin.h>
#endif
 
#ifdef __SSE2__
#include <emmintrin.h>
#endif
 
#ifdef __SSE3__
#include <pmmintrin.h>
#endif
 
#ifdef __SSSE3__
#include <tmmintrin.h>
#endif
 
#if defined (__SSE4__) || defined (__SSE4_1__) || defined (__SSE4_2__) || defined(__SSE4A__)
#define SSE4
#include <smmintrin.h>
#endif

#ifdef __AVX__
#include <immintrin.h>
//#include <fma4intrin.h>
#endif

#if defined(__AVX__) || defined(__SSE4__) || defined(__SSE3__) || defined(__SSE2__)
#include <mm_malloc.h>
#endif
*/


/* SSE2 intrinsics */
#include <emmintrin.h>

#include <mm_malloc.h>	/* mm_malloc, mm_free */


#define MAX(a,b) (((a)>(b))?(a):(b))

typedef unsigned long Tindex;
/*================================================================================================*/
typedef double Tfloat;	/* must not be changed! */
/*================================================================================================*/

const Tindex sizeofTfloat=sizeof(Tfloat);


#define SIMD_WIDTH 2
/*                             8==sizeof(double)==sizeof(Tfloat) */
#define SIMD_ALIGN (SIMD_WIDTH*8)



Tfloat* initvec(Tindex N)
{	/* initialize vector (allocate&touch memory) */
	Tindex arraysize=N*sizeofTfloat;
	Tfloat *v=(Tfloat*)_mm_malloc(arraysize,SIMD_ALIGN);
	if(v)
	{	/* initialize (SSE2 version) */
		/*__m128d vv=_mm_set_pd(0.,0.);*/
		Tindex i;
		for(i=0;i<N;i+=SIMD_WIDTH)
		{
			__m128d vv=_mm_set_pd((Tfloat)(i+1),(Tfloat)i);
			_mm_store_pd(&v[i],vv);
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
/*
  int __builtin_cpu_supports (const char *feature)
  (see e.g. https://gcc.gnu.org/onlinedocs/gcc-6.1.0/gcc/x86-Built-in-Functions.html)
  can be used for the gcc to check for e.g. feature "sse2"s
*/
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
	Tindex datasize, rwsize;
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
		N-=N%SIMD_WIDTH;	/* assuming that N%SIMD_WIDTH==0 */
	}
	if(N<SIMD_WIDTH) N=SIMD_WIDTH;
	if (argc>2)
	{
		nrepeat=labs(atol(argv[2]));
	}
	if(nrepeat<1) nrepeat=1;
	
	rwsize=N*sizeofTfloat*(iternumfloatread+iternumfloatwrite);
	datasize=N*sizeofTfloat*iternumfloat;
	
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
		_mm_free(a);
		exit(1);
	}
	
	__m128d va,vb;
	
#if defined(DO_ADD) || defined(DO_sTRIAD) || defined(DO_vTRIAD)
	Tfloat *c=initvec(N);
	if(!c)
	{
		printf("ERROR allocating memory for c\n");
		_mm_free(b);
		_mm_free(a);
		exit(1);
	}
	__m128d vc;
#endif
#ifdef DO_vTRIAD
	Tfloat *d=initvec(N);
	if(!d)
	{
		printf("ERROR allocating memory for d\n");
#if defined(DO_ADD) || defined(DO_sTRIAD)
		_mm_free(c);
#endif
		_mm_free(b);
		_mm_free(a);
		exit(1);
	}
	__m128d vd;
#endif
#ifdef DO_sTRIAD
	Tfloat s=1.23;
	__m128d vs=_mm_set_pd(s,s);
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
		for(i=0;i<N;i+=SIMD_WIDTH)
		{
			IACA_START
			/*va=vb;*/
			va=_mm_load_pd(&b[i]);
			_mm_store_pd(&a[i],va);
		}
		IACA_END
#endif
#ifdef DO_ADD
		/* a=b+c */
		/*pa=a; pb=b; pc=c;*/
		for(i=0;i<N;i+=SIMD_WIDTH)
		{
			IACA_START
			vb=_mm_load_pd(&b[i]);
			vc=_mm_load_pd(&c[i]);
			va=_mm_add_pd(vb,vc);
			_mm_store_pd(&a[i],va);
		}
		IACA_END
#endif
#ifdef DO_sTRIAD
		/* a=s*b+c */
		/*pa=a; pb=b; pc=c;*/
		for(i=0;i<N;i+=SIMD_WIDTH)
		{
			IACA_START
			vb=_mm_load_pd(&b[i]);
			vc=_mm_load_pd(&c[i]);
			va=_mm_add_pd(_mm_mul_pd(vs,vb),vc);
			_mm_store_pd(&a[i],va);
		}
		IACA_END
#endif
#ifdef DO_vTRIAD
		/* a=b*c+d */
		/*pa=a; pb=b; pc=c; pd=d;*/
		for(i=0;i<N;i+=SIMD_WIDTH)
		{
			IACA_START
			vb=_mm_load_pd(&b[i]);
			vc=_mm_load_pd(&c[i]);
			vd=_mm_load_pd(&d[i]);
			va=_mm_add_pd(_mm_mul_pd(vb,vc),vd);
			_mm_store_pd(&a[i],va);
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
	printf("sse2");
	printf(" 1");	/* #PEs */
	printf("\t%s\t%lu\t%lu\t%u\t%lu\t%zu\t%lu\t%lu",bmtypes,N,nrepeat,1,N,sizeof(Tfloat),datasize,rwsize);
#ifdef USE_CLOCK
	time_diff = (double)(clk_end-clk_start)/CLOCKS_PER_SEC;
	printf("\t\t%g",time_diff);
#endif
#ifdef USE_GETTIMEOFDAY
	time_diff=(double)(tod_end.tv_sec-tod_start.tv_sec)+(double)(tod_end.tv_usec-tod_start.tv_usec)/1.E6;
	printf("\t\t%g",time_diff);
#endif
	time_avg=time_diff/nrepeat;
	time_iter=time_avg/N;
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
	_mm_free(c);
#endif
	_mm_free(b);
	_mm_free(a);
	
	return 0;
}
