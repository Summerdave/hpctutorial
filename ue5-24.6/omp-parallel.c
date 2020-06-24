#include <stdio.h>

int main() {
#pragma omp parallel
	{
		printf("1");
		printf("2");
		printf("3");
		printf("4");
	}
	printf("\n");
	return 0;
}
