// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#pragma once

#include "IMAPCommandRangeAction.h"

namespace HM
{
   class IMAPFolder;
}

namespace HM
{
   class IMAPCopy  : public IMAPCommandRangeAction
   {
   public:
           IMAPCopy();

      virtual IMAPResult DoAction(std::shared_ptr<IMAPConnection> pConnection, int messageIndex, std::shared_ptr<Message> pOldMessage, const std::shared_ptr<IMAPCommandArgument> pArgument);

      bool GetCopyUIDResponse(String &response) const;


   private:
      String BuildUidSetString_(const std::vector<unsigned int> &uids) const;

      std::shared_ptr<IMAPFolder> destination_folder_;
      bool destination_selectable_;
      std::vector<unsigned int> source_uids_;
      std::vector<unsigned int> destination_uids_;

   };
}
