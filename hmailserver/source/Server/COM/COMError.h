// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#pragma once

#include "../Common/Application/ErrorManager.h"
#include "../Common/Util/ScopedSETranslator.h"

class COMError
{
public:
   COMError(void);
   ~COMError(void);

   static HRESULT GenerateGenericMessage();
   static HRESULT GenerateError(HM::String sDescription);
   template <typename Func>
   static HRESULT Guard(const char *source, Func func)
   {
      try
      {
         HM::ScopedSETranslator se_translator;
         return func();
      }
      catch (std::exception const &e)
      {
         HM::ErrorManager::Instance()->ReportError(HM::ErrorManager::Medium, 4231, source,
            "Unhandled exception in COM call.", e);
         return GenerateGenericMessage();
      }
      catch (...)
      {
         HM::ErrorManager::Instance()->ReportError(HM::ErrorManager::Medium, 4231, source,
            "Unknown exception in COM call.");
         return GenerateGenericMessage();
      }
   }

};
