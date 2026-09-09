// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com
#pragma once
#include "../SpamTest.h"
#include "DMARCRecord.h"
namespace HM
{
   class SenderAuthentication;
   class SpamTestDMARC : public SpamTest
   {
   public:
      virtual String GetName() const;
      virtual bool GetIsEnabled();
      virtual SpamTestType GetTestType() {return PostTransmission; }
      virtual bool GetAlwaysRun() {return true; }
      virtual std::set<std::shared_ptr<SpamTestResult> > RunTest(std::shared_ptr<SpamTestData> pTestData);
   private:
      static String GetHeaderFromDomain_(std::shared_ptr<SpamTestData> pTestData);
      bool IsAuthenticated_(std::shared_ptr<SenderAuthentication> senderAuthentication, const DMARCRecord &record, const String &headerFromDomain);
      static DMARCRecord::Policy GetPolicyToApply_(const DMARCRecord &record, DMARCRecord::Policy policy);
      static DMARCRecord::Policy DegradePolicy_(DMARCRecord::Policy policy);
   };
}
