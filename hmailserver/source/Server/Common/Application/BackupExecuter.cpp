// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#include "StdAfx.h"

#include "BackupExecuter.h"
#include "Backup.h"

#include "..\Util\Utilities.h"
#include "..\Util\Time.h"
#include "..\BO\Domains.h"
#include "..\BO\Domain.h"
#include "..\BO\IMAPFolders.h"
#include "..\BO\Accounts.h"
#include "..\BO\Aliases.h"
#include "..\BO\DomainAliases.h"
#include "..\BO\DistributionLists.h"

#include "..\Persistence\PersistentMessage.h"
#include "..\Util\Compression.h"
#include "..\Util\GUIDCreator.h"
#include "..\Util\ServiceManager.h"

#include "BackupManager.h"
#include "ACLManager.h"
#include "Reinitializator.h"

#include "../../IMAP/IMAPConfiguration.h"

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace HM
{
   BackupExecuter::BackupExecuter()
   {
      backup_mode_ = 0;
   }

   BackupExecuter::~BackupExecuter(void)
   {

   }

   void 
   BackupExecuter::LoadSettings_()
   {
      destination_ = Configuration::Instance()->GetBackupDestination();
      if (destination_.Right(1) == _T("\\"))
         destination_ = destination_.Left(destination_.GetLength() - 1);

      backup_mode_ = Configuration::Instance()->GetBackupOptions();
   }

   bool 
   BackupExecuter::StartBackup()
   {
      Logger::Instance()->LogBackup("Loading backup settings....");         

      LoadSettings_();

      // Special temp setting to skip files during backup/restore while still storing/restoring db file/message info.
      bool bMessagesDBOnly = IniFileSettings::Instance()->GetBackupMessagesDBOnly();


      if (backup_mode_ & Backup::BOMessages)
      {
         if (!PersistentMessage::GetAllMessageFilesAreInDataFolder())
         {
            Application::Instance()->GetBackupManager()->OnBackupFailed("All messages are not located in the data folder.");
            return false;
         }
      }

      if (!FileUtilities::Exists(destination_))
      {
         Application::Instance()->GetBackupManager()->OnBackupFailed("The specified backup directory is not accessible: " + destination_);
         return false;
      }


      String sTime = Time::GetCurrentDateTime();
      sTime.Replace(_T(":"), _T(""));

      // Generate name for zip file. We always create zip
      // file
      String sZipFile;
      sZipFile.Format(_T("%s\\HMBackup %s.7z"), destination_.c_str(), sTime.c_str());

      String sXMLFile;
      sXMLFile.Format(_T("%s\\hMailServerBackup.xml"), destination_.c_str());

      // Keep each uncompressed backup in its own data directory so multiple
      // raw backups can coexist in the same destination. Compressed backups
      // use a unique working directory, while retaining the internal
      // DataBackup folder name expected by existing restore code.
      String sDataBackupFolderName;
      String sDataBackupDir;
      String sDataBackupCleanupDir;

      if (backup_mode_ & Backup::BOCompression)
      {
         sDataBackupCleanupDir = destination_ + "\\BackupTemp-" + GUIDCreator::GetGUID();
         sDataBackupDir = sDataBackupCleanupDir + "\\DataBackup";
      }
      else
      {
         sDataBackupFolderName.Format(_T("DataBackup %s"), sTime.c_str());
         sDataBackupDir = destination_ + "\\" + sDataBackupFolderName;
         sDataBackupCleanupDir = sDataBackupDir;
      }

      // Backup all properties.
      XDoc oDoc; 

      XNode *pBackupNode = oDoc.AppendChild(_T("Backup"));
      XNode *pBackupInfoNode = pBackupNode->AppendChild(_T("BackupInformation"));

      // Store backup mode
      pBackupInfoNode->AppendAttr(_T("Mode"), StringParser::IntToString(backup_mode_));
      pBackupInfoNode->AppendAttr(_T("Version"), Application::Instance()->GetVersionNumber());

      // Backup business objects
      if (backup_mode_ & Backup::BODomains)
      {
         Logger::Instance()->LogBackup("Backing up domains...");

         if (!BackupDomains_(pBackupNode))
         {
            Application::Instance()->GetBackupManager()->OnBackupFailed("Could not backup domains.");
            return false;
         }
         
         // Backup message files
         if (backup_mode_ & Backup::BOMessages && !bMessagesDBOnly)
         {
            Logger::Instance()->LogBackup("Backing up data directory...");
            if (!BackupDataDirectory_(sDataBackupDir))
            {
               Application::Instance()->GetBackupManager()->OnBackupFailed("Could not backup data directory.");
               return false;
            }


         }
      }

      // Save information in the XML file where messages can be found.
      if (backup_mode_ & Backup::BOMessages)
      {
         XNode *pMessageFile = pBackupInfoNode->AppendChild(_T("DataFiles"));

         if (backup_mode_ & Backup::BOCompression)
         {
            pMessageFile->AppendAttr(_T("Format"), _T("7z"));
            // Do not store the archive size here. The archive has not been
            // populated yet, so the old value was always stale/incorrect.
         }
         else
         {
            pMessageFile->AppendAttr(_T("Format"), _T("Raw"));
            pMessageFile->AppendAttr(_T("FolderName"), sDataBackupFolderName);
         }
      }

      if (backup_mode_ & Backup::BOSettings)
      {
         Logger::Instance()->LogBackup("Backing up settings...");
         Configuration::Instance()->XMLStore(pBackupNode);
      }


      Logger::Instance()->LogBackup(_T("Writing XML file..."));
      String sXMLData = oDoc.GetXML();
      if (!FileUtilities::WriteToFile(sXMLFile, sXMLData, true))
      {
         Application::Instance()->GetBackupManager()->OnBackupFailed("Could not write to the XML file.");
         return false;
      }

      // Compress the XML file
      Compression oComp;
      if (!oComp.AddFile(sZipFile, sXMLFile))
      {
         FileUtilities::DeleteFile(sXMLFile);
         FileUtilities::DeleteFile(sZipFile);
         if (FileUtilities::Exists(sDataBackupCleanupDir))
            FileUtilities::DeleteDirectory(sDataBackupCleanupDir, true);

         Application::Instance()->GetBackupManager()->OnBackupFailed("Could not add the backup XML file to the compressed archive.");
         return false;
      }

      // Delete the XML file
      FileUtilities::DeleteFile(sXMLFile);

      // Should we compress the message files?
      if (backup_mode_ & Backup::BOMessages && 
          backup_mode_ & Backup::BOCompression && !bMessagesDBOnly)
      {
         Logger::Instance()->LogBackup("Compressing message files...");
         
         if (backup_mode_ & Backup::BOMessages)
         {
            if (!oComp.AddDirectory(sZipFile, sDataBackupDir + "\\"))
            {
               FileUtilities::DeleteFile(sZipFile);
               FileUtilities::DeleteDirectory(sDataBackupCleanupDir, true);
               Application::Instance()->GetBackupManager()->OnBackupFailed("Could not add the message data directory to the compressed archive.");
               return false;
            }
         }

         // Since the files are now compressed, we can deleted
         // the data backup directory
         if (!FileUtilities::DeleteDirectory(sDataBackupCleanupDir, true))
         {
            Application::Instance()->GetBackupManager()->OnBackupFailed("Could not delete temporary backup files from the destination directory.");
            return false;
         }
       }

      Application::Instance()->GetBackupManager()->OnBackupCompleted();

      return true;
   }

   bool 
   BackupExecuter::BackupDataDirectory_(const String &sDataBackupDir)
   {
      String sDataDir = IniFileSettings::Instance()->GetDataDirectory();

      String errorMessage;

      bool bResult = FileUtilities::CopyDirectory(sDataDir, sDataBackupDir, errorMessage);
      if (!bResult)
      {
         Logger::Instance()->LogBackup("Failed to copy data directory. Details: " + errorMessage);
         return bResult;
      }

      bResult = FileUtilities::DeleteFilesInDirectory(sDataBackupDir);

      if (!bResult)
      {
         Logger::Instance()->LogBackup("Failed to delete files in backup root directory. Please see hMailServer error log.");
      }
      
      return bResult;
   }

   bool 
   BackupExecuter::BackupDomains_(XNode *pBackupNode)
   {
      std::shared_ptr<Domains> pDomains = std::shared_ptr<Domains>(new Domains);
      pDomains->Refresh();
      pDomains->XMLStore(pBackupNode, backup_mode_);

      return true;
   }

   bool
   BackupExecuter::StartRestore(std::shared_ptr<Backup> pBackup)
   {
      bool bMessagesDBOnly = IniFileSettings::Instance()->GetBackupMessagesDBOnly();

      Logger::Instance()->LogBackup("Reading XML file...");
      String sZipFile = pBackup->GetBackupFile();

      String sTempDir = IniFileSettings::Instance()->GetTempDirectory();
      String sXMLFile = sTempDir + "\\hMailServerBackup.xml";
      FileUtilities::DeleteFile(sXMLFile);

      Compression oComp;
      if (!oComp.Uncompress(sZipFile, sTempDir, "hMailServerBackup.xml"))
      {
         String sErrorMessage = Formatter::Format("Unable to uncompress hMailServerBackup.xml from {0} to {1}. Please confirm that hMailServer has permissions to {0} and {1}.", sZipFile, sTempDir);
         Application::Instance()->GetBackupManager()->OnBackupFailed(sErrorMessage);
         return false;
      }

      String sXMLData = FileUtilities::ReadCompleteTextFile(sXMLFile);
      if (sXMLData.IsEmpty())
      {
         String sErrorMessage = Formatter::Format("The file {0} could not be read.", sXMLFile);
         Application::Instance()->GetBackupManager()->OnBackupFailed(sErrorMessage);
         return false;
      }

      XDoc oDoc;
      oDoc.Load(sXMLData);

      FileUtilities::DeleteFile(sXMLFile);

      String sBackup = "Backup";
      XNode *pBackupNode = oDoc.GetChild(sBackup);
      if (!pBackupNode)
      {
         String sErrorMessage = "The supplied XML file is not a valid hMailServer backup file";
         Application::Instance()->GetBackupManager()->OnBackupFailed(sErrorMessage);
         return false;
      }

      int iRestoreOptions = pBackup->GetRestoreOptions();

      String sPreparedDataDirectory;
      String sRollbackDataDirectory;
      const bool bRestoreDataDirectory =
         (iRestoreOptions & Backup::BODomains) &&
         (iRestoreOptions & Backup::BOMessages) &&
         !bMessagesDBOnly;

      if (bRestoreDataDirectory)
      {
         Logger::Instance()->LogBackup("Preparing and validating data directory restore...");

         String sErrorMessage;
         if (!PrepareRestoreDataDirectory_(pBackup, pBackupNode, sPreparedDataDirectory, sErrorMessage))
         {
            Application::Instance()->GetBackupManager()->OnBackupFailed(sErrorMessage);
            return false;
         }
      }

    
      if (iRestoreOptions & Backup::BODomains)
      {
         if (bRestoreDataDirectory)
         {
            String sErrorMessage;
            if (!BeginRestoreDataDirectoryCommit_(sRollbackDataDirectory, sErrorMessage))
            {
               FileUtilities::DeleteDirectory(sPreparedDataDirectory, true);
               Application::Instance()->GetBackupManager()->OnBackupFailed(sErrorMessage);
               return false;
            }
         }

         // First drop all domains. We need to do this prior to restoring
         // the data directory. When we drop the domains, we will also
         // drop the domain folders from the data directory. If we do this
         // in the wrong order, we'll hence first restore the data directory
         // and then drop it.
         std::shared_ptr<Domains> pDomains = std::shared_ptr<Domains>(new Domains);

         pDomains->Refresh();
         if (!bMessagesDBOnly) 
         {
            if (!pDomains->DeleteAll())
            {
               if (bRestoreDataDirectory)
                  AbortRestoreDataDirectoryCommit_(sPreparedDataDirectory, sRollbackDataDirectory);

               String sErrorMessage = "Unable to remove existing domains before restore. Existing message data was preserved.";
               Application::Instance()->GetBackupManager()->OnBackupFailed(sErrorMessage);
               return false;
            }
         }

         // We need to do the same with public folders.
         if (iRestoreOptions & Backup::BOSettings && !bMessagesDBOnly)
         {
            if (!Configuration::Instance()->GetIMAPConfiguration()->GetPublicFolders()->DeleteAll())
            {
               if (bRestoreDataDirectory)
                  AbortRestoreDataDirectoryCommit_(sPreparedDataDirectory, sRollbackDataDirectory);

               String sErrorMessage = "Unable to remove existing public folders before restore. Existing message data was preserved.";
               Application::Instance()->GetBackupManager()->OnBackupFailed(sErrorMessage);
               return false;
            }
         }
         
         // Should we restore messages as well?
         if (bRestoreDataDirectory)
         {
            Logger::Instance()->LogBackup("Restoring data directory...");

            String sErrorMessage;
            if (!CommitRestoreDataDirectory_(sPreparedDataDirectory, sRollbackDataDirectory, sErrorMessage))
            {
               Application::Instance()->GetBackupManager()->OnBackupFailed(sErrorMessage);
               return false;
            }
         }

         Logger::Instance()->LogBackup("Restoring domains...");

         if (!pDomains->XMLLoad(pBackupNode, iRestoreOptions))
         {
            String sErrorMessage = "Restore of domains failed. Please check hMailServer log.";
            Logger::Instance()->LogBackup(sErrorMessage);

            if (!sRollbackDataDirectory.IsEmpty() && FileUtilities::Exists(sRollbackDataDirectory))
               Logger::Instance()->LogBackup("The pre-restore data directory was preserved for recovery at: " + sRollbackDataDirectory);

            Application::Instance()->GetBackupManager()->OnBackupFailed(sErrorMessage);
            
            return false;
         }
      }

      // Backup settings last since they may be referring to objects in the domains.
      if (iRestoreOptions & Backup::BOSettings)
      {
         Logger::Instance()->LogBackup("Restoring settings...");
         if (!Configuration::Instance()->XMLLoad(pBackupNode, iRestoreOptions))
         {
            String sErrorMessage = "Restore of settings failed. Please check hMailServer log.";
            Logger::Instance()->LogBackup(sErrorMessage);

            if (!sRollbackDataDirectory.IsEmpty() && FileUtilities::Exists(sRollbackDataDirectory))
               Logger::Instance()->LogBackup("The pre-restore data directory was preserved for recovery at: " + sRollbackDataDirectory);

            Application::Instance()->GetBackupManager()->OnBackupFailed(sErrorMessage);
            return false;
         }
      }

      if (bRestoreDataDirectory)
         FinalizeRestoreDataDirectoryCommit_(sRollbackDataDirectory);


      // Reinitialize server since everything may have changed.
      // We can't run Application::ReInitialize here, since this
      // thread is owned by the backup manager, which is owned
      // by the application. And the backup manager is recreated
      // upon reinitialization.
      Logger::Instance()->LogBackup("Reinitializing server (async)...");

      Reinitializator::Instance()->ReInitialize();

      Logger::Instance()->LogBackup("Restore completed successfully.");         

      return true;
   }

   bool
   BackupExecuter::PrepareRestoreDataDirectory_(std::shared_ptr<Backup> pBackup, XNode *pBackupNode, String &sPreparedDataDirectory, String &sErrorMessage)
   {
      XNode *pBackupInfoNode = pBackupNode->GetChild(_T("BackupInformation"));
      
      // Create the path to the zip file.
      String sBackupFile = pBackup->GetBackupFile();
      String sPath = sBackupFile.Mid(0, sBackupFile.ReverseFind(_T("\\")));

      String sDirContainingDataFiles;
      String sDataFileFormat = pBackupInfoNode->GetChildAttr(_T("DataFiles"), _T("Format"))->value;
      
      String sExtractedFilesDirectory;
      if (sDataFileFormat.CompareNoCase(_T("7Z")) == 0)
      {
         // Create the path to the directory that will contain the extracted files. 
         //  This directory is temporary and will be removed when we're done.
         sExtractedFilesDirectory = Utilities::GetUniqueTempDirectory();

         // Extract the files to this directory.
         Compression oComp;
         if (!oComp.Uncompress(sBackupFile, sExtractedFilesDirectory))
         {
            sErrorMessage = Formatter::Format("Unable to extract message data from backup archive {0}.", sBackupFile);
            FileUtilities::DeleteDirectory(sExtractedFilesDirectory, true);
            return false;
         }

         // The data files in the zip file are stored in
         // a directory called DataBackup.
         sDirContainingDataFiles = sExtractedFilesDirectory + "\\DataBackup";
      }
      else
      {
         // Fetch the path to the data files.
         String sFolderName = pBackupInfoNode->GetChildAttr(_T("DataFiles"), _T("FolderName"))->value;
         sDirContainingDataFiles = sPath + "\\" + sFolderName;
      }

      String sDataDirectory = IniFileSettings::Instance()->GetDataDirectory();
      sPreparedDataDirectory = sDataDirectory + ".restore-" + GUIDCreator::GetGUID();

      String errorMessage;
      bool bCopySucceeded = false;
      try
      {
         bCopySucceeded = FileUtilities::CopyDirectory(sDirContainingDataFiles, sPreparedDataDirectory, errorMessage);
      }
      catch (const std::exception &e)
      {
         sErrorMessage = Formatter::Format("Unable to prepare message data for restore from {0}. Error: {1}", sDirContainingDataFiles, String(e.what()));
      }

      if (!bCopySucceeded)
      {
         if (sErrorMessage.IsEmpty())
         {
            sErrorMessage = "Unable to copy message data into the restore staging directory.";
            if (!errorMessage.IsEmpty())
               sErrorMessage += " Details: " + errorMessage;
         }

         FileUtilities::DeleteDirectory(sPreparedDataDirectory, true);

         if (!sExtractedFilesDirectory.IsEmpty())
            FileUtilities::DeleteDirectory(sExtractedFilesDirectory, true);

         return false;
      }

      if (sDataFileFormat.CompareNoCase(_T("7z")) == 0)
      {
         // The temporary directory we created while
         // unzipping should be deleted now.
         FileUtilities::DeleteDirectory(sExtractedFilesDirectory, true);
      }

      return true;
   }

   bool
   BackupExecuter::BeginRestoreDataDirectoryCommit_(String &sRollbackDataDirectory, String &sErrorMessage)
   {
      String sDataDirectory = IniFileSettings::Instance()->GetDataDirectory();
      const bool bHadExistingDataDirectory = FileUtilities::Exists(sDataDirectory);

      if (bHadExistingDataDirectory)
      {
         sRollbackDataDirectory = sDataDirectory + ".restore-rollback-" + GUIDCreator::GetGUID();
         if (!FileUtilities::Move(sDataDirectory, sRollbackDataDirectory))
         {
            sErrorMessage = "Unable to preserve the current data directory before restore. The existing data directory was left untouched.";
            sRollbackDataDirectory.Empty();
            return false;
         }
      }

      if (!FileUtilities::CreateDirectory(sDataDirectory))
      {
         if (bHadExistingDataDirectory)
         {
            if (!FileUtilities::Move(sRollbackDataDirectory, sDataDirectory))
            {
               sErrorMessage = Formatter::Format("Unable to create a temporary empty data directory for restore, and the preserved data directory could not be moved back automatically. Existing data remains at {0}.", sRollbackDataDirectory);
               return false;
            }
         }

         sErrorMessage = "Unable to create a temporary empty data directory for restore. Existing message data was preserved.";
         return false;
      }

      return true;
   }

   bool
   BackupExecuter::CommitRestoreDataDirectory_(const String &sPreparedDataDirectory, const String &sRollbackDataDirectory, String &sErrorMessage)
   {
      if (!FileUtilities::Exists(sPreparedDataDirectory))
      {
         sErrorMessage = "The prepared restore data directory no longer exists.";
         AbortRestoreDataDirectoryCommit_(sPreparedDataDirectory, sRollbackDataDirectory);
         return false;
      }

      String sDataDirectory = IniFileSettings::Instance()->GetDataDirectory();
      if (FileUtilities::Exists(sDataDirectory))
      {
         if (!FileUtilities::DeleteDirectory(sDataDirectory, true))
         {
            sErrorMessage = "Unable to remove the temporary empty data directory before activating restored data.";
            AbortRestoreDataDirectoryCommit_(sPreparedDataDirectory, sRollbackDataDirectory);
            return false;
         }
      }

      if (!FileUtilities::Move(sPreparedDataDirectory, sDataDirectory))
      {
         AbortRestoreDataDirectoryCommit_(sPreparedDataDirectory, sRollbackDataDirectory);

         if (sRollbackDataDirectory.IsEmpty() || !FileUtilities::Exists(sRollbackDataDirectory))
            sErrorMessage = "Unable to activate the prepared restore data directory. The original data directory was restored when available.";
         else
            sErrorMessage = Formatter::Format("Unable to activate the prepared restore data directory and automatic rollback failed. Previous data remains in {0}.", sRollbackDataDirectory);

         return false;
      }

      return true;
   }

   void
   BackupExecuter::AbortRestoreDataDirectoryCommit_(const String &sPreparedDataDirectory, const String &sRollbackDataDirectory)
   {
      String sDataDirectory = IniFileSettings::Instance()->GetDataDirectory();

      if (FileUtilities::Exists(sDataDirectory))
         FileUtilities::DeleteDirectory(sDataDirectory, true);

      if (!sRollbackDataDirectory.IsEmpty() && FileUtilities::Exists(sRollbackDataDirectory))
      {
         if (!FileUtilities::Move(sRollbackDataDirectory, sDataDirectory))
            Logger::Instance()->LogBackup("Unable to restore the pre-restore data directory automatically. Existing data remains at: " + sRollbackDataDirectory);
      }

      if (!sPreparedDataDirectory.IsEmpty() && FileUtilities::Exists(sPreparedDataDirectory))
         FileUtilities::DeleteDirectory(sPreparedDataDirectory, true);
   }

   void
   BackupExecuter::FinalizeRestoreDataDirectoryCommit_(const String &sRollbackDataDirectory)
   {
      if (!sRollbackDataDirectory.IsEmpty() && FileUtilities::Exists(sRollbackDataDirectory))
      {
         if (!FileUtilities::DeleteDirectory(sRollbackDataDirectory, true))
            Logger::Instance()->LogBackup("Restore completed successfully, but the preserved pre-restore data directory could not be removed: " + sRollbackDataDirectory);
      }
   }
}
