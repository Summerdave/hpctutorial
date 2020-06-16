/* stream_template.c
   Schönauer vector triade
   Tutorial for the High Performance Computing lecture SS20
   M. Bernreuther <bernreuther@hlrs.de>

  compile with:
        gcc -Wall -g -lm stream_template.c -o stream
        gcc -march=native -O3 -ftree-vectorizer-verbose=3 -lm stream_template.c
  -o stream
        //  -mavx -mavx2

        clang -Wall -g -lm stream_template.c -o stream
        clang -mavx2 -Ofast -lm stream_template.c -o stream

        icc -Wall -fast -opt-report -lm stream_ref.c -o stream
        icc -fast -opt-report -parallel -par-report -vec_report3 -guide-par
  -guide-vec stream_ref.c -lm -o stream

  run with:
        stream [<n>] [<nrepeat>]

*/

#include <stdio.h>  /* printf() */
#include <stdlib.h> /* calloc(),free(),labs(), atol() */

#include <time.h> /* clock_gettime(),clock_getres() */

double *
initvec(unsigned long N) { /* initialize vector (allocate&touch memory) */
  double *v = (double *)calloc(N, sizeof(double)); /* malloc */
  if (v) {                                         /* initialize */
    unsigned long i;
    for (i = 0; i < N; ++i)
      v[i] = (double)i; /* first touch */
  } else {
    printf("initvec: ERROR allocating memory (%lu*%zu=%lu Bytes)\n", N,
           sizeof(double), N * sizeof(double));
    exit(1);
  }
  return v;
}

int main(int argc, char *argv[]) {
  unsigned long n = 1000;
  if (argc > 1)
    n = labs(atol(argv[1]));

  unsigned long nrepeat = 1;
  /*if (argc>2) nrepeat=labs(atol(argv[2]));*/

  double *a = initvec(n);
  double *b = initvec(n);
  double *c = initvec(n);
  double *d = initvec(n);

  unsigned long i, j;
  clockid_t clkt_id = CLOCK_MONOTONIC;
  struct timespec clkt_start, clkt_end, clkt_res;
  /*================================================================================================*/
  clock_gettime(clkt_id, &clkt_start);

  /*for(j=0;j<nrepeat;++j)*/
  for (i = 0; i < n; ++i) {
    a[i] = b[i] * c[i] + d[i];
  }

  clock_gettime(clkt_id, &clkt_end);
  /*================================================================================================*/
  double time_diff = (double)(clkt_end.tv_sec - clkt_start.tv_sec) +
                     (double)(clkt_end.tv_nsec - clkt_start.tv_nsec) / 1.E9;

  printf("%lu %lu\t%g\n", n, nrepeat, time_diff);

  free(d);
  free(c);
  free(b);
  free(a);
  return 0;
}
