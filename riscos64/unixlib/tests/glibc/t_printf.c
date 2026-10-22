#define _GNU_SOURCE
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
static int xprintf(const char*f,...){ char b[2000]; va_list a; va_start(a,f); vsnprintf(b,sizeof b,f,a); va_end(a); int n=0; for(const unsigned char*p=(const unsigned char*)b;*p;p++){ if(*p>=0x80) n+=printf("\\x%02x",*p); else {putchar(*p); n++;} } return n; }
int main(int argc,char**argv){
  const char*loc=argc>1?argv[1]:"C"; if(!setlocale(LC_ALL,loc)){ printf("no locale\n"); return 1; }
  static const long long iv[]={0,1,-1,12,999,1000,-1000,12345,123456,1234567,-1234567,2147483647LL,-2147483647LL-1,12345678901234LL,-9876543210123LL,1000000000000000000LL};
  static const char*ifmt[]={"%d","%'d","%'10d","%-'12d|","%'012d","%+'d","% 'd","%'.8d","%'5.3d","%'*d"};
  for(unsigned f=0;f<sizeof ifmt/sizeof*ifmt;f++){ for(unsigned i=0;i<sizeof iv/sizeof*iv;i++){ char b[200]; int v=(int)iv[i]; if(strstr(ifmt[f],"*")) snprintf(b,sizeof b,ifmt[f],14,v); else snprintf(b,sizeof b,ifmt[f],v); xprintf("[%s] ",b);} printf("\n"); }
  static const char*lfmt[]={"%ld","%'ld","%'lld","%'20lld","%'-20lld|","%'lu","%'llu","%'jd","%'zd","%'i"};
  for(unsigned f=0;f<sizeof lfmt/sizeof*lfmt;f++){ for(unsigned i=0;i<sizeof iv/sizeof*iv;i++){ char b[200]; if(strstr(lfmt[f],"u")) snprintf(b,sizeof b,lfmt[f],(unsigned long long)iv[i]); else if(strstr(lfmt[f],"jd")) snprintf(b,sizeof b,lfmt[f],(intmax_t)iv[i]); else if(strstr(lfmt[f],"zd")) snprintf(b,sizeof b,lfmt[f],(ssize_t)iv[i]); else if(strstr(lfmt[f],"i")&&!strstr(lfmt[f],"l")) snprintf(b,sizeof b,lfmt[f],(int)iv[i]); else snprintf(b,sizeof b,lfmt[f],iv[i]); xprintf("[%s] ",b);} printf("\n"); }
  static const double dv[]={0.0,0.5,1.5,-1234.5678,1234567.891,1e10,1e-5,123456789.0,999.9996,1e15,-0.0,3.14159265358979};
  static const char*dfmt[]={"%f","%'f","%'.2f","%'15.3f","%'-15.1f|","%'015.2f","%+'.0f","%#'.0f","%e","%'e","%g","%'g","%'G","%.3g","%'.10g","%10.2f","%-10.2f|","%'.0f","%a"};
  for(unsigned f=0;f<sizeof dfmt/sizeof*dfmt;f++){ for(unsigned i=0;i<sizeof dv/sizeof*dv;i++){ char b[400]; snprintf(b,sizeof b,dfmt[f],dv[i]); xprintf("[%s] ",b);} printf("\n"); }
  { char b[200]; long double L=1234567.25L; snprintf(b,sizeof b,"%'.3Lf",L); xprintf("[%s] ",b); snprintf(b,sizeof b,"%'Lg",L); xprintf("[%s]\n",b); }
  { char b[200]; snprintf(b,sizeof b,"%'f %'f %'f",NAN,INFINITY,-INFINITY); xprintf("[%s]\n",b); }
  { char b[300]; snprintf(b,sizeof b,"%s|%5s|%-5s|%.2s|%c|%x|%'x|%o|%5%|%%|%-4c|",  "abc","ab","ab","abcdef",'z',255,255,8,'q'); xprintf("[%s]\n",b); }
  { char b[300]; int n=0; snprintf(b,sizeof b,"abc%n def %'d%n",&n,1234,&n); xprintf("[%s] n=%d\n",b,n); }
  { char b[300]; snprintf(b,sizeof b,"don't %d, it's %'d (%s)",1,1000000,"x'y"); xprintf("[%s]\n",b); }
  { printf("direct: %'d %'.2f\n",1234567,1234567.891); fprintf(stdout,"fprintf: %'u\n",4000000000u); }
  { char *s; if(asprintf(&s,"asprintf %'d",1234567)>0) {printf("%s\n",s); free(s);} }
  { char b[20]; int n=snprintf(b,sizeof b,"%'d and more text here",1234567); xprintf("short: [%s] n=%d\n",b,n); }
  return 0; }
