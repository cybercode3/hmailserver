// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com

#pragma once

#include "IMAPCopy.h"

namespace HM
{
   class IMAPMove : public IMAPCopy
   {
   public:
      IMAPMove();

      virtual IMAPResult DoAction(std::shared_ptr<IMAPConnection> pConnection, int messageIndex, std::shared_ptr<Message> pOldMessage, const std::shared_ptr<IMAPCommandArgument> pArgument) override;

   private:
      IMAPResult EnsureSourcePermissions_(std::shared_ptr<IMAPConnection> pConnection);

      bool source_permissions_checked_;
   };
}
