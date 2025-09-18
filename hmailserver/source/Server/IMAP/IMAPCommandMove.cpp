// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com

#include "stdafx.h"
#include "IMAPCommandMove.h"

#include "IMAPMove.h"
#include "IMAPConnection.h"

#include "../Common/BO/IMAPFolder.h"

#include "IMAPSimpleCommandParser.h"

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace HM
{
   IMAPCommandMOVE::IMAPCommandMOVE()
   {
   }

   IMAPCommandMOVE::~IMAPCommandMOVE()
   {
   }

   IMAPResult
   IMAPCommandMOVE::ExecuteCommand(std::shared_ptr<IMAPConnection> pConnection, std::shared_ptr<IMAPCommandArgument> pArgument)
   {
      if (!pConnection->IsAuthenticated())
         return IMAPResult(IMAPResult::ResultNo, "Authenticate first");

      if (!pConnection->GetCurrentFolder())
         return IMAPResult(IMAPResult::ResultNo, "No folder selected.");

      std::shared_ptr<IMAPMove> move_command = std::shared_ptr<IMAPMove>(new IMAPMove());
      move_command->SetIsUID(false);

      std::shared_ptr<IMAPSimpleCommandParser> parser = std::shared_ptr<IMAPSimpleCommandParser>(new IMAPSimpleCommandParser());
      parser->Parse(pArgument);

      if (parser->ParamCount() != 2)
         return IMAPResult(IMAPResult::ResultBad, "Command requires 2 parameters.\r\n");

      String mail_sequence = parser->GetParamValue(pArgument, 0);
      String folder_name = parser->GetParamValue(pArgument, 1);

      pArgument->Command("\"" + folder_name + "\"");

      std::shared_ptr<IMAPFolder> destination_folder = pConnection->GetFolderByFullPath(folder_name);
      if (!destination_folder)
         return IMAPResult(IMAPResult::ResultNo, "Can't find mailbox with that name.\r\n");

      IMAPResult result = move_command->DoForMails(pConnection, mail_sequence, pArgument);

      if (result.GetResult() == IMAPResult::ResultOK)
      {
         String completion = pArgument->Tag() + " OK";

         String copy_uid_response;
         if (move_command->GetCopyUIDResponse(copy_uid_response))
         {
            completion += " [";
            completion += copy_uid_response;
            completion += "]";
         }

         completion += " MOVE completed\r\n";
         pConnection->SendAsciiData(completion);
      }

      return result;
   }
}
