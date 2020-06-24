/* mandelbrot_seq.c
   calculate Mandelbrot set  (initial sequential version)
   M. Bernreuther <bernreuther@hlrs.de>

	gcc -Wall -g mandelbrot_seq.c -o mandelbrot_seq
	gcc -march=corei7-avx -O3 -ftree-vectorizer-verbose=3 mandelbrot_seq.c -o mandelbrot_seq

	try also e.g.
	$ ./mandelbrot_seq 640 480  127 -2 -0.9375 0.5 0.9375 mandelbrot1.pgm
	$ ./mandelbrot_seq 1024 768  127 -2 -1 0.6666667 1 mandelbrot2.pgm
	$ ./mandelbrot_seq 1920 1080 255 -2 -1 1.5555556 1 mandelbrot3.pgm
	$ ./mandelbrot_seq 1280 960  65535 0.25 -0.12 0.41 0 mandelbrot4.pgm


	colorize PGM e.g. with Gimp (https://www.gimp.org/)
	$ gimp mandelbrot.pgm
	check & improve generated grayscale image:
	- check Colors > Info > Histogram
	- adjust Colors > Levels
	transform greyscale to color image:
	- Image > Mode > RGB
	several options to colorize:
	- Colors > Map > Alien Map
	- Toolbox | Blend tool (L) (Tools > Paint Tools > Blend (L)) to select active Gradient, e.g. Skyline
	  Colors > Map > Gradient Map
	gimp can also export an image in another format (File > Export)

	convert PGM to other file formats, with e.g. ImageMagick (https://www.imagemagick.org/script/convert.php)
	$ convert mandelbrot.pgm mandelbrot.png
	and additionally use a colormap to map greyscale to colors:
	$ convert mandelbrot.pgm colormapline.png -clut mandelbrot.jpg
	$ convert \( mandelbrot.pgm -modulate 100,0 \) \( -size 1x1 xc:black xc:blue xc:cyan xc:green xc:yellow xc:red xc:magenta xc:white +append -resize 28x1! -size 227x1 xc:white +append -size 1x1 xc:black +append -resize 256x1! \) -clut mandelbrot.png


	The program uses the basic algorithms and does not employ any application specific optimizations
	(see e.g. https://en.wikipedia.org/wiki/Mandelbrot_set#Optimizations)

*/

/*#define USE_CLOCK*/
#define USE_CLOCKGETTIME

#include<stdlib.h>	/* malloc(),labs(),atol() */
#include<stdio.h>	/* printf() */

#if defined(USE_CLOCK) || defined(USE_CLOCKGETTIME)
#include <time.h>	/* clock(), clock_gettime(),clock_getres() */
#endif

typedef unsigned long Tindex;
typedef double Tfloat;
typedef unsigned int Titer;

const Tindex sizeofTiter=sizeof(Titer);


/*inline*/ Tindex colrow2index(Tindex column, Tindex row, Tindex numcolumns)
{
	return row*numcolumns+column;
}

void writePGM(Titer *M, Titer maxval, Tindex width, Tindex height, const char * filename)
{	/* see e.g. http://netpbm.sourceforge.net/doc/pgm.html */
	FILE *fhdl = fopen(filename, "w");
	if (!fhdl)
	{
		printf("ERROR opening file %s\n",filename);
		exit(2);
	}
	
	fprintf(fhdl, "P2\n");
	fprintf(fhdl, "# %s\twritten by mandelbrot\n",filename);
	fprintf(fhdl, "%lu %lu\n",width,height);
	fprintf(fhdl, "%u\n",maxval);
	Tindex row,column;
	for(row=0;row<height;++row)
	{
		for(column=0;column<width;++column)
		{
			fprintf(fhdl, "%u",M[colrow2index(column,row,width)]);
			if(column+1<width) fprintf(fhdl, " ");
		}
		fprintf(fhdl, "\n");
	}
	
	fclose(fhdl);
}

int main(int argc, char *argv[])
{

	Tindex numcolumns=400;
	Tindex numrows=400;
	Tfloat CrealMin=-2., CimgMin=-2.;
	Tfloat CrealMax=2., CimgMax=2.;
	Titer maxiter=255;	/* <= 65535 */
	Tfloat Zabsbound=2.;
	char filename_default[]="mandelbrot.pgm";
	
	double time_res=0;
#ifdef USE_CLOCK
	clock_t clk_start, clk_end;
	time_res=1.;
#endif
#ifdef USE_CLOCKGETTIME
	clockid_t clkt_id=CLOCK_MONOTONIC;      /* CLOCK_REALTIME,CLOCK_MONOTONIC,CLOCK_PROCESS_CPUTIME_ID,CLOCK_THREAD_CPUTIME_ID */
	struct timespec clkt_start,clkt_end,clkt_res;
	clock_getres(clkt_id,&clkt_res);
	time_res=clkt_res.tv_sec+(double)clkt_res.tv_nsec/1.E9;
#endif
	double time_diff=0.;
	Tfloat Zabs2bound=Zabsbound*Zabsbound;
	char* filename=filename_default;
	
	if (argc>1)
	{
		if (argc>2)
		{
			numcolumns=labs(atol(argv[1]));
			numrows=labs(atol(argv[2]));
		}
		else
		{
			printf("usage: %s [<width> <height>] [<maxiter>] [<CrealMin> <CimgMin> <CrealMax> <CimgMax>]\n",argv[0]);
			exit(0);
		}
		if (argc>3)
		{
			maxiter=labs(atol(argv[3]));
		}
		if (argc>7)
		{
			CrealMin=atof(argv[4]);
			CimgMin=atof(argv[5]);
			CrealMax=atof(argv[6]);
			CimgMax=atof(argv[7]);
		}
		if (argc>8)
		{
			filename=argv[8];
		}
	}

	Tindex numelements=numcolumns*numrows;
	
	printf("width x height:\t%lu x %lu\n",numcolumns,numrows);
	printf("C Range:\t%g + %g i\t-\t%g + %g i\n",CrealMin,CimgMin,CrealMax,CimgMax);
	printf("max. iterations:\t%u\n",maxiter);
	
	unsigned long arraysize=numelements*sizeofTiter;
	Titer *M=(Titer*)malloc(arraysize);
	if(!M)
	{
		printf("ERROR allocating memory (%lu Bytes)\n",arraysize);
		exit(1);
	}
	
	Tindex column, row;
	Tfloat Creal, Cimg;
	Tfloat Zreal, Zimg, Zreal_tmp;
	Titer i;
	Tfloat Zabs2;
	Titer* Mact;
	
	Tfloat dCreal=0.;
	if(numcolumns>1) dCreal=(CrealMax-CrealMin)/(numcolumns-1);
	Tfloat dCimg=0.;
	if(numrows>1) dCimg=(CimgMin-CimgMax)/(numrows-1);
	
/*------------------------------------------------------------------------------------------------*/	
#ifdef USE_CLOCK
	clk_start = clock();
#endif
#ifdef USE_CLOCKGETTIME
	clock_gettime(clkt_id,&clkt_start);
#endif
	Mact=M;
	Cimg=CimgMax;
	for(row=0;row<numrows;++row)
	{
		/*Cimg=CimgMax+row*dCimg;*/
		Creal=CrealMin;
#pragma omp parallel for private(Zreal_tmp, Zimg, Zreal, Zabs2, i)
		for(column=0;column<numcolumns;++column)
		{
			/*Creal=CrealMin+column*dCreal;*/
			Zreal=0.; Zimg=0.;
			for(i=0;i<maxiter;++i)
			{	/* 10 FlOp per iteration */
				/* Z_{n+1}=Z_n^2+C */	/* 7 FlOp (see below) */
				Zreal_tmp=Zreal*Zreal-Zimg*Zimg+Creal;	/* 4 FlOp */
				Zimg=2.*Zreal*Zimg+Cimg;	/* 3 FlOp */
				Zreal=Zreal_tmp;
				Zabs2=Zreal*Zreal+Zimg*Zimg;	/* 3 FlOp */
				if(Zabs2>Zabs2bound) break;
			}
#pragma omp critical
			{
			*(Mact++)=i;	/* *Mact=i; ++Mact; */
			/*M[colrow2index(column,row,numcolumns)]=i;*/
			Creal+=dCreal;
			}
		}
		/*printf("%lu\r",row*100/numrows);fflush(stdout);*/
		Cimg+=dCimg;
	}
#ifdef USE_CLOCKGETTIME
	clock_gettime(clkt_id,&clkt_end);
	time_diff=(double)(clkt_end.tv_sec-clkt_start.tv_sec)+(double)(clkt_end.tv_nsec-clkt_start.tv_nsec)/1.E9;
	printf("Time (clock_gettime):\t%g\n",time_diff);
#endif
#ifdef USE_CLOCK
	clk_end = clock();
	time_diff = (double)(clk_end-clk_start)/CLOCKS_PER_SEC;
	printf("Time (clock):\t%g\n",time_diff);
#endif
/*------------------------------------------------------------------------------------------------*/
	
	unsigned long sumiter=0;
	for(i=0;i<numelements;++i)
	{
		sumiter+=M[i];
	}
	printf("total number of iterations:\t%lu (%lu FlOp)\n",sumiter,sumiter*10);
	if(time_diff>0.) printf("FlOp/s:\t%g\n",sumiter*10./time_diff);
	
	writePGM(M,maxiter,numcolumns,numrows,filename);
	printf("PGM file:\t%s\n",filename);

	free(M);
	
	return 0;
}

