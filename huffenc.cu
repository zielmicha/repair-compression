#include <cstdio>
#include <thrust/scan.h>
#include <thrust/device_vector.h>

__global__ void computeLengths(int* data, int* lengths, int* symbolSizes, int n) {
    int thid = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (thid >= n) return;
    lengths[thid] = symbolSizes[data[thid]];
    //printf("%d %d\n", thid, lengths[thid]);
}

int* toIntPtr(thrust::device_vector<int>& v) {
    return thrust::raw_pointer_cast(&v[0]);
}

__global__ void computeFirstByteSym(int* lengthSum, int* firstByteSym, int n, int outSize) {
    int thid = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (thid >= n || thid == 0) return;

    int curr = (lengthSum[thid - 1] / 8);
    int prev = (thid == 1) ? 0 : (lengthSum[thid - 2]/8);
    if (prev != curr) {
        //printf("sym: %d, curr: %d\n", thid, curr);
        firstByteSym[curr] = thid;
    }
}

__global__ void computeResult(int* symbols, int* data, int* firstByteSym, int* lengthSum, int* results, int n, int outSize) {
    int thid = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (thid >= outSize) return;

    int symI = firstByteSym[thid];
    if (symI != 0) symI --;
    int offsetDelta = thid * 8;

    //printf("thid: %d, sym: %d\n", thid, symI);
    int result = 0;
    while (symI < n) {
        int shift = (symI == 0 ? 0 : lengthSum[symI-1]) - offsetDelta;
        //printf("thid: %d | symI: %d, shift: %d\n", thid, symI, shift);
        if (shift > 8) break;
        int code = symbols[data[symI]];
        if (shift >= 0)
            result |= code << shift;
        else
            result |= code >> (-shift);
        symI ++;
    }
    results[thid] = result;
}

int main(){
    cudaSetDevice(1);
    int symbolsData[] = {0b1, 0b01, 0b001, 0b000};
    int sizesData[] = {1, 2, 3, 3};
    const int symN = 4;
    const int dataN = 10000;

    thrust::device_vector<int> data;
    for (int i=0; i < dataN; i ++) data.push_back(i % symN);
    thrust::device_vector<int> symbols (symbolsData, symbolsData + symN);
    thrust::device_vector<int> symbolSizes (sizesData, sizesData + symN);

    thrust::device_vector<int> lengthSum (dataN);

    computeLengths<<<(dataN + 1023) / 1024, 1024>>>
        (toIntPtr(data), toIntPtr(lengthSum),
         toIntPtr(symbolSizes), dataN);
    thrust::inclusive_scan(lengthSum.begin(), lengthSum.end(), lengthSum.begin());

    //for (int i : lengthSum) printf("l=%d\n", i);

    int outSize = (lengthSum.back() / 8 + 1);

    thrust::device_vector<int> firstByteSym;
    firstByteSym.resize(outSize, 0);
    computeFirstByteSym<<<(outSize + 1023) / 1024, 1024>>>
        (toIntPtr(lengthSum),
         toIntPtr(firstByteSym),
         dataN, outSize);

    //for (int i : firstByteSym) printf("s=%d\n", i);

    thrust::device_vector<int> result;
    result.resize(outSize, 0);

    computeResult<<<(outSize + 1023) / 1024, 1024>>>
        (toIntPtr(symbols),
         toIntPtr(data),
         toIntPtr(firstByteSym),
         toIntPtr(lengthSum),
         toIntPtr(result),
         dataN, outSize);

    //for (int i : result) printf("result=%d\n", i);
    cudaDeviceSynchronize ();
    return 0;
}
