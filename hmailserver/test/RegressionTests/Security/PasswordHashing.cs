// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com

using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Authentication;
using System.Text;
using hMailServer;
using NUnit.Framework;
using RegressionTests.Infrastructure;
using RegressionTests.Shared;

namespace RegressionTests.Security
{
   [TestFixture]
   public class PasswordHashing : TestFixtureBase
   {
      private const string Address = "test@example.test";
      private const string Password = "SecretPassword";
      private const string AdministratorPassword = "testar";

      private const ePasswordHashAlgorithm AlgorithmArgon2id = ePasswordHashAlgorithm.ePWHashArgon2id;
      private const ePasswordHashAlgorithm AlgorithmPbkdf2Sha256 = ePasswordHashAlgorithm.ePWHashPBKDF2SHA256;

      private const int DefaultArgon2idMemoryCost = 19456;
      private const int DefaultArgon2idIterations = 2;

      private const int EncryptionPlainText = 0;
      private const int EncryptionBlowfish = 1;
      private const int EncryptionMd5 = 2;

      private const int MinArgon2idMemoryCost = 4096;
      private const int MaxArgon2idMemoryCost = 1048576;
      private const int MinArgon2idIterations = 1;
      private const int MaxArgon2idIterations = 20;
      private const int MinPbkdf2Iterations = 10000;
      private const int MaxPbkdf2Iterations = 10000000;

      private static string EncodeBase64(string s)
      {
         return Convert.ToBase64String(Encoding.UTF8.GetBytes(s));
      }

      private static string GetStoredPassword()
      {
         return SingletonProvider<TestSetup>.Instance.GetApp().Domains[0].Accounts[0].Password;
      }

      private static string GetIniFileName()
      {
         // The server administrator password lives in hMailServer.ini rather than in
         // the database.
         return IniFileLocator.GetIniFileName();
      }

      private static string ReadStoredAdministratorPassword()
      {
         const string key = "AdministratorPassword=";

         using (var fileStream = new FileStream(GetIniFileName(), FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
         using (var textReader = new StreamReader(fileStream))
         {
            string line;

            while ((line = textReader.ReadLine()) != null)
            {
               line = line.Trim();

               if (line.StartsWith(key, StringComparison.OrdinalIgnoreCase))
                  return line.Substring(key.Length);
            }
         }

         return string.Empty;
      }

      private static void RequireKnownAdministratorPassword()
      {
         if (new Application().Authenticate("Administrator", AdministratorPassword) == null)
            Assert.Ignore("The server administrator password is not '" + AdministratorPassword +
                          "', so this test cannot restore it afterwards.");
      }

      private static void ClearCache()
      {
         SingletonProvider<TestSetup>.Instance.GetApp().Settings.Cache.Clear();
      }

      private void SetArgon2idHashing(int iterations, int memoryCostKb)
      {
         _settings.PasswordHashAlgorithm = AlgorithmArgon2id;
         _settings.PasswordHashIterations = iterations;
         _settings.PasswordHashMemoryCost = memoryCostKb;
      }

      private void SetPbkdf2Sha256Hashing(int iterations)
      {
         _settings.PasswordHashAlgorithm = AlgorithmPbkdf2Sha256;
         _settings.PasswordHashIterations = iterations;
         _settings.PasswordHashMemoryCost = 0;
      }

      private static void OverwriteStoredPassword(Account account, string password, int passwordEncryption)
      {
         var sql = string.Format(
            "update hm_accounts set accountpassword = '{0}', accountpwencryption = {1} where accountid = {2}",
            TestSetup.Escape(password), passwordEncryption, account.ID);

         SingletonProvider<TestSetup>.Instance.GetApp().Database.ExecuteSQL(sql);
         ClearCache();
      }

      private static void LogonAndDisconnect(string address, string password)
      {
         var pop3 = new Pop3ClientSimulator();
         Assert.IsTrue(pop3.ConnectAndLogon(address, password));
         pop3.Disconnect();
      }

      private static void AssertLogonSucceedsOnAllProtocols(string address, string password)
      {
         var pop3 = new Pop3ClientSimulator();
         Assert.IsTrue(pop3.ConnectAndLogon(address, password));
         pop3.Disconnect();

         var imap = new ImapClientSimulator();
         Assert.IsTrue(imap.ConnectAndLogon(address, password));
         imap.Disconnect();

         string errorMessage;
         var smtp = new SmtpClientSimulator();
         smtp.ConnectAndLogon(EncodeBase64(address), EncodeBase64(password), out errorMessage);
         smtp.Disconnect();
      }

      private static void AssertLogonFails(string address, string password)
      {
         string errorMessage;

         var pop3 = new Pop3ClientSimulator();
         Assert.IsFalse(pop3.ConnectAndLogon(address, password, out errorMessage));

         var imap = new ImapClientSimulator();
         Assert.IsFalse(imap.ConnectAndLogon(address, password, out errorMessage));

         var smtp = new SmtpClientSimulator();
         CustomAsserts.Throws<AuthenticationException>(() =>
            smtp.ConnectAndLogon(EncodeBase64(address), EncodeBase64(password), out errorMessage));
      }

      [Test]
      public void PasswordHashSettingsHaveExpectedDefaults()
      {
         Assert.AreEqual(AlgorithmPbkdf2Sha256, _settings.PasswordHashAlgorithm);
         Assert.AreEqual(0, _settings.PasswordHashMemoryCost);
         Assert.AreEqual(MinPbkdf2Iterations, _settings.PasswordHashIterations);
      }

      [Test]
      public void PasswordHashAutoUpgradeCanBeToggled()
      {
         _settings.PasswordHashAutoUpgradeEnabled = false;
         Assert.IsFalse(_settings.PasswordHashAutoUpgradeEnabled);
         _settings.PasswordHashAutoUpgradeEnabled = true;
         Assert.IsTrue(_settings.PasswordHashAutoUpgradeEnabled);
      }

      [Test]
      public void LegacyPasswordsAreLeftAloneWhenAutoUpgradeIsOff()
      {
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         var md5 = _application.Utilities.MD5(Password);
         OverwriteStoredPassword(account, md5, EncryptionMd5);
         _settings.PasswordHashAutoUpgradeEnabled = false;
         ClearCache();
         AssertLogonSucceedsOnAllProtocols(Address, Password);
         ClearCache();
         Assert.AreEqual(md5, GetStoredPassword());
      }

      [Test]
      public void TurningAutoUpgradeBackOnResumesMigration()
      {
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         var md5 = _application.Utilities.MD5(Password);
         OverwriteStoredPassword(account, md5, EncryptionMd5);
         _settings.PasswordHashAutoUpgradeEnabled = false;
         LogonAndDisconnect(Address, Password);
         ClearCache();
         Assert.AreEqual(md5, GetStoredPassword());
         _settings.PasswordHashAutoUpgradeEnabled = true;
         ClearCache();
         LogonAndDisconnect(Address, Password);
         ClearCache();
         Assert.IsTrue(GetStoredPassword().StartsWith("$pbkdf2-sha256$"));
      }

      [Test]
      public void NewAccountsAreHashedEvenWhenAutoUpgradeIsOff()
      {
         _settings.PasswordHashAutoUpgradeEnabled = false;
         SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         Assert.IsTrue(GetStoredPassword().StartsWith("$pbkdf2-sha256$"));
      }

      [Test]
      public void TheAdministratorPasswordIsNotRehashedWhenAutoUpgradeIsOff()
      {
         RequireKnownAdministratorPassword();

         try
         {
            SetArgon2idHashing(DefaultArgon2idIterations, DefaultArgon2idMemoryCost);
            _settings.SetAdministratorPassword(AdministratorPassword);
            var beforeChange = ReadStoredAdministratorPassword();
            Assert.IsTrue(beforeChange.StartsWith("$argon2id$"));
            _settings.PasswordHashMemoryCost = 32768;
            _settings.PasswordHashAutoUpgradeEnabled = false;
            Assert.IsNotNull(new Application().Authenticate("Administrator", AdministratorPassword));
            Assert.AreEqual(beforeChange, ReadStoredAdministratorPassword());
            _settings.PasswordHashAutoUpgradeEnabled = true;
            Assert.IsNotNull(new Application().Authenticate("Administrator", AdministratorPassword));
            var afterUpgrade = ReadStoredAdministratorPassword();
            Assert.AreNotEqual(beforeChange, afterUpgrade);
            StringAssert.Contains("m=32768", afterUpgrade);
         }
         finally
         {
            SetArgon2idHashing(DefaultArgon2idIterations, DefaultArgon2idMemoryCost);
            _settings.SetAdministratorPassword(AdministratorPassword);
         }
      }

      [Test]
      public void SettingTheAdministratorPasswordStoresAHashOfIt()
      {
         RequireKnownAdministratorPassword();
         _settings.SetAdministratorPassword(AdministratorPassword);
         var stored = ReadStoredAdministratorPassword();
         Assert.IsTrue(stored.StartsWith("$pbkdf2-sha256$"));
         Assert.IsNotNull(new Application().Authenticate("Administrator", AdministratorPassword));
         Assert.IsNull(new Application().Authenticate("Administrator", AdministratorPassword + "x"));
      }

      [Test]
      public void PasswordHashSettingsCanBeChanged()
      {
         _settings.PasswordHashAlgorithm = AlgorithmPbkdf2Sha256;
         _settings.PasswordHashMemoryCost = 32768;
         _settings.PasswordHashIterations = 700000;
         Assert.AreEqual(AlgorithmPbkdf2Sha256, _settings.PasswordHashAlgorithm);
         Assert.AreEqual(32768, _settings.PasswordHashMemoryCost);
         Assert.AreEqual(700000, _settings.PasswordHashIterations);
      }

      [Test]
      public void NewAccountsAreHashedUsingArgon2idWhenSelected()
      {
         SetArgon2idHashing(DefaultArgon2idIterations, DefaultArgon2idMemoryCost);
         SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         Assert.IsTrue(GetStoredPassword().StartsWith("$argon2id$"));
      }

      [Test]
      public void NewAccountsAreHashedUsingPbkdf2WhenSelected()
      {
         SetPbkdf2Sha256Hashing(MinPbkdf2Iterations);
         SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         Assert.IsTrue(GetStoredPassword().StartsWith("$pbkdf2-sha256$"));
      }

      [Test]
      public void LegacyMd5PasswordsCanStillBeUsedToLogOn()
      {
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         var md5 = _application.Utilities.MD5(Password);
         OverwriteStoredPassword(account, md5, EncryptionMd5);
         AssertLogonSucceedsOnAllProtocols(Address, Password);
      }

      [Test]
      public void LegacyMd5PasswordsRejectAnIncorrectPassword()
      {
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         var md5 = _application.Utilities.MD5(Password);
         OverwriteStoredPassword(account, md5, EncryptionMd5);
         AssertLogonFails(Address, "WrongPassword");
      }

      [Test]
      public void LegacyBlowfishPasswordsCanStillBeUsedToLogOn()
      {
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         var blowfish = _application.Utilities.BlowfishEncrypt(Password);
         OverwriteStoredPassword(account, blowfish, EncryptionBlowfish);
         AssertLogonSucceedsOnAllProtocols(Address, Password);
      }

      [Test]
      public void LegacyBlowfishPasswordsRejectAnIncorrectPassword()
      {
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         var blowfish = _application.Utilities.BlowfishEncrypt(Password);
         OverwriteStoredPassword(account, blowfish, EncryptionBlowfish);
         AssertLogonFails(Address, "WrongPassword");
      }

      [Test]
      public void LegacyPasswordsAreRehashedOnLogon()
      {
         SetArgon2idHashing(DefaultArgon2idIterations, DefaultArgon2idMemoryCost);
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         var md5 = _application.Utilities.MD5(Password);
         OverwriteStoredPassword(account, md5, EncryptionMd5);
         LogonAndDisconnect(Address, Password);
         ClearCache();
         var storedPassword = GetStoredPassword();
         Assert.AreNotEqual(md5, storedPassword);
         Assert.IsTrue(storedPassword.StartsWith("$argon2id$"));
      }

      [Test]
      public void RehashingIsNotRepeatedOnSubsequentLogons()
      {
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         OverwriteStoredPassword(account, _application.Utilities.MD5(Password), EncryptionMd5);
         LogonAndDisconnect(Address, Password);
         ClearCache();
         var afterFirstLogon = GetStoredPassword();
         LogonAndDisconnect(Address, Password);
         ClearCache();
         Assert.AreEqual(afterFirstLogon, GetStoredPassword());
      }

      [Test]
      public void AStrongerCostCausesARehashOnLogon()
      {
         SetArgon2idHashing(DefaultArgon2idIterations, DefaultArgon2idMemoryCost);
         SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         var beforeChange = GetStoredPassword();
         _settings.PasswordHashMemoryCost = 32768;
         ClearCache();
         LogonAndDisconnect(Address, Password);
         ClearCache();
         var afterChange = GetStoredPassword();
         Assert.AreNotEqual(beforeChange, afterChange);
         StringAssert.Contains("m=32768", afterChange);
      }

      [Test]
      public void ALowerMemoryCostAlsoCausesARehashOnLogon()
      {
         SetArgon2idHashing(DefaultArgon2idIterations, 32768);
         SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         _settings.PasswordHashMemoryCost = DefaultArgon2idMemoryCost;
         ClearCache();
         LogonAndDisconnect(Address, Password);
         ClearCache();
         StringAssert.Contains("m=19456", GetStoredPassword());
      }

      [Test]
      public void ALowerIterationCountAlsoCausesARehashOnLogon()
      {
         SetArgon2idHashing(6, DefaultArgon2idMemoryCost);
         SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         _settings.PasswordHashIterations = DefaultArgon2idIterations;
         ClearCache();
         LogonAndDisconnect(Address, Password);
         ClearCache();
         StringAssert.Contains("t=2", GetStoredPassword());
      }

      [Test]
      public void ChangingTheAlgorithmCausesARehashOnLogon()
      {
         SetArgon2idHashing(DefaultArgon2idIterations, DefaultArgon2idMemoryCost);
         SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         SetPbkdf2Sha256Hashing(MinPbkdf2Iterations);
         ClearCache();
         LogonAndDisconnect(Address, Password);
         ClearCache();
         Assert.IsTrue(GetStoredPassword().StartsWith("$pbkdf2-sha256$"));
      }

      [Test]
      public void AFailedLogonDoesNotChangeTheStoredPassword()
      {
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         var md5 = _application.Utilities.MD5(Password);
         OverwriteStoredPassword(account, md5, EncryptionMd5);
         string errorMessage;
         Assert.IsFalse(new Pop3ClientSimulator().ConnectAndLogon(Address, "WrongPassword", out errorMessage));
         ClearCache();
         Assert.AreEqual(md5, GetStoredPassword());
      }

      [Test]
      public void PlaintextPasswordsAreLeftAloneWhenAutoUpgradeIsOff()
      {
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         OverwriteStoredPassword(account, Password, EncryptionPlainText);
         _settings.PasswordHashAutoUpgradeEnabled = false;
         ClearCache();
         AssertLogonSucceedsOnAllProtocols(Address, Password);
         ClearCache();
         Assert.AreEqual(Password, GetStoredPassword());
      }

      [Test]
      public void PlaintextPasswordsAreRehashedOnLogonWhenAutoUpgradeIsOn()
      {
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         OverwriteStoredPassword(account, Password, EncryptionPlainText);
         _settings.PasswordHashAutoUpgradeEnabled = true;
         ClearCache();
         LogonAndDisconnect(Address, Password);
         ClearCache();
         Assert.IsTrue(GetStoredPassword().StartsWith("$pbkdf2-sha256$"));
      }

      [Test]
      public void ReadingAPlaintextAccountDoesNotRehashItRegardlessOfAutoUpgrade()
      {
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         OverwriteStoredPassword(account, Password, EncryptionPlainText);
         _settings.PasswordHashAutoUpgradeEnabled = true;
         ClearCache();
         Assert.AreEqual(Password, GetStoredPassword());
         ClearCache();
         Assert.AreEqual(Password, GetStoredPassword());
      }

      [Test]
      public void APlaintextPasswordCanBeUsedWithDifferentCasingAndStillWorksAfterTheRehash()
      {
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         OverwriteStoredPassword(account, Password, EncryptionPlainText);
         var differentCasing = Password.ToUpperInvariant();
         LogonAndDisconnect(Address, differentCasing);
         ClearCache();
         Assert.IsTrue(GetStoredPassword().StartsWith("$pbkdf2-sha256$"));
         AssertLogonSucceedsOnAllProtocols(Address, differentCasing);
      }

      [Test]
      public void ABlowfishPasswordCanBeUsedWithDifferentCasingAndStillWorksAfterTheRehash()
      {
         SetArgon2idHashing(DefaultArgon2idIterations, DefaultArgon2idMemoryCost);
         var account = SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         var blowfish = _application.Utilities.BlowfishEncrypt(Password);
         OverwriteStoredPassword(account, blowfish, EncryptionBlowfish);
         var differentCasing = Password.ToUpperInvariant();
         LogonAndDisconnect(Address, differentCasing);
         ClearCache();
         Assert.IsTrue(GetStoredPassword().StartsWith("$argon2id$"));
         AssertLogonSucceedsOnAllProtocols(Address, differentCasing);
      }

      [Test]
      public void APermissiveScriptHandlerDoesNotOverwriteTheStoredPassword()
      {
         SingletonProvider<TestSetup>.Instance.AddAccount(_domain, Address, Password);
         var storedBeforeScript = GetStoredPassword();
         var scripting = _application.Settings.Scripting;
         var script = @"Sub OnClientValidatePassword(account, password)
                 Result.Value = 0
              End Sub";
         File.WriteAllText(scripting.CurrentScriptFile, script);
         scripting.Enabled = true;
         scripting.Reload();

         try
         {
            Assert.IsTrue(ImapClientSimulator.ValidatePassword(Address, "WhateverTheClientSent"));
            ClearCache();
            Assert.AreEqual(storedBeforeScript, GetStoredPassword());
         }
         finally
         {
            scripting.Enabled = false;
         }
      }

      [Test]
      public void MemoryCostBelowTheMinimumIsRejected()
      {
         var ex = Assert.Throws<COMException>(() => _settings.PasswordHashMemoryCost = MinArgon2idMemoryCost - 1);
         StringAssert.Contains("Invalid password hash memory cost", ex.Message);
      }

      [Test]
      public void MemoryCostAtTheMinimumIsAccepted()
      {
         _settings.PasswordHashMemoryCost = MinArgon2idMemoryCost;
         Assert.AreEqual(MinArgon2idMemoryCost, _settings.PasswordHashMemoryCost);
      }

      [Test]
      public void MemoryCostAboveTheMaximumIsRejected()
      {
         var ex = Assert.Throws<COMException>(() => _settings.PasswordHashMemoryCost = MaxArgon2idMemoryCost + 1);
         StringAssert.Contains("Invalid password hash memory cost", ex.Message);
      }

      [Test]
      public void MemoryCostAtTheMaximumIsAccepted()
      {
         _settings.PasswordHashMemoryCost = MaxArgon2idMemoryCost;
         Assert.AreEqual(MaxArgon2idMemoryCost, _settings.PasswordHashMemoryCost);
      }

      [Test]
      public void MemoryCostZeroIsStillAccepted()
      {
         _settings.PasswordHashMemoryCost = 32768;
         _settings.PasswordHashMemoryCost = 0;
         Assert.AreEqual(0, _settings.PasswordHashMemoryCost);
      }

      [Test]
      public void Argon2idIterationsNegativeIsRejected()
      {
         var ex = Assert.Throws<COMException>(() => _settings.PasswordHashIterations = -1);
         StringAssert.Contains("cannot be negative", ex.Message);
      }

      [Test]
      public void Argon2idIterationsAtTheMinimumIsAccepted()
      {
         _settings.PasswordHashAlgorithm = AlgorithmArgon2id;
         _settings.PasswordHashIterations = MinArgon2idIterations;
         Assert.AreEqual(MinArgon2idIterations, _settings.PasswordHashIterations);
      }

      [Test]
      public void Argon2idIterationsAboveTheMaximumIsRejected()
      {
         _settings.PasswordHashAlgorithm = AlgorithmArgon2id;
         var ex = Assert.Throws<COMException>(() => _settings.PasswordHashIterations = MaxArgon2idIterations + 1);
         StringAssert.Contains("Invalid password hash iteration count", ex.Message);
      }

      [Test]
      public void Argon2idIterationsAtTheMaximumIsAccepted()
      {
         _settings.PasswordHashAlgorithm = AlgorithmArgon2id;
         _settings.PasswordHashIterations = MaxArgon2idIterations;
         Assert.AreEqual(MaxArgon2idIterations, _settings.PasswordHashIterations);
      }

      [Test]
      public void Pbkdf2IterationsBelowTheMinimumIsRejected()
      {
         _settings.PasswordHashAlgorithm = AlgorithmPbkdf2Sha256;
         var ex = Assert.Throws<COMException>(() => _settings.PasswordHashIterations = MinPbkdf2Iterations - 1);
         StringAssert.Contains("Invalid password hash iteration count", ex.Message);
      }

      [Test]
      public void Pbkdf2IterationsAtTheMinimumIsAccepted()
      {
         _settings.PasswordHashAlgorithm = AlgorithmPbkdf2Sha256;
         _settings.PasswordHashIterations = MinPbkdf2Iterations;
         Assert.AreEqual(MinPbkdf2Iterations, _settings.PasswordHashIterations);
      }

      [Test]
      public void Pbkdf2IterationsAboveTheMaximumIsRejected()
      {
         _settings.PasswordHashAlgorithm = AlgorithmPbkdf2Sha256;
         var ex = Assert.Throws<COMException>(() => _settings.PasswordHashIterations = MaxPbkdf2Iterations + 1);
         StringAssert.Contains("Invalid password hash iteration count", ex.Message);
      }

      [Test]
      public void Pbkdf2IterationsAtTheMaximumIsAccepted()
      {
         _settings.PasswordHashAlgorithm = AlgorithmPbkdf2Sha256;
         _settings.PasswordHashIterations = MaxPbkdf2Iterations;
         Assert.AreEqual(MaxPbkdf2Iterations, _settings.PasswordHashIterations);
      }

      [Test]
      public void IterationsZeroIsStillAcceptedForBothAlgorithms()
      {
         _settings.PasswordHashAlgorithm = AlgorithmArgon2id;
         _settings.PasswordHashIterations = 6;
         _settings.PasswordHashIterations = 0;
         Assert.AreEqual(0, _settings.PasswordHashIterations);

         _settings.PasswordHashAlgorithm = AlgorithmPbkdf2Sha256;
         _settings.PasswordHashIterations = 700000;
         _settings.PasswordHashIterations = 0;
         Assert.AreEqual(0, _settings.PasswordHashIterations);
      }
   }
}
