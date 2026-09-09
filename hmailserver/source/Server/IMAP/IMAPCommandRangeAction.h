// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#pragma once

#include "IMAPCommand.h"
#include "IMAPFolderView.h"

namespace HM
{
   class IMAPFolderView;

   enum class MissingMessagePolicy
   {
      Ignore,
      ReportAfterActing,
      FailBeforeActing,
   };

   class IMAPCommandRangeAction : public IMAPCommand  
   {
   public:
	   IMAPCommandRangeAction();
	   virtual ~IMAPCommandRangeAction();

      void SetIsUID(bool bIsUID);
      
      IMAPResult ExecuteCommand(std::shared_ptr<IMAPConnection> pConnection, std::shared_ptr<IMAPCommandArgument> pArgument) {return IMAPResult();}
      IMAPResult DoForMails(std::shared_ptr<IMAPConnection> pConnection, const String &sMailNos, const std::shared_ptr<IMAPCommandArgument> pArgument);

   protected:

      bool GetIsUID();
      virtual IMAPResult DoAction(std::shared_ptr<IMAPConnection> pConnection, int messageIndex, std::shared_ptr<Message> pMessage, const std::shared_ptr<IMAPCommandArgument> pArgument) = 0;

      virtual bool UsesLiveMessages() const { return false; }
      virtual MissingMessagePolicy GetMissingMessagePolicy() const { return MissingMessagePolicy::Ignore; }

   private:

      std::vector<std::pair<int, IMAPViewEntry>> ResolveTargets_(std::shared_ptr<IMAPFolderView> view, const String &sMailNos);

      bool is_uid_;
     
   };

}
