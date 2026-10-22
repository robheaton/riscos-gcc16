#include <stdio.h>
#include <locale.h>
#include <langinfo.h>
#include <time.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
static const char*H(const char*s){ static char b[8][400]; static int k; char*o=b[k++&7]; size_t n=0; for(;*s&&n<380;s++){ unsigned char c=*s; if(c>=0x80||c<0x20) n+=sprintf(o+n,"\\x%02x",c); else o[n++]=c; } o[n]=0; return o; }
int main(int argc,char**argv){
  const char*loc=argc>1?argv[1]:"C";
  char *r=setlocale(LC_ALL,loc); printf("setlocale(%s)=%s\n",loc,r?r:"NULL");
  static const struct{const char*n;nl_item i;}items[]={{"CODESET",CODESET},{"RADIXCHAR",RADIXCHAR},{"THOUSEP",THOUSEP},{"D_T_FMT",D_T_FMT},{"D_FMT",D_FMT},{"T_FMT",T_FMT},{"T_FMT_AMPM",T_FMT_AMPM},{"AM_STR",AM_STR},{"PM_STR",PM_STR},
   {"DAY_1",DAY_1},{"DAY_7",DAY_7},{"ABDAY_1",ABDAY_1},{"ABDAY_7",ABDAY_7},{"MON_1",MON_1},{"MON_12",MON_12},{"ABMON_1",ABMON_1},{"ABMON_12",ABMON_12},{"YESEXPR",YESEXPR},{"NOEXPR",NOEXPR},{"ERA",ERA},{"CRNCYSTR",CRNCYSTR}};
  for(unsigned i=0;i<sizeof items/sizeof items[0];i++) printf("%s=[%s]\n",items[i].n,H(nl_langinfo(items[i].i)));
  struct lconv*l=localeconv(); printf("decimal=[%s] thousands=[%s] grouping=[%s] int_curr=[%s] currency=[%s] frac=%d p_cs=%d\n",H(l->decimal_point),H(l->thousands_sep),H(l->grouping),H(l->int_curr_symbol),H(l->currency_symbol),(signed char)l->frac_digits==-1?127:l->frac_digits,l->p_cs_precedes==255?127:l->p_cs_precedes);
  struct tm t={0}; t.tm_year=126; t.tm_mon=9; t.tm_mday=21; t.tm_hour=20; t.tm_min=30; t.tm_sec=5; t.tm_wday=3; t.tm_yday=293;
  char b[200]; strftime(b,sizeof b,"%A|%a|%B|%b|%c|%x|%X|%p|%r|%D|%e|%j|%U|%y|%Z",&t); printf("strftime=[%s]\n",H(b));
  printf("MB_CUR_MAX=%zu\n",MB_CUR_MAX);
  printf("strcoll(a,B)=%d strcoll(e,f)=%d strcoll(Z,a)=%d\n",strcoll("a","B")>0?1:strcoll("a","B")<0?-1:0,strcoll("e","f")<0?-1:1,strcoll("Z","a")>0?1:-1);
  return 0;}
