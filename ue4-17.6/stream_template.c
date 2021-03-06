/* stream_template.c
   Schönauer vector triade
   Tutorial for the High Performance Computing lecture SS20
   M. Bernreuther <bernreuther@hlrs.de>

  compile with:
	gcc -Wall -g -lm stream_template.c -o stream
	gcc -march=native -O3 -ftree-vectorizer-verbose=3 -lm stream_template.c -o stream
	//  -mavx -mavx2

	clang -Wall -g -lm stream_template.c -o stream
	clang -mavx2 -Ofast -lm stream_template.c -o stream

	icc -Wall -fast -opt-report -lm stream_ref.c -o stream
	icc -fast -opt-report -parallel -par-report -vec_report3 -guide-par -guide-vec stream_ref.c -lm -o stream

  run with:
	stream [<n>] [<nrepeat>]

*/

#include<stdlib.h>	/* calloc(),free(),labs(), atol() */
#include<stdio.h>	/* printf() */

#include <time.h>       /* clock_gettime(),clock_getres() */
#include <mm_malloc.h>
#include <immintrin.h>
#include <pthread.h>

#ifdef USE_AVX
#define free _mm_free
#define SIMD_WIDTH 4
#endif

double* initvec(unsigned long N)
{       /* initialize vector (allocate&touch memory) */
#ifndef USE_AVX
	double *v=(double*)calloc(N,sizeof(double));	/* malloc */
#else
	double *v = (double*) _mm_malloc(N*sizeof(double), SIMD_WIDTH*sizeof(double));
#endif
	if(v)
	{       /* initialize */
		unsigned long i;
		for(i=0;i<N;++i) v[i]=(double)i;	/* first touch */
	}
	else
	{
		printf("initvec: ERROR allocating memory (%lu*%zu=%lu Bytes)\n",N,sizeof(double),N*sizeof(double));
		exit(1);
	}
	return v;
}

typedef struct {
	unsigned int p, numthreads;
	unsigned int N;
	double *a, *b, *c, *d;
} t_thread_work;

void thread_stream(t_thread_work* w) {

	int num_threads = w->numthreads;
	int p = w->p;
	int N = w->N;
#ifdef BLOCKED 
	int i1 = N * p / num_threads;
	int i2 = N * (p + 1) / num_threads;
	for(int i = i1; i < i2; ++i)
		w->a[i] = w->b[i] * w->c[i] + w->d[i];
#else
	for(int i = p; i < N; i += num_threads)
		w->a[i] = w->b[i] * w->c[i] + w->d[i];
#endif
}

int main(int argc, char *argv[])

{


	unsigned long n=1000;
	if (argc>1) n=labs(atol(argv[1]));
	
	unsigned long nrepeat=1;
	/*if (argc>2) nrepeat=labs(atol(argv[2]));*/
	
	
	double *a=initvec(n);
	double *b=initvec(n);
	double *c=initvec(n);
	double *d=initvec(n);
	
	unsigned long i,j;
	clockid_t clkt_id=CLOCK_MONOTONIC;
	struct timespec clkt_start,clkt_end,clkt_res;
	/*================================================================================================*/
	clock_gettime(clkt_id,&clkt_start);


	const int THREADS = 6;
	t_thread_work threads[THREADS];
	for(int p = 0; p < THREADS; ++p)
	{
		threads[p].N = n;
		threads[p].p = p;
		threads[p].numthreads = THREADS;
		threads[p].a = a;
		threads[p].b = b;
		threads[p].c = c;
		threads[p].d = d;
	}

	pthread_t workers[THREADS];

	for(int p = 0; p < THREADS; ++p)
		pthread_create(&workers[p], NULL, (void*(*)(void*))&thread_stream, (void*)&(threads[p]));
	for(int p = 0; p < THREADS; ++p)
		pthread_join(workers[p], NULL);
/*
#ifndef USE_AVX
		for(i=0;i<n;++i)
		{
			a[i]=b[i]*c[i]+d[i];
		}
		
#else
		for (i = 0; i < n; i += SIMD_WIDTH) {
			__m256d vb = _mm256_load_pd(&b[i]);
			__m256d vc = _mm256_load_pd(&c[i]);
			__m256d vd = _mm256_load_pd(&d[i]);
			__m256d va=_mm256_fmadd_pd(vb,vc,vd);
			_mm256_store_pd(&a[i], va);
		}

#endif
*/	
	clock_gettime(clkt_id,&clkt_end);
	/*================================================================================================*/
	double time_diff=(double)(clkt_end.tv_sec-clkt_start.tv_sec)+(double)(clkt_end.tv_nsec-clkt_start.tv_nsec)/1.E9;
	
	printf("%lu %lu\t%g\n",n,nrepeat,time_diff);
	
	free(d);
	free(c);
	free(b);
	free(a);
	return 0;
}
