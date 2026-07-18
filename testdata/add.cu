#include <stdio.h>
__global__ void add(int *a,int *b,int *c){ int i=threadIdx.x; c[i]=a[i]+b[i]; }
int main(){
  int a[4]={1,2,3,4}, b[4]={10,20,30,40}, c[4]={0};
  int *da,*db,*dc;
  cudaMalloc(&da,16); cudaMalloc(&db,16); cudaMalloc(&dc,16);
  cudaMemcpy(da,a,16,cudaMemcpyHostToDevice);
  cudaMemcpy(db,b,16,cudaMemcpyHostToDevice);
  add<<<1,4>>>(da,db,dc);
  cudaMemcpy(c,dc,16,cudaMemcpyDeviceToHost);
  printf("sum0=%d\n", c[0]);
  return c[0]==11?0:1;
}
