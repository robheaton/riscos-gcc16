#include <stdio.h>
#include <time.h>
#include <string.h>
int main(void){ struct tm t={0}; t.tm_year=126; t.tm_mon=0; t.tm_mday=5; t.tm_hour=9; t.tm_min=3; t.tm_sec=7; t.tm_wday=1; t.tm_yday=4;
 const char*f[]={"%-d","%e","%_d","%-H","%-m","%5d","%-I","%k","%l","%s","%G %g %V","%u %w","%C","%n%t|","%%","%10A","%^a","%y%Y","%Ey","%Od","%z","%Z","%D %F %T %R %h",0};
 for(int i=0;f[i];i++){ char b[100]; size_t n=strftime(b,sizeof b,f[i],&t); printf("[%s] -> %zu [%s]\n",f[i],n,b);} return 0; }
