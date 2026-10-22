#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
static int cmp(const void*a,const void*b){ return strcoll(*(char*const*)a,*(char*const*)b); }
int main(int argc,char**argv){
  const char*loc=argc>1?argv[1]:"en_US.UTF-8"; if(!setlocale(LC_ALL,loc)){ printf("no locale\n"); return 1; }
  static const char*w[]={"a","A","b","B","z","Z","aa","aA","Aa","AA","ab","aB","a b","a-b","ab ","a_b","a.b","a,b","a;b","(a)","[a]","{a}","<a>","a1","a10","a2","1","10","2","0","a0","apple","Apple","APPLE","apple pie","apple-pie","applepie","apple_pie","Applepie","banana","Banana","cherry","cherry pie",
   "resume","résumé","Resume","Résumé","resumé","coté","côte","cote","côté","naive","naïve","Zürich","Zurich","zebra","ångström","angstrom","Ångström","ñandú","nandu","ñu","nz","ø","o","ö","oe","œ","æ","ae","ß","ss","straße","strasse","çedilla","cedilla","Ünder","under","Éclair","eclair","Ecole","école",
   "hello","Hello","HELLO","hello world","hello-world","hello_world","Hello World","~tilde","!bang","#hash","$dollar","%percent","&amp","*star","+plus","=eq","?q","@at","^caret","`tick","|pipe","/slash","\\back",":colon","'q","\"dq","tab\there","","x","X","y","Y","_","-","--","- -","a--b","a-- b",
   "Ωmega","omega","αλφα","Αλφα","βήτα","абв","Абв","бвг","ёж","еж","я","Я","яблоко","Яблоко","日本","中文","한국","€","$","£","¥","©","®","™","½","¼","²","³","°","±","×","÷","µ","¶","§","¿","¡","«","»","\xE2\x80\x9Cq\xE2\x80\x9D","\xE2\x80\x98q\xE2\x80\x99",
   "file1","file2","file10","File1","FILE1","file_1","file-1","file.1","file 1","file.txt","file.TXT","File.txt","file.c","file.h","README","readme","Readme","ReadMe","Makefile","makefile","MAKEFILE","a\xC3","\xFF" "b","z\xC3\xA9","zz","Zz","zZ","ZZ",0};
  int n=0; while(w[n]) n++;
  const char**v=malloc(n*sizeof*v); memcpy(v,w,n*sizeof*v);
  unsigned long long h=1469598103934665603ULL; long diffs=0;
  for(int i=0;i<n;i++)for(int j=0;j<n;j++){ int c=strcoll(w[i],w[j]); c=c<0?-1:c>0?1:0; h^=(unsigned)(c+1); h*=1099511628211ULL; if(argc>2 && c!=0) {} }
  printf("pairs hash %016llx n=%d\n",h,n);
  qsort(v,n,sizeof*v,cmp);
  for(int i=0;i<n;i++){ for(const unsigned char*p=(const unsigned char*)v[i];*p;p++) { if(*p>=0x80||*p<0x20) printf("\\x%02x",*p); else putchar(*p);} putchar(10);}
  /* strxfrm consistent with strcoll */
  int bad=0; for(int i=0;i<n;i+=3)for(int j=0;j<n;j+=2){ char a[1024],b[1024]; strxfrm(a,w[i],sizeof a); strxfrm(b,w[j],sizeof b); int x=strcmp(a,b),y=strcoll(w[i],w[j]); if((x<0)!=(y<0)||(x>0)!=(y>0)) bad++; }
  printf("xfrm inconsistent: %d\n",bad);
  return 0; }
