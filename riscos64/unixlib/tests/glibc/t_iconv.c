/* prints the result of many iconv conversions; run with glibc and with UnixLib 64 and diff the output */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iconv.h>
#include <stdlib.h>
#include <stdarg.h>
static unsigned long long H=1469598103934665603ULL; static long NL; static int verbose;
static void P(const char*f,...){ char b[512]; va_list a; va_start(a,f); vsnprintf(b,sizeof b,f,a); va_end(a); for(char*p=b;*p;p++){H^=(unsigned char)*p;H*=1099511628211ULL;} NL++; if(verbose)fputs(b,stdout);}
static void section(const char*n){ printf("%s %ld %016llx\n",n,NL,H); H=1469598103934665603ULL; NL=0; }
static void run(const char*to,const char*from,const unsigned char*in,size_t n){
  iconv_t g=iconv_open(to,from);
  if(g==(iconv_t)-1){ P("%s->%s open fails errno=%d\n",from,to,errno==EINVAL); return; }
  char og[256]; char*ig=(char*)in; size_t lg=n,ogl=sizeof og; char*pg=og;
  size_t rg=iconv(g,&ig,&lg,&pg,&ogl); int eg=errno;
  P("%s->%s in=",from,to); for(size_t i=0;i<n;i++)P("%02x",in[i]);
  P(" ret=%ld err=%d left=%zu out=",(long)rg,rg==(size_t)-1?(eg==EILSEQ?1:eg==EINVAL?2:eg==E2BIG?3:9):0,lg); for(char*p=og;p<pg;p++)P("%02x",(unsigned char)*p); P("\n");
  iconv_close(g);
}
static void enc(unsigned cp,unsigned char*u,int*n){ if(cp<0x80){u[0]=cp;*n=1;}else if(cp<0x800){u[0]=0xC0|cp>>6;u[1]=0x80|cp&63;*n=2;}else if(cp<0x10000){u[0]=0xE0|cp>>12;u[1]=0x80|(cp>>6)&63;u[2]=0x80|cp&63;*n=3;}else{u[0]=0xF0|cp>>18;u[1]=0x80|(cp>>12)&63;u[2]=0x80|(cp>>6)&63;u[3]=0x80|cp&63;*n=4;} }
int main(int argc,char**argv){ verbose=argc>1;
  const char*sets[]={"ASCII","ISO-8859-1","ISO-8859-2","ISO-8859-3","ISO-8859-4","ISO-8859-5","ISO-8859-7","ISO-8859-9","ISO-8859-10","ISO-8859-13","ISO-8859-14","ISO-8859-15","ISO-8859-16","WINDOWS-1250","WINDOWS-1251","WINDOWS-1252","WINDOWS-1253","WINDOWS-1254","WINDOWS-1257","CP437","CP850","CP858","CP866","KOI8-R","KOI8-U",0};
  for(int s=0;sets[s];s++){ if(s)section(sets[s-1]);
    for(int b=0;b<256;b++){ unsigned char c=b; run("UTF-8",sets[s],&c,1); }
    for(unsigned cp=0;cp<0x3000;cp++){ if(cp>=0xD800&&cp<0xE000)continue; unsigned char u[4];int n; enc(cp,u,&n); run(sets[s],"UTF-8",u,n); }
  }
  section("last-set");
  static const char*bad[]={"\x80","\xC0\x80","\xC1\xBF","\xE0\x80\x80","\xED\xA0\x80","\xC3","\xE2\x82","\xF0\x9F\x98","\xFF","\xC3\xA9\x80","a\xE2\x82\xACz","\xEF\xBB\xBF" "x","\xF0\x9F\x98\x80",0};
  const char*targets[]={"UTF-16LE","UTF-16BE","UTF-16","UTF-32","UTF-32LE","UTF-32BE","UCS-2","UCS-2LE","UCS-2BE","ISO-8859-1","ASCII","UTF-8",0};
  for(int i=0;bad[i];i++)for(int t=0;targets[t];t++) run(targets[t],"UTF-8",(const unsigned char*)bad[i],strlen(bad[i]));
  for(int t=0;targets[t];t++) for(unsigned cp=0;cp<0x110000;cp+=(cp<0x800?1:cp<0x20000?37:0x111)){ if(cp>=0xD800&&cp<0xE000)continue; if(cp>=0xE0000&&cp<=0xE007F)continue; unsigned char u[4];int n; enc(cp,u,&n); run(targets[t],"UTF-8",u,n); }
  section("utf-edge+cp");
  static const unsigned char u16[][6]={{0xFF,0xFE,0x41,0,0,0},{0xFE,0xFF,0,0x41,0,0},{0x41,0,0x42,0,0,0},{0x3D,0xD8,0x00,0xDE,0,0},{0x00,0xDE,0x41,0,0,0},{0x3D,0xD8,0x41,0,0,0},{0x3D,0xD8,0,0,0,0},{0x41,0,0,0,0,0}};
  const int l16[]={4,4,4,4,4,4,2,3};
  for(int i=0;i<8;i++){ const char*f[]={"UTF-16","UTF-16LE","UTF-16BE","UCS-2","UCS-2BE",0}; for(int k=0;f[k];k++) run("UTF-8",f[k],u16[i],l16[i]); }
  static const unsigned char u32[][8]={{0xFF,0xFE,0,0,0x41,0,0,0},{0,0,0xFE,0xFF,0,0,0,0x41},{0x41,0,0,0,0x42,0,0,0},{0,0xD8,0,0,0,0,0,0},{0x41,0,0,0,0,0,0,0},{0x41,0,0,0,0x42,0,0,0}};
  const int l32[]={8,8,8,4,4,6};
  for(int i=0;i<6;i++){ const char*f[]={"UTF-32","UTF-32LE","UTF-32BE",0}; for(int k=0;f[k];k++) run("UTF-8",f[k],u32[i],l32[i]); }
  section("utf16-in+utf32-in");
  const char*names[]={"utf8","UTF8","utf-8","ISO_8859-1","ISO8859-1","iso-8859-15","latin9","CP1252","windows-1252","ANSI_X3.4-1968","US-ASCII","bogus","UTF-8//IGNORE","UTF-8//TRANSLIT","UCS2","UCS-2LE","utf16","CP819","KOI8R","IBM437","cp850",0};
  for(int i=0;names[i];i++){ unsigned char a='a'; run("UTF-8",names[i],&a,1); run(names[i],"UTF-8",&a,1); }
  section("names");
  /* two calls on one descriptor: state (UTF-16 writes the byte order mark once) and reset */
  { iconv_t c=iconv_open("UTF-16LE","UTF-8"); char o[64]; char*po=o; size_t ol=sizeof o; char in1[]="ab"; char*pi=in1; size_t il=2; iconv(c,&pi,&il,&po,&ol); char in2[]="c\xC3\xA9"; pi=in2; il=3; iconv(c,&pi,&il,&po,&ol); P("two calls: %zu bytes:",(size_t)(po-o)); for(char*p=o;p<po;p++)P(" %02x",(unsigned char)*p); P("\n"); iconv_close(c); }
  { iconv_t c=iconv_open("UTF-16","UTF-8"); char o[64]; char*po=o; size_t ol=sizeof o; char in1[]="ab"; char*pi=in1; size_t il=2; iconv(c,&pi,&il,&po,&ol); iconv(c,0,0,0,0); pi=in1; il=2; iconv(c,&pi,&il,&po,&ol); P("bom twice after reset: %zu bytes:",(size_t)(po-o)); for(char*p=o;p<po;p++)P(" %02x",(unsigned char)*p); P("\n"); iconv_close(c); }
  /* output buffer too small in the middle of a string */
  { iconv_t c=iconv_open("UTF-8","ISO-8859-1"); char o[5]; char*po=o; size_t ol=5; char in[]="a\xE9\xE9\xE9"; char*pi=in; size_t il=4; size_t r=iconv(c,&pi,&il,&po,&ol); P("e2big: ret=%ld err=%d consumed=%d written=%d left=%zu\n",(long)r,errno==E2BIG,(int)(pi-in),(int)(po-o),il); iconv_close(c); }
  section("state");
  return 0;
}
