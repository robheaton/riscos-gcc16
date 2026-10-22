#define _GNU_SOURCE
#include <stdio.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <langinfo.h>
#include <time.h>
#include <ctype.h>
int main(void){
  printf("initial LC_ALL=%s CTYPE=%s MB_CUR_MAX=%zu\n",setlocale(LC_ALL,NULL),setlocale(LC_CTYPE,NULL),MB_CUR_MAX);
  printf("set en_GB.UTF-8: %s\n",setlocale(LC_ALL,"en_GB.UTF-8"));
  printf("TIME=%s CTYPE=%s COLLATE=%s MB_CUR_MAX=%zu CODESET=%s\n",setlocale(LC_TIME,NULL),setlocale(LC_CTYPE,NULL),setlocale(LC_COLLATE,NULL),MB_CUR_MAX,nl_langinfo(CODESET));
  printf("set LC_CTYPE=C: %s\n",setlocale(LC_CTYPE,"C"));
  printf("LC_ALL now: %s\n",setlocale(LC_ALL,NULL));
  printf("MB_CUR_MAX=%zu CODESET=%s\n",MB_CUR_MAX,nl_langinfo(CODESET));
  printf("bad name: %s\n",setlocale(LC_ALL,"xx"));
  printf("bad name keeps: %s\n",setlocale(LC_ALL,NULL));
  printf("composite: %s\n",setlocale(LC_ALL,"LC_CTYPE=en_US.UTF-8;LC_NUMERIC=C;LC_TIME=de_DE.UTF-8;LC_COLLATE=C;LC_MONETARY=C;LC_MESSAGES=C;LC_PAPER=C;LC_NAME=C;LC_ADDRESS=C;LC_TELEPHONE=C;LC_MEASUREMENT=C;LC_IDENTIFICATION=C"));
  printf("TIME=%s CTYPE=%s\n",setlocale(LC_TIME,NULL),setlocale(LC_CTYPE,NULL));
  setenv("LANG","fr_FR.UTF-8",1); unsetenv("LC_ALL");
  printf("env LANG=fr_FR.UTF-8: %s  DAY_1=%s\n",setlocale(LC_ALL,""),nl_langinfo(DAY_1));
  setenv("LC_TIME","en_GB.UTF-8",1);
  printf("LC_TIME env en_GB: %s\n",setlocale(LC_ALL,""));
  setenv("LC_ALL","de_DE.UTF-8",1);
  printf("LC_ALL env de_DE: %s DAY_1=%s\n",setlocale(LC_ALL,""),nl_langinfo(DAY_1));
  setenv("LC_ALL","zz_ZZ",1);
  printf("LC_ALL env unknown: %s\n",setlocale(LC_ALL,""));
  unsetenv("LC_ALL"); setlocale(LC_ALL,"en_GB.UTF-8");
  struct tm t={0}; t.tm_year=126; t.tm_mon=0; t.tm_mday=5; t.tm_hour=9; t.tm_min=3; t.tm_sec=7; t.tm_wday=1;
  char b[200]; strftime(b,sizeof b,"%c|%x|%X|%r|%p|%P|%^a|%^B|%10A|%-d|%e",&t); printf("%s\n",b);
  struct tm u={0}; char*e=strptime("05/01/26 09:03:07","%x %X",&u); printf("strptime: %s year=%d mon=%d day=%d h=%d\n",e?"ok":"fail",u.tm_year,u.tm_mon,u.tm_mday,u.tm_hour);
  setlocale(LC_ALL,"C"); strftime(b,sizeof b,"%c|%x|%X|%r|%p",&t); printf("C: %s\n",b);
  setlocale(LC_ALL,"en_US.UTF-8");
  wchar_t w[10]; mbstowcs(w,"h\xC3\xA9llo",10); printf("towupper: %lc iswalpha(e-acute)=%d\n",(wint_t)towupper(w[1]),iswalpha(w[1])!=0);
  printf("wcscoll(a,B)=%d wcscoll(B,a)=%d\n",wcscoll(L"a",L"B")<0,wcscoll(L"B",L"a")>0);
  return 0; }
