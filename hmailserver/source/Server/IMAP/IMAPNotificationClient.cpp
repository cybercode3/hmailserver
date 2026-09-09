// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#include "stdafx.h"

#include "IMAPNotificationClient.h"
#include "IMAPConnection.h"
#include "IMAPStore.h"
#include "IMAPFolderView.h"

#include "../Common/Tracking/ChangeNotification.h"
#include "../common/Tracking/NotificationServer.h"

#include "../Common/BO/Messages.h"
#include "../Common/BO/IMAPFolder.h"

#include "../Common/TCPIP/DisconnectedException.h"

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace HM
{
   IMAPNotificationClient::IMAPNotificationClient() :
      message_change_subscription_id_(0),
      folder_list_change_subscription_id_(0),
      account_id_(0),
      folder_id_(0)
   {

   }

   IMAPNotificationClient::~IMAPNotificationClient()
   {
      try
      {
         if (folder_list_change_subscription_id_ > 0)
         {
            std::shared_ptr<NotificationServer> notificationServer = Application::Instance()->GetNotificationServer();
            notificationServer->UnsubscribeFolderListChanges(account_id_, folder_list_change_subscription_id_);
         }
      }
      catch (...)
      {

      }
   }

   void 
   IMAPNotificationClient::SubscribeMessageChanges(__int64 accountID, __int64 folderID)
   {
      assert(accountID >= 0);
      assert(folderID > 0);

      account_id_ = accountID;
      folder_id_ = folderID;

      std::shared_ptr<NotificationServer> notificationServer = Application::Instance()->GetNotificationServer();
      message_change_subscription_id_ = notificationServer->SubscribeMessageChanges(account_id_, folder_id_, shared_from_this());
   }

   void
   IMAPNotificationClient::UnsubscribeMessageChanges()
   {
      assert(account_id_ >= 0);
      assert(folder_id_ > 0);
      assert(message_change_subscription_id_ > 0);

      // Since we don't want to look at he folder any more,
      // we're not interested in any updates.
      std::shared_ptr<NotificationServer> notificationServer = Application::Instance()->GetNotificationServer();
      notificationServer->UnsubscribeMessageChanges(account_id_, folder_id_, message_change_subscription_id_);

      // If there are cached updates for this folder but the client
      // don't want to look at the folder any more, the cached updates
      // will be gone.
      boost::lock_guard<boost::recursive_mutex> guard(mutex_);
      cached_changes_.clear();
   }

   void 
   IMAPNotificationClient::SetConnection(std::weak_ptr<IMAPConnection> connection)
   //---------------------------------------------------------------------------()
   // DESCRIPTION:
   // Called by the mailbox change notifier when something has happened to the mailbox.
   //---------------------------------------------------------------------------()
   {
      parent_connection_ = connection;
   }

   void 
   IMAPNotificationClient::OnNotification(std::shared_ptr<ChangeNotification> notification)
   //---------------------------------------------------------------------------()
   // DESCRIPTION:
   // Called by the mailbox change notifier when something has happened to the mailbox.
   //---------------------------------------------------------------------------()
   {
      std::shared_ptr<IMAPConnection> parentConnection = parent_connection_.lock();

      if (!parentConnection)
         return;

      // The notifying thread is not this connection's thread. Hold its state lock so the
      // folder cannot be closed between the idling check and the send.
      IMAPConnection::StateLock lock(parentConnection->GetStateMutex());

      if (parentConnection->GetIsIdling())
      {
         try
         {
            SendChangeNotification_(notification);
         }
         catch (DisconnectedException&)
         {
            // We were unable to send the notifications to the client, because he has disconnected.
            // This is normal behavior, and not an error we want to log.
         }
      }
      else
         CacheChangeNotification_(notification);
   }

   //---------------------------------------------------------------------------()
   // DESCRIPTION:
   // Cache this change. We'll send a notification later on.
   //---------------------------------------------------------------------------()
   void 
   IMAPNotificationClient::CacheChangeNotification_(std::shared_ptr<ChangeNotification> pChangeNotification)
   {
      boost::lock_guard<boost::recursive_mutex> guard(mutex_);
      cached_changes_.push_back(pChangeNotification);
   }

   //---------------------------------------------------------------------------()
   // DESCRIPTION:
   // Send a summary of all changes to the client...
   //---------------------------------------------------------------------------()
   void 
   IMAPNotificationClient::SendCachedNotifications(bool send_expunge)
   {
      std::shared_ptr<IMAPConnection> connection = parent_connection_.lock();

      if (!connection)
         return;

      // Lock order is always connection state before mutex_.
      IMAPConnection::StateLock stateLock(connection->GetStateMutex());
      boost::lock_guard<boost::recursive_mutex> guard(mutex_);

      int lastExists = -1;
      int lastRecent = -1;

      auto view = connection->GetCurrentFolderView();
      std::set<__int64> flagMessages;

      for(std::shared_ptr<ChangeNotification> changeNotification : cached_changes_)
      {
         switch (changeNotification->GetType())
         {
         case ChangeNotification::NotificationMessageAdded:
            {
               std::shared_ptr<IMAPFolder> currentFolder = connection->GetCurrentFolder();
               if (!currentFolder)
                  break;
               std::shared_ptr<Messages> pMessages = currentFolder->GetMessages();
               pMessages->Refresh(false);
               if (view)
                  view->AppendNewMessages(pMessages);
               lastExists = view ? view->GetMessageCount() : pMessages->GetCount();
               lastRecent = (int)connection->GetRecentMessageCount();
               break;
            }
         case ChangeNotification::NotificationMessageDeleted:
            {
               if (!send_expunge)
                  break;
               SendEXPUNGE_(changeNotification->GetAffectedMessageIds());
               if (view)
                  lastExists = view->GetMessageCount();
               lastRecent = (int)connection->GetRecentMessageCount();
               break;
            }
         case ChangeNotification::NotificationMessageFlagsChanged:
            {
               for(__int64 messageID : changeNotification->GetAffectedMessageIds())
                  flagMessages.insert(messageID);
               break;
            }
         }
      }

      if (send_expunge && view)
      {
         auto vanished = view->TakeVanished();
         if (!vanished.empty())
         {
            SendEXPUNGE_(vanished);
            lastExists = view->GetMessageCount();
            lastRecent = (int)connection->GetRecentMessageCount();
         }
      }

      if (!flagMessages.empty())
         SendFLAGS_(flagMessages);
      if (lastExists >= 0)
         SendEXISTS_(lastExists);
      if (lastRecent >= 0)
         SendRECENT_(lastRecent);

      std::vector<std::shared_ptr<ChangeNotification> >::iterator iter = cached_changes_.begin();
      for (; iter != cached_changes_.end();)
      {
         std::shared_ptr<ChangeNotification> changeNotification = (*iter);
         if (changeNotification->GetType() == ChangeNotification::NotificationMessageDeleted && !send_expunge)
         {
            iter++;
            continue;
         }
         iter = cached_changes_.erase(iter);
      }
   }

   void 
   IMAPNotificationClient::SendChangeNotification_(std::shared_ptr<ChangeNotification> pChangeNotification)
   {
      std::shared_ptr<IMAPConnection> connection = parent_connection_.lock();
      if (!connection)
         return;

      std::shared_ptr<IMAPFolder> currentFolder = connection->GetCurrentFolder();
      if (!currentFolder)
         return;

      switch (pChangeNotification->GetType())
      {
      case ChangeNotification::NotificationMessageAdded:
         {
            std::shared_ptr<Messages> pMessages = currentFolder->GetMessages();
            auto view = connection->GetCurrentFolderView();
            if (view)
               view->AppendNewMessages(pMessages);
            SendEXISTS_(view ? view->GetMessageCount() : pMessages->GetCount());
            SendRECENT_((int)connection->GetRecentMessageCount());
            break;
         }
      case ChangeNotification::NotificationMessageDeleted:
         {
            SendEXPUNGE_(pChangeNotification->GetAffectedMessageIds());
            auto view = connection->GetCurrentFolderView();
            if (view)
               SendEXISTS_(view->GetMessageCount());
            SendRECENT_((int)connection->GetRecentMessageCount());
            break;
         }
      case ChangeNotification::NotificationMessageFlagsChanged:
         {
            std::set<__int64> affectedMessages;
            for(__int64 messageID : pChangeNotification->GetAffectedMessageIds())
               affectedMessages.insert(messageID);
            SendFLAGS_(affectedMessages);
            break;
         }
      }
   }

   void
   IMAPNotificationClient::SendEXPUNGE_(const std::vector<__int64> & message_ids)
   {
      std::shared_ptr<IMAPConnection> connection = parent_connection_.lock();
      if (!connection)
         return;
      auto view = connection->GetCurrentFolderView();
      if (!view)
         return;
      auto sequences = view->RemoveMessages(message_ids);
      connection->RemoveRecentMessages(message_ids);
      String sResponse;
      for (int sequence : sequences)
         sResponse.AppendFormat(_T("* %d EXPUNGE\r\n"), sequence);
      if (sResponse.IsEmpty())
         return;
      connection->SendAsciiData(sResponse);
   }

   void 
   IMAPNotificationClient::SendFLAGS_(const std::set<__int64> & vecMessages)
   {
      std::shared_ptr<IMAPConnection> connection = parent_connection_.lock();
      if (!connection)
         return;
      std::shared_ptr<IMAPFolder> currentFolder = connection->GetCurrentFolder();
      if (!currentFolder)
         return;
      auto view = connection->GetCurrentFolderView();
      if (!view)
         return;
      for(__int64 messageID : vecMessages)
      {
         int sequence = 0;
         if (!view->GetSequenceByMessageID(messageID, sequence))
            continue;
         std::shared_ptr<Message> pMessage = currentFolder->GetMessages()->GetItemByDBID(messageID);
         if (!pMessage)
            continue;
         connection->SendAsciiData(IMAPStore::GetMessageFlags(pMessage, sequence));
      }
   }

   void 
   IMAPNotificationClient::SendEXISTS_(int iExists)
   {
      std::shared_ptr<IMAPConnection> connection = parent_connection_.lock();
      if (!connection)
         return;

      String sResponse = GenerateExistsString(iExists);
      connection->SendAsciiData(sResponse);
   }

   void 
   IMAPNotificationClient::SendRECENT_(int recent)
   {
      std::shared_ptr<IMAPConnection> connection = parent_connection_.lock();
      if (!connection)
         return;

      String sResponse = GenerateRecentString(recent);

      connection->SendAsciiData(sResponse);
   }

   String
   IMAPNotificationClient::GenerateRecentString(int recent)
   {
      return Formatter::Format("* {0} RECENT\r\n", recent);
   }

   String
   IMAPNotificationClient::GenerateExistsString(int exists)
   {
      return Formatter::Format("* {0} EXISTS\r\n", exists);
   }
}
