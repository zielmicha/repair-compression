all: repair-omp huffenc
repair-omp: repair-omp.cpp
	g++ -DNUMTHREADS=4 -O2 -Wall -g -fopenmp -std=c++11 repair-omp.cpp -o repair-omp

huffenc: huffenc.cu
	nvcc huffenc.cu -std=c++11 -o huffenc
