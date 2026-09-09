// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com

#include "stdafx.h"
#include "IMAPMove.h"

#include <functional>
#include <vector>

#include "IMAPConnection.h"
#include "MessagesContainer.h"
#include "IMAPFolderView.h"

#include "../Common/BO/IMAPFolder.h"
#include "../Common/BO/Message.h"
#include "../Common/Tracking/ChangeNotification.h"
#include "../Common/Tracking/NotificationServer.h"
#include "../Common/Application/Application.h"
#include "../Common/BO/ACLPermission.h"

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace HM
{
   IMAPMove::IMAPMove() :
      source_permissions_checked_(false)
   {
   }

   IMAPResult
      IMAPMove::EnsureSourcePermissions_(std::shared_ptr<IMAPConnection> pConnection)
   {
      if (source_permissions_checked_)
         return IMAPResult();

      if (pConnection->GetCurrentFolderReadOnly())
         return IMAPResult(IMAPResult::ResultNo, "Move command on read-only folder.");

      std::shared_ptr<IMAPFolder> current_folder = pConnection->GetCurrentFolder();
      if (!current_folder)
         return IMAPResult(IMAPResult::ResultNo, "No folder selected.");

      if (!pConnection->CheckPermission(current_folder, ACLPermission::PermissionWriteDeleted))
         return IMAPResult(IMAPResult::ResultBad, "ACL: DeleteMessages permission denied (Required for MOVE command).");

      if (!pConnection->CheckPermission(current_folder, ACLPermission::PermissionExpunge))
         return IMAPResult(IMAPResult::ResultBad, "ACL: Expunge permission denied (Required for MOVE command).");

      source_permissions_checked_ = true;
      return IMAPResult();
   }

   IMAPResult
      IMAPMove::DoAction(std::shared_ptr<IMAPConnection> pConnection, int messageIndex, std::shared_ptr<Message> pOldMessage, const std::shared_ptr<IMAPCommandArgument> pArgument)
   {
      if (!pConnection || !pOldMessage)
         return IMAPResult(IMAPResult::ResultBad, "Invalid parameters");

      IMAPResult permission_result = EnsureSourcePermissions_(pConnection);
      if (permission_result.GetResult() != IMAPResult::ResultOK)
         return permission_result;

      IMAPResult copy_result = IMAPCopy::DoAction(pConnection, messageIndex, pOldMessage, pArgument);
      if (copy_result.GetResult() != IMAPResult::ResultOK)
         return copy_result;

      std::shared_ptr<IMAPFolder> current_folder = pConnection->GetCurrentFolder();
      if (!current_folder)
         return IMAPResult(IMAPResult::ResultNo, "No folder selected.");

      auto messages = MessagesContainer::Instance()->GetMessages(current_folder->GetAccountID(), current_folder->GetID());
      std::set<__int64> ids_to_delete;
      ids_to_delete.insert(pOldMessage->GetID());
      auto deleted_message_ids = messages->DeleteMessagesById(ids_to_delete);

      if (deleted_message_ids.empty())
      {
         std::shared_ptr<IMAPFolder> destination_folder = GetDestinationFolder();
         __int64 destination_message_id = GetLastDestinationMessageID();
         if (destination_folder && destination_message_id > 0)
         {
            auto destination_messages = MessagesContainer::Instance()->GetMessages(destination_folder->GetAccountID(), destination_folder->GetID());
            std::set<__int64> rollback_ids;
            rollback_ids.insert(destination_message_id);
            destination_messages->DeleteMessagesById(rollback_ids);
            MessagesContainer::Instance()->SetFolderNeedsRefresh(destination_folder->GetID());
         }
         ClearLastDestinationMessageID();
         return IMAPResult(IMAPResult::ResultNo, "MOVE failed to remove source message.");
      }

      ClearLastDestinationMessageID();

      auto view = pConnection->GetCurrentFolderView();
      std::vector<int> expunged_sequences;
      if (view)
         expunged_sequences = view->RemoveMessages(deleted_message_ids);
      pConnection->RemoveRecentMessages(deleted_message_ids);

      String response;
      for (int sequence : expunged_sequences)
         response.AppendFormat(_T("* %d EXPUNGE\r\n"), sequence);
      if (!response.IsEmpty())
         pConnection->SendAsciiData(response);

      std::shared_ptr<ChangeNotification> notification =
         std::make_shared<ChangeNotification>(current_folder->GetAccountID(), current_folder->GetID(), ChangeNotification::NotificationMessageDeleted, deleted_message_ids);
      Application::Instance()->GetNotificationServer()->SendNotification(pConnection->GetNotificationClient(), notification);

      return IMAPResult();
   }

}
