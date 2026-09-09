// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com
#pragma once
namespace HM
{
   class PublicSuffixList : public Singleton<PublicSuffixList>
   {
   public:
      void Initialize();
      String GetRegistrableDomain(const String &domain) const;
      bool GetRegistrableDomain(const String &domain, String &registrableDomain) const;
   private:
      size_t GetPublicSuffixLabelCount_(const std::vector<String> &labels) const;
      static String JoinLabels_(const std::vector<String> &labels, size_t firstLabel);
      std::set<String> rules_, wildcard_rules_, exception_rules_;
   };
}
