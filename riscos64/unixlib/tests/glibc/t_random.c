#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
  unsigned long long h=1469598103934665603ULL; long n=0;
  #define H(v) do{ h^=(unsigned long long)(unsigned)(v); h*=1099511628211ULL; n++; }while(0)
  { long a=random(); long b=random(); int c=rand(); printf("default first: %ld %ld %d\n", a, b, c); }
  for(unsigned s=0;s<20;s++){ srandom(s*7919u); for(int i=0;i<10000;i++) H(random()); }
  printf("srandom 20 seeds x 10000: %016llx\n",h); h=1469598103934665603ULL;
  for(unsigned s=0;s<5;s++){ srand(s); for(int i=0;i<1000;i++) H(rand()); }
  printf("srand/rand: %016llx\n",h); h=1469598103934665603ULL;
  { unsigned sd=42; for(int i=0;i<100000;i++) H(rand_r(&sd)); printf("rand_r: %016llx\n",h); h=1469598103934665603ULL; }
  static const size_t sizes[]={8,16,31,32,40,63,64,100,127,128,200,255,256,300,1024};
  for(unsigned k=0;k<sizeof sizes/sizeof*sizes;k++){ char *st=malloc(sizes[k]+16); char*old=initstate(1234+k,st,sizes[k]); for(int i=0;i<5000;i++) H(random()); printf("initstate %zu: %016llx\n",sizes[k],h); h=1469598103934665603ULL; if(old) {} }
  { char a[256], b[128]; initstate(1,a,256); long x=random(); initstate(2,b,128); long y=random(); char *o=setstate(a); long x2=random(); o=setstate(b); long y2=random(); printf("setstate: %ld %ld %ld %ld\n",x,y,x2,y2); (void)o; }
  { char *r=initstate(1,(char*)malloc(4),4); printf("initstate(4) -> %s\n", r?"ok":"NULL"); }
  return 0; }
