/* dump.c LOCALE: prints one C initializer of struct ul_locale (see locale_data.h) from the glibc locale LOCALE.UTF-8 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <langinfo.h>
static void q(const char*s){ putchar('"'); for(;*s;s++){ unsigned char c=*s; if(c=='"'||c=='\\')printf("\\%c",c); else if(c<0x20||c==0x7f) printf("\\%03o",c); else putchar(c);} putchar('"'); }
#include <limits.h>
static void N(int v,int last){ if(v==CHAR_MAX) printf("CHAR_MAX"); else printf("%d",v); printf(last?" }, ":", "); }
static void L(const char*what,nl_item i0,int n){ printf("  { "); for(int i=0;i<n;i++){ if(i)printf(", "); q(nl_langinfo(i0+i)); } printf(" }, /* %s */\n",what); }
int main(int argc,char**argv){
  char name[64]; snprintf(name,sizeof name,"%s.UTF-8",argv[1]);
  if(!setlocale(LC_ALL,name)){ fprintf(stderr,"no locale %s\n",name); return 1; }
  printf("{ \"%s\",\n",argv[1]);
  L("abday",ABDAY_1,7); L("day",DAY_1,7); L("abmon",ABMON_1,12); L("mon",MON_1,12);
  printf("  "); q(nl_langinfo(AM_STR)); printf(", "); q(nl_langinfo(PM_STR)); printf(", /* am, pm */\n");
  printf("  "); q(nl_langinfo(D_T_FMT)); printf(", "); q(nl_langinfo(D_FMT)); printf(", "); q(nl_langinfo(T_FMT)); printf(", "); q(nl_langinfo(T_FMT_AMPM)); printf(", /* d_t_fmt d_fmt t_fmt t_fmt_ampm */\n");
  struct lconv*l=localeconv();
  printf("  "); q(l->decimal_point); printf(", "); q(l->thousands_sep); printf(", "); q(l->grouping); printf(", /* radix thousep grouping */\n");
  printf("  "); q(nl_langinfo(YESEXPR)); printf(", "); q(nl_langinfo(NOEXPR)); printf(", /* yesexpr noexpr */\n");
  printf("  "); q(l->int_curr_symbol); printf(", "); q(l->currency_symbol); printf(", "); q(l->mon_decimal_point); printf(", "); q(l->mon_thousands_sep); printf(", "); q(l->mon_grouping); printf(", "); q(l->positive_sign); printf(", "); q(l->negative_sign); printf(", "); q(nl_langinfo(CRNCYSTR)); printf(",\n");
  { int v[14]={l->int_frac_digits,l->frac_digits,l->p_cs_precedes,l->p_sep_by_space,l->n_cs_precedes,l->n_sep_by_space,l->p_sign_posn,l->n_sign_posn,l->int_p_cs_precedes,l->int_p_sep_by_space,l->int_n_cs_precedes,l->int_n_sep_by_space,l->int_p_sign_posn,l->int_n_sign_posn};
    printf("  "); for(int k=0;k<14;k++) N(v[k],k==13); printf("/* int_frac frac p_cs p_sep n_cs n_sep p_sign n_sign int_p_cs int_p_sep int_n_cs int_n_sep int_p_sign int_n_sign */\n"); }
  return 0; }
