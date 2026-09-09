// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com
#pragma once
#include "DMARCRecord.h"
namespace HM
{
   class DMARCEvaluator
   {
   public:
      static String GetOrganizationalDomain(const String &domain);
      static bool IsAligned(const String &authenticatedDomain, const String &headerFromDomain, DMARCRecord::Alignment alignment);
      static DMARCRecord::Policy GetApplicablePolicy(const DMARCRecord &record, const String &headerFromDomain, const String &policyDomain);
   };
}
