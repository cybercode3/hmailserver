// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com
#pragma once
#include "DKIM/DKIM.h"
#include "../../SMTP/SPF/SPF.h"
namespace HM
{
   class SpamTestData;
   class SenderAuthentication
   {
   public:
      enum class DMARCResult { NotEvaluated = 0, Pass = 1, Fail = 2 };
      SenderAuthentication();
      SPF::Result EvaluateSPF(std::shared_ptr<SpamTestData> testData);
      bool GetSPFChecked() const; SPF::Result GetSPFResult() const; String GetSPFDomain() const; String GetSPFExplanation() const;
      DKIM::Result EvaluateDKIM(std::shared_ptr<SpamTestData> testData);
      bool GetDKIMChecked() const; DKIM::Result GetDKIMResult() const;
      const std::vector<std::pair<AnsiString, DKIM::Result> > &GetDKIMSignatures() const;
      void SetDMARCResult(DMARCResult result, const String &headerFromDomain);
      DMARCResult GetDMARCResult() const; String GetDMARCDomain() const;
   private:
      bool spf_checked_; SPF::Result spf_result_; String spf_domain_; String spf_explanation_;
      bool dkim_checked_; DKIM::Result dkim_result_; std::vector<std::pair<AnsiString, DKIM::Result> > dkim_signatures_;
      DMARCResult dmarc_result_; String dmarc_domain_;
   };
}
