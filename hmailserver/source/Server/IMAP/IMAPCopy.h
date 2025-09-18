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

      __int64 GetLastDestinationMessageID() const { return last_destination_message_id_; }
      void ClearLastDestinationMessageID() { last_destination_message_id_ = 0; }
      std::shared_ptr<IMAPFolder> GetDestinationFolder() const { return destination_folder_; }


   private:
      String BuildUidSetString_(const std::vector<unsigned int> &uids) const;

      std::shared_ptr<IMAPFolder> destination_folder_;
      bool destination_selectable_;
      std::vector<unsigned int> source_uids_;
      std::vector<unsigned int> destination_uids_;
      __int64 last_destination_message_id_;

   };
}
