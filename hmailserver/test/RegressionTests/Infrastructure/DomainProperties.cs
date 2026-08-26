// Copyright (c) 2010 Martin Knafve / hMailServer.com.
// http://www.hmailserver.com

using NUnit.Framework;
using RegressionTests.Shared;

namespace RegressionTests.Infrastructure
{
   [TestFixture]
   public class DomainProperties : TestFixtureBase
   {
      [Test]
      [Category("Domains")]
      [Description("Ensure that empty domain size is reported as zero")]
      public void SizeBeforeSend()
      {
         var domain = SingletonProvider<TestSetup>.Instance.AddTestDomain();
         SingletonProvider<TestSetup>.Instance.AddAccount(domain, "test@example.test", "test");

         Assert.AreEqual(0, domain.Size);
      }

      [Test]
      [Category("Domains")]
      [Description("Regression test for PR #537 - domain size was calculated using the wrong column " +
                   "on MS SQL, SQL CE and PostgreSQL, causing the reported size to be incorrect (near zero).") ]
      public void SizeReflectsMessagesAcrossAccounts()
      {
         var domain = SingletonProvider<TestSetup>.Instance.AddTestDomain();
         SingletonProvider<TestSetup>.Instance.AddAccount(domain, "test1@example.test", "test");
         SingletonProvider<TestSetup>.Instance.AddAccount(domain, "test2@example.test", "test");

         var body = TestSetup.CreateLargeDummyMailBody();

         SmtpClientSimulator.StaticSend("test1@example.test", "test1@example.test", "Test message", body);
         SmtpClientSimulator.StaticSend("test1@example.test", "test1@example.test", "Test message", body);
         ImapClientSimulator.AssertMessageCount("test1@example.test", "test", "Inbox", 2);

         SmtpClientSimulator.StaticSend("test2@example.test", "test2@example.test", "Test message", body);
         SmtpClientSimulator.StaticSend("test2@example.test", "test2@example.test", "Test message", body);
         ImapClientSimulator.AssertMessageCount("test2@example.test", "test", "Inbox", 2);

         Assert.GreaterOrEqual(domain.Size, 2,
            "Domain size must aggregate every account in the domain, not a single id-matched account.");
      }
   }
}
