// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#include "stdafx.h"
#include "IMAPCopy.h"
#include "IMAPConnection.h"
#include "../Common/BO/Message.h"
#include "../Common/BO/Account.h"
#include "../Common/BO/IMAPFolder.h"
#include "../Common/Persistence/PersistentMessage.h"
#include "IMAPSimpleCommandParser.h"
#include "../Common/BO/ACLPermission.h"
#include "../Common/Tracking/ChangeNotification.h"


#include "MessagesContainer.h"

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace HM
{
   IMAPCopy::IMAPCopy() :
      destination_selectable_(false),
      last_destination_message_id_(0)
   {

   }


   IMAPResult
   IMAPCopy::DoAction(std::shared_ptr<IMAPConnection> pConnection, int messageIndex, std::shared_ptr<Message> pOldMessage, const std::shared_ptr<IMAPCommandArgument> pArgument)
   {
      last_destination_message_id_ = 0;

      if (!pArgument || !pOldMessage)
         return IMAPResult(IMAPResult::ResultBad, "Invalid parameters");
      
      if (!destination_folder_)
      {
         std::shared_ptr<IMAPSimpleCommandParser> pParser = std::shared_ptr<IMAPSimpleCommandParser>(new IMAPSimpleCommandParser());

         pParser->Parse(pArgument);

         if (pParser->WordCount() <= 0)
            return IMAPResult(IMAPResult::ResultNo, "The command requires parameters.");

         String sFolderName;
         if (pParser->Word(0)->Clammerized())
            sFolderName = pArgument->Literal(0);
         else
         {
            sFolderName = pParser->Word(0)->Value();
            IMAPFolder::UnescapeFolderString(sFolderName);
         }

         std::shared_ptr<IMAPFolder> parsed_folder = pConnection->GetFolderByFullPath(sFolderName);
         if (!parsed_folder)
            return IMAPResult(IMAPResult::ResultBad, "The folder could not be found.");

         destination_folder_ = parsed_folder;
         destination_selectable_ = pConnection->CheckPermission(destination_folder_, ACLPermission::PermissionRead);
      }

      std::shared_ptr<IMAPFolder> pFolder = destination_folder_;

      std::shared_ptr<const Account> pAccount = pConnection->GetAccount();

      if (!pFolder->IsPublicFolder())
      {
         if (!pAccount->SpaceAvailable(pOldMessage->GetSize()))
            return IMAPResult(IMAPResult::ResultNo, "Your quota has been exceeded.");
      }

      // Check if the user has permission to copy to this destination folder
      if (!pConnection->CheckPermission(pFolder, ACLPermission::PermissionInsert))
         return IMAPResult(IMAPResult::ResultBad, "ACL: Insert permission denied (Required for COPY command).");

      std::shared_ptr<Message> pNewMessage = PersistentMessage::CopyToIMAPFolder(pAccount, pOldMessage, pFolder);

      if (!pNewMessage)
         return IMAPResult(IMAPResult::ResultBad, "Failed to copy message");

      last_destination_message_id_ = pNewMessage->GetID();

      // Check if the user has access to set the Seen flag, otherwise 
      if (!pConnection->CheckPermission(pFolder, ACLPermission::PermissionWriteSeen))
         pNewMessage->SetFlagSeen(false);  

      if (!PersistentMessage::SaveObject(pNewMessage))
         return IMAPResult(IMAPResult::ResultBad, "Failed to save copy of message.");

      MessagesContainer::Instance()->SetFolderNeedsRefresh(pFolder->GetID());

      if (pOldMessage->GetUID() > 0)
         source_uids_.push_back(pOldMessage->GetUID());

      if (pNewMessage->GetUID() > 0)
         destination_uids_.push_back(pNewMessage->GetUID());

      // Set a delayed notification so that the any IMAP idle client is notified when this
      // command has been finished.
      std::shared_ptr<ChangeNotification> pNotification =
         std::shared_ptr<ChangeNotification>(new ChangeNotification(pFolder->GetAccountID(), pFolder->GetID(), ChangeNotification::NotificationMessageAdded));

      pConnection->SetDelayedChangeNotification(pNotification);

      return IMAPResult();
   }

   String
   IMAPCopy::BuildUidSetString_(const std::vector<unsigned int> &uids) const
   {
      String result;
      bool first = true;

      for (unsigned int uid : uids)
      {
         if (uid == 0)
            continue;

         if (!first)
            result += ",";

         String uid_string;
         uid_string.Format(_T("%u"), uid);
         result += uid_string;

         first = false;
      }

      return result;
   }

   bool
   IMAPCopy::GetCopyUIDResponse(String &response) const
   {
      if (!destination_folder_)
         return false;

      if (!destination_selectable_)
         return false;

      if (source_uids_.empty() || destination_uids_.empty())
         return false;

      if (source_uids_.size() != destination_uids_.size())
         return false;

      String source_set = BuildUidSetString_(source_uids_);
      String destination_set = BuildUidSetString_(destination_uids_);

      if (source_set.IsEmpty() || destination_set.IsEmpty())
         return false;

      int uid_validity = destination_folder_->GetCreationTime().ToInt();

      String response_code;
      response_code.Format(_T("COPYUID %d %s %s"), uid_validity, source_set.c_str(), destination_set.c_str());

      response = response_code;
      return true;
   }
}
