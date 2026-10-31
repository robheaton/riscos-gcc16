#include <iostream>
#include <locale>
#include <iomanip>
#include <ctime>
#include <string>
int main(){
  try { std::locale l("en_US.UTF-8"); std::cout << "en_US.UTF-8 ok name=" << l.name() << "\n";
    std::cout.imbue(l); std::cout << 1234567.891 << " " << std::showbase << std::put_money(12345) << "\n"; }
  catch(std::exception&e){ std::cout << "en_US.UTF-8 threw: " << e.what() << "\n"; }
  try { std::locale l("C"); std::cout << "C ok " << l.name() << "\n"; } catch(std::exception&e){ std::cout << "C threw: " << e.what() << "\n"; }
  try { std::locale l(""); std::cout << "empty ok " << l.name() << "\n"; } catch(std::exception&e){ std::cout << "empty threw: " << e.what() << "\n"; }
  try { std::locale l("xx_XX"); std::cout << "bogus ok\n"; } catch(std::exception&e){ std::cout << "bogus threw (expected)\n"; }
  std::locale::global(std::locale("")); std::cout << "global=" << std::locale().name() << "\n";
  const std::collate<char>& c = std::use_facet<std::collate<char>>(std::locale::classic());
  std::string a="a", b="B"; std::cout << "classic collate a,B: " << c.compare(a.data(),a.data()+1,b.data(),b.data()+1) << "\n";
  std::tm t{}; t.tm_year=126; t.tm_mon=0; t.tm_mday=5; t.tm_hour=9; t.tm_wday=1; std::cout << std::put_time(&t, "%A %d %B %Y %H:%M") << "\n";
  return 0; }
