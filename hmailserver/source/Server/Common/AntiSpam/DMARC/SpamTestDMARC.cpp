// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com
#include "StdAfx.h"
#include "SpamTestDMARC.h"
#include "DMARCEvaluator.h"
#include "DMARCPolicyLocator.h"
#include "DMARCTxtLookup.h"
#include "../AntiSpamConfiguration.h"
#include "../SenderAuthentication.h"
#include "../SpamTestData.h"
#include "../SpamTestResult.h"
#include "../DKIM/DKIM.h"
#include "../../BO/MessageData.h"
#include "../../Util/Parsing/AddresslistParser.h"
#include "../../../SMTP/SPF/SPF.h"
#include <random>
#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif
namespace HM
{
   String SpamTestDMARC::GetName() const { return "SpamTestDMARC"; }
   bool SpamTestDMARC::GetIsEnabled() { return Configuration::Instance()->GetAntiSpamConfiguration().GetDMARCEnabled(); }
   std::set<std::shared_ptr<SpamTestResult> > SpamTestDMARC::RunTest(std::shared_ptr<SpamTestData> pTestData)
   {
      std::set<std::shared_ptr<SpamTestResult> > results;
      String headerFromDomain = GetHeaderFromDomain_(pTestData); if (headerFromDomain.IsEmpty()) return results;
      DMARCRecord record; String policyDomain; DMARCPolicyLocator locator(std::shared_ptr<DMARCTxtLookup>(new DMARCDnsTxtLookup));
      if (locator.Locate(headerFromDomain, record, policyDomain) != DMARCPolicyLocator::Result::Found) return results;
      std::shared_ptr<SenderAuthentication> auth = pTestData->GetSenderAuthentication();
      auth->EvaluateSPF(pTestData); auth->EvaluateDKIM(pTestData);
      if (IsAuthenticated_(auth, record, headerFromDomain))
      {
         auth->SetDMARCResult(SenderAuthentication::DMARCResult::Pass, headerFromDomain);
         results.insert(std::shared_ptr<SpamTestResult>(new SpamTestResult(GetName(), SpamTestResult::Pass, 0, "")));
         return results;
      }
      auth->SetDMARCResult(SenderAuthentication::DMARCResult::Fail, headerFromDomain);
      String message; message.Format(_T("Rejected by DMARC. (%s)"), headerFromDomain.c_str());
      AntiSpamConfiguration &config = Configuration::Instance()->GetAntiSpamConfiguration();
      std::shared_ptr<SpamTestResult> result(new SpamTestResult(GetName(), SpamTestResult::Fail, config.GetDMARCFailureScore(), message));
      if (config.GetDMARCHonorPolicy())
      {
         DMARCRecord::Policy policy = DMARCEvaluator::GetApplicablePolicy(record, headerFromDomain, policyDomain);
         switch (GetPolicyToApply_(record, policy))
         {
         case DMARCRecord::Policy::Reject: result->SetRejectMessage(true); break;
         case DMARCRecord::Policy::Quarantine: result->SetMarkAsSpam(true); break;
         }
      }
      results.insert(result); return results;
   }
   DMARCRecord::Policy SpamTestDMARC::GetPolicyToApply_(const DMARCRecord &record, DMARCRecord::Policy policy)
   {
      int percent = record.GetPercent(); if (percent >= 100) return policy;
      thread_local std::mt19937 generator(std::random_device{}()); std::uniform_int_distribution<int> distribution(0,99);
      if (distribution(generator) < percent) return policy; return DegradePolicy_(policy);
   }
   DMARCRecord::Policy SpamTestDMARC::DegradePolicy_(DMARCRecord::Policy policy)
   {
      switch (policy) { case DMARCRecord::Policy::Reject: return DMARCRecord::Policy::Quarantine; case DMARCRecord::Policy::Quarantine: return DMARCRecord::Policy::None; }
      return DMARCRecord::Policy::None;
   }
   String SpamTestDMARC::GetHeaderFromDomain_(std::shared_ptr<SpamTestData> pTestData)
   {
      std::shared_ptr<MessageData> data=pTestData->GetMessageData(); if (!data) return "";
      AddresslistParser parser; std::vector<std::shared_ptr<Address> > addresses=parser.ParseList(data->GetFrom()); if (addresses.size()!=1) return "";
      String domain=addresses[0]->sDomainName; domain.ToLower(); return domain;
   }
   bool SpamTestDMARC::IsAuthenticated_(std::shared_ptr<SenderAuthentication> auth,const DMARCRecord &record,const String &from)
   {
      if (auth->GetSPFResult()==SPF::Pass && DMARCEvaluator::IsAligned(auth->GetSPFDomain(),from,record.GetSPFAlignment())) return true;
      for (auto signature:auth->GetDKIMSignatures()) if (signature.second==DKIM::Pass && DMARCEvaluator::IsAligned(String(signature.first),from,record.GetDKIMAlignment())) return true;
      return false;
   }
}
