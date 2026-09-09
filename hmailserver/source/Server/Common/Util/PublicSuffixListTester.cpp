#include "StdAfx.h"
#include "PublicSuffixListTester.h"
#include "PublicSuffixList.h"
namespace HM
{
 void PublicSuffixListTester::Test(){TestRegistrableDomain_();TestDomainsWithoutRegistrableDomain_();}
 void PublicSuffixListTester::TestRegistrableDomain_(){PublicSuffixList*l=PublicSuffixList::Instance();if(l->GetRegistrableDomain(_T("example.com"))!=_T("example.com")||l->GetRegistrableDomain(_T("a.b.example.co.uk"))!=_T("example.co.uk")||l->GetRegistrableDomain(_T("a.b.evil.github.io"))!=_T("evil.github.io")||l->GetRegistrableDomain(_T("a.b.kawasaki.jp"))!=_T("a.b.kawasaki.jp")||l->GetRegistrableDomain(_T("www.city.kawasaki.jp"))!=_T("city.kawasaki.jp")){assert(0);throw;}}
 void PublicSuffixListTester::TestDomainsWithoutRegistrableDomain_(){PublicSuffixList*l=PublicSuffixList::Instance();String r;if(l->GetRegistrableDomain(_T("com"),r)||l->GetRegistrableDomain(_T("co.uk"),r)||l->GetRegistrableDomain(_T("github.io"),r)||l->GetRegistrableDomain(_T("localhost"),r)){assert(0);throw;}}
}
