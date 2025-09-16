// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#include "stdafx.h"
#include "IMAPCommandUID.h"
#include "IMAPConnection.h"
#include "IMAPSimpleCommandParser.h"


#include "IMAPFetch.h"
#include "IMAPCopy.h"
#include "IMAPStore.h"
#include "IMAPCommandSearch.h"
#include "MessagesContainer.h"

#include "../Common/BO/IMAPFolder.h"
#include "../Common/BO/Message.h"
#include "../Common/BO/ACLPermission.h"
#include "../Common/Application/Application.h"
#include "../Common/Tracking/ChangeNotification.h"
#include "../Common/Tracking/NotificationServer.h"

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace HM
{
   IMAPCommandUID::IMAPCommandUID()
   {

   }

   IMAPCommandUID::~IMAPCommandUID()
   {

   }


   IMAPResult
   IMAPCommandUID::ExecuteCommand(std::shared_ptr<IMAPConnection> pConnection, std::shared_ptr<IMAPCommandArgument> pArgument)
   {
      if (!pConnection->IsAuthenticated())
         return IMAPResult(IMAPResult::ResultNo, "Authenticate first");

      String sTag = pArgument->Tag();
      String sCommand = pArgument->Command();

      if (!pConnection->GetCurrentFolder())
         return IMAPResult(IMAPResult::ResultNo, "No folder selected.");

      std::shared_ptr<IMAPSimpleCommandParser> pParser = std::shared_ptr<IMAPSimpleCommandParser>(new IMAPSimpleCommandParser());

      pParser->Parse(pArgument);
      
      if (pParser->WordCount() < 2)
         return IMAPResult(IMAPResult::ResultBad, "Command requires at least 1 parameter.");

      String sTypeOfUID = pParser->Word(1)->Value();

      if (sTypeOfUID.CompareNoCase(_T("FETCH")) == 0)
      {
         if (pParser->WordCount() < 4)
            return IMAPResult(IMAPResult::ResultBad, "Command requires at least 3 parameters.");

         command_ = std::shared_ptr<IMAPFetch>(new IMAPFetch());
      }
      else if (sTypeOfUID.CompareNoCase(_T("COPY")) == 0)
      {

         if (pParser->WordCount() < 4)
            return IMAPResult(IMAPResult::ResultBad, "Command requires at least 3 parameters.");

         command_ = std::shared_ptr<IMAPCopy>(new IMAPCopy());
      }
      else if (sTypeOfUID.CompareNoCase(_T("STORE")) == 0)
      {
         if (pParser->WordCount() < 4)
            return IMAPResult(IMAPResult::ResultBad, "Command requires at least 3 parameters.");

         command_ = std::shared_ptr<IMAPStore>(new IMAPStore());
      }
      else if (sTypeOfUID.CompareNoCase(_T("SEARCH")) == 0)
      {
         std::shared_ptr<IMAPCommandSEARCH> pCommand = std::shared_ptr<IMAPCommandSEARCH> (new IMAPCommandSEARCH(false));
         pCommand->SetIsUID();
         IMAPResult result = pCommand->ExecuteCommand(pConnection, pArgument);

         if (result.GetResult() == IMAPResult::ResultOK)
            pConnection->SendAsciiData(sTag + " OK UID completed\r\n");

         return result;
      }
      else if (sTypeOfUID.CompareNoCase(_T("SORT")) == 0)
      {
         std::shared_ptr<IMAPCommandSEARCH> pCommand = std::shared_ptr<IMAPCommandSEARCH> (new IMAPCommandSEARCH(true));
         pCommand->SetIsUID();
         IMAPResult result = pCommand->ExecuteCommand(pConnection, pArgument);

         if (result.GetResult() == IMAPResult::ResultOK)
            pConnection->SendAsciiData(sTag + " OK UID completed\r\n");

         return result;
      }
      else if (sTypeOfUID.CompareNoCase(_T("EXPUNGE")) == 0)
      {
         if (pParser->WordCount() < 3)
            return IMAPResult(IMAPResult::ResultBad, "Command requires at least 2 parameters.");

         if (pParser->WordCount() > 3)
            return IMAPResult(IMAPResult::ResultBad, "Too many parameters.");

         String sequence_set = pParser->Word(2)->Value();

         if (sequence_set.IsEmpty())
            return IMAPResult(IMAPResult::ResultBad, "No mail number specified");

         if (!StringParser::ValidateString(sequence_set, _T("01234567890,.:*")))
            return IMAPResult(IMAPResult::ResultBad, "Incorrect mail number");

         return UIDExpunge_(pConnection, pArgument, sequence_set);
      }


      if (!command_)
         return IMAPResult(IMAPResult::ResultBad, "Bad command.");

      command_->SetIsUID(true);

      // Copy the first word containing the message sequence
      long lSecWordStartPos = sCommand.Find(_T(" "), 5);
      if (lSecWordStartPos == -1)
         return IMAPResult(IMAPResult::ResultBad, "No mail number specified");

      lSecWordStartPos++;

      long lSecWordEndPos = sCommand.Find(_T(" "), lSecWordStartPos);
      String sMailNo;
      String sShowPart;

      if (lSecWordEndPos == -1)
      {
         sMailNo = sCommand.Mid(lSecWordStartPos);
      }
      else
      {
         long lSecWordLength = lSecWordEndPos - lSecWordStartPos;
         sMailNo = sCommand.Mid(lSecWordStartPos, lSecWordLength);
         sShowPart = sCommand.Mid(lSecWordEndPos + 1);
      }

      if (sMailNo.IsEmpty())
         return IMAPResult(IMAPResult::ResultBad, "No mail number specified");

      if (!StringParser::ValidateString(sMailNo, "01234567890,.:*"))
         return IMAPResult(IMAPResult::ResultBad, "Incorrect mail number");

      // Set the command to execute as argument
      pArgument->Command(sShowPart);

      // Execute the command. If we have gotten this far, it means that the syntax
      // of the command is correct. If we fail now, we should return NO.
      IMAPResult result = command_->DoForMails(pConnection, sMailNo, pArgument);

      if (result.GetResult() == IMAPResult::ResultOK)
      {
         String completion = pArgument->Tag() + " OK";

         std::shared_ptr<IMAPCopy> copy_command = std::dynamic_pointer_cast<IMAPCopy>(command_);
         if (copy_command)
         {
            String copy_uid_response;
            if (copy_command->GetCopyUIDResponse(copy_uid_response))
            {
               completion += " [";
               completion += copy_uid_response;
               completion += "]";
            }
         }

         completion += " UID completed\r\n";
         pConnection->SendAsciiData(completion);
      }

      return result;
   }

   IMAPResult
   IMAPCommandUID::UIDExpunge_(std::shared_ptr<IMAPConnection> pConnection, std::shared_ptr<IMAPCommandArgument> pArgument, const String &sequence_set)
   {
      if (pConnection->GetCurrentFolderReadOnly())
         return IMAPResult(IMAPResult::ResultNo, "Expunge command on read-only folder.");

      std::shared_ptr<IMAPFolder> current_folder = pConnection->GetCurrentFolder();

      if (!current_folder)
         return IMAPResult(IMAPResult::ResultNo, "No folder selected.");

      if (!pConnection->CheckPermission(current_folder, ACLPermission::PermissionExpunge))
         return IMAPResult(IMAPResult::ResultBad, "ACL: Expunge permission denied (Required for EXPUNGE command).");

      std::vector<String> sequence_parts = StringParser::SplitString(sequence_set, _T(","));
      unsigned int highest_uid = current_folder->GetCurrentUID();

      std::vector<__int64> expunged_message_ids;
      std::vector<__int64> expunged_message_indexes;

      std::function<bool(int, std::shared_ptr<Message>)> filter =
         [&expunged_message_ids, &expunged_message_indexes, &sequence_parts, highest_uid](int index, std::shared_ptr<Message> message) -> bool
      {
         if (!message->GetFlagDeleted())
            return false;

         unsigned int uid = message->GetUID();
         if (uid == 0)
            return false;

         if (!UIDMatchesSequence_(uid, sequence_parts, highest_uid))
            return false;

         expunged_message_indexes.push_back(index);
         expunged_message_ids.push_back(message->GetID());
         return true;
      };

      auto messages = MessagesContainer::Instance()->GetMessages(current_folder->GetAccountID(), current_folder->GetID());
      messages->DeleteMessages(filter);

      String response;
      for (__int64 message_index : expunged_message_indexes)
      {
         String line;
         line.Format(_T("* %d EXPUNGE\r\n"), (int) message_index);
         response += line;
      }

      pConnection->SendAsciiData(response);

      if (!expunged_message_ids.empty())
      {
         auto &recent_messages = pConnection->GetRecentMessages();

         for (__int64 message_id : expunged_message_ids)
         {
            auto recent_it = recent_messages.find(message_id);
            if (recent_it != recent_messages.end())
               recent_messages.erase(recent_it);
         }

         std::shared_ptr<ChangeNotification> notification =
            std::shared_ptr<ChangeNotification>(new ChangeNotification(current_folder->GetAccountID(), current_folder->GetID(), ChangeNotification::NotificationMessageDeleted, expunged_message_indexes));

         Application::Instance()->GetNotificationServer()->SendNotification(pConnection->GetNotificationClient(), notification);
      }

      pConnection->SendAsciiData(pArgument->Tag() + " OK UID EXPUNGE completed\r\n");

      return IMAPResult();
   }

   unsigned int
   IMAPCommandUID::ParseUIDValue_(String value, unsigned int highest_uid)
   {
      value.Trim();

      if (value.IsEmpty())
         return 0;

      if (value == _T("*"))
         return highest_uid;

      return (unsigned int) _ttoi(value);
   }

   bool
   IMAPCommandUID::UIDMatchesSequence_(unsigned int uid, const std::vector<String> &sequence_parts, unsigned int highest_uid)
   {
      for (String token : sequence_parts)
      {
         token.Trim();

         if (token.IsEmpty())
            continue;

         int colon_position = token.Find(_T(":"));

         if (colon_position >= 0)
         {
            String start_part = token.Mid(0, colon_position);
            String end_part = token.Mid(colon_position + 1);

            unsigned int start_value = ParseUIDValue_(start_part, highest_uid);
            unsigned int end_value = ParseUIDValue_(end_part, highest_uid);

            if (end_part.IsEmpty())
               end_value = start_value;

            unsigned int lower = start_value;
            unsigned int upper = end_value;

            if (upper < lower)
            {
               lower = end_value;
               upper = start_value;
            }

            if (uid >= lower && uid <= upper)
               return true;
         }
         else
         {
            unsigned int value = ParseUIDValue_(token, highest_uid);
            if (value == uid)
               return true;
         }
      }

      return false;
   }
}