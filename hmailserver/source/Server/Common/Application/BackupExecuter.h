// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#pragma once


namespace HM
{
   class Domain;
   class IMAPFolders;
   class IMAPFolder;
   class Messages;
   class Message;
   class BackupManager;

   class BackupExecuter
   {
   public:
      BackupExecuter();
      ~BackupExecuter(void);

      bool StartBackup();
      bool StartRestore(std::shared_ptr<Backup> pBackup);

   private:

      void LoadSettings_();

      bool BackupDomains_(XNode *pNode);
      bool BackupDataDirectory_(const String &sDataBackupDir);

      bool PrepareRestoreDataDirectory_(std::shared_ptr<Backup> pBackup, XNode *pBackupNode, String &sPreparedDataDirectory, String &sErrorMessage);
      bool BeginRestoreDataDirectoryCommit_(String &sRollbackDataDirectory, String &sErrorMessage);
      bool CommitRestoreDataDirectory_(const String &sPreparedDataDirectory, const String &sRollbackDataDirectory, String &sErrorMessage);
      void AbortRestoreDataDirectoryCommit_(const String &sPreparedDataDirectory, const String &sRollbackDataDirectory);
      void FinalizeRestoreDataDirectoryCommit_(const String &sRollbackDataDirectory);
      
      int backup_mode_;
      
      // Backup properties
      String destination_;
      String xmlfile_;
   };
}