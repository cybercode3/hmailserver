// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com
#include "StdAfx.h"
#include "PublicSuffixList.h"
#include "Utilities.h"
#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif
namespace HM
{
   namespace { const String FileName=_T("public_suffix_list.dat"); bool IsAscii(const String &v){for(TCHAR c:v) if(c>127)return false; return true;} }
   void PublicSuffixList::Initialize()
   {
      rules_.clear(); wildcard_rules_.clear(); exception_rules_.clear();
      String fileName=Utilities::GetBinDirectory(); if(fileName.Right(1)!=_T("\\")) fileName+=_T("\\"); fileName+=FileName;
#ifdef _DEBUG
      if(!FileUtilities::Exists(fileName)) fileName=_T("C:\\Program Files\\hMailServer\\Bin\\")+FileName;
#endif
      String contents=FileUtilities::ReadCompleteTextFile(fileName);
      if(contents.IsEmpty()){String msg;msg.Format(_T("Failed to load the public suffix list %s."),fileName.c_str());ErrorManager::Instance()->ReportError(ErrorManager::Medium,4337,"PublicSuffixList::Initialize",msg);return;}
      std::vector<String> lines=StringParser::SplitString(contents,_T("\n"));
      for(String line:lines){line.Trim();line.ToLower();if(line.IsEmpty()||line.StartsWith(_T("//"))||!IsAscii(line))continue;if(line.StartsWith(_T("!")))exception_rules_.insert(line.Mid(1));else if(line.StartsWith(_T("*.")))wildcard_rules_.insert(line.Mid(2));else rules_.insert(line);}
   }
   String PublicSuffixList::GetRegistrableDomain(const String &domain) const {String r;if(GetRegistrableDomain(domain,r))return r;r=domain;r.ToLower();return r;}
   bool PublicSuffixList::GetRegistrableDomain(const String &domain,String &registrableDomain) const
   {
      String result=domain;result.ToLower();while(result.EndsWith(_T(".")))result=result.Mid(0,result.GetLength()-1);
      std::vector<String> labels=StringParser::SplitString(result,_T("."));if(labels.size()<2)return false;size_t suffix=GetPublicSuffixLabelCount_(labels);if(suffix>=labels.size())return false;registrableDomain=JoinLabels_(labels,labels.size()-suffix-1);return true;
   }
   size_t PublicSuffixList::GetPublicSuffixLabelCount_(const std::vector<String>&labels) const
   {
      size_t n=labels.size();for(size_t i=0;i<n;i++)if(exception_rules_.find(JoinLabels_(labels,i))!=exception_rules_.end())return n-i-1;
      for(size_t i=0;i<n;i++){if(rules_.find(JoinLabels_(labels,i))!=rules_.end())return n-i;if(i+1<n&&wildcard_rules_.find(JoinLabels_(labels,i+1))!=wildcard_rules_.end())return n-i;}return 1;
   }
   String PublicSuffixList::JoinLabels_(const std::vector<String>&labels,size_t first){String r;for(size_t i=first;i<labels.size();i++){if(i>first)r+=_T(".");r+=labels[i];}return r;}
}
