// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com

#pragma once

namespace HM
{
   class SenderAuthentication;
   class MessageData;

   class AuthenticationResultsHeader
   {
   public:
      static void Apply(std::shared_ptr<MessageData> messageData, std::shared_ptr<SenderAuthentication> senderAuthentication);
      static String BuildValue(std::shared_ptr<SenderAuthentication> senderAuthentication, const String &authservId);
      static AnsiString GetAuthservId(const AnsiString &fieldValue);
   private:
      static void RemoveOwnFields_(std::shared_ptr<MessageData> messageData, const String &authservId);
   };
}
