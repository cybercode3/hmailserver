// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com
#pragma once
namespace HM
{
   class DMARCTxtLookup
   {
   public:
      virtual ~DMARCTxtLookup() {}
      virtual bool GetTXTRecords(const String &domain, std::vector<String> &records) = 0;
   };
   class DMARCDnsTxtLookup : public DMARCTxtLookup
   {
   public:
      virtual bool GetTXTRecords(const String &domain, std::vector<String> &records);
   };
}
