using System;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Xml;

namespace VMTestRunner.Console
{
   public class TestRunner
   {
      private const string NuGetPackagesRelativePath = @"..\..\..\..\packages\";
      private const string RegressionTestsBinRelativePath = @"..\..\..\..\RegressionTests\bin\x64\Debug\";
      private const string VolumeTestsBinRelativePath = @"..\..\..\..\VolumeTests\bin\x64\Debug\";
      private const string NUnitConsoleRunnerPackagePath = @"NUnit.ConsoleRunner.3.16.3\tools";
      private const string NUnitPackagePath = @"NUnit.3.13.3\lib\net45";
      private readonly string _nUnitPath;
      private readonly string _nUnitConsolePath;

      private const string Username = "vmware";
      private const string Password = "Secret123";

      private const string RunTestScriptName = "RunTestsInHyperV.bat";

      private const string SetupLogPath = @"C:\setup.log";

      private readonly TestEnvironment _environment;

      private readonly string _softwareUnderTest;

      private readonly int _testIndex;

      private static readonly NLog.Logger Logger = NLog.LogManager.GetCurrentClassLogger();

      public TestRunner(TestEnvironment environment, string softwareUnderTest, int testIndex)
      {
         _environment = environment;
         _softwareUnderTest = softwareUnderTest;
         _testIndex = testIndex;

         var packagePath = Path.Combine(Environment.CurrentDirectory, NuGetPackagesRelativePath);

         _nUnitConsolePath = Path.Combine(packagePath, NUnitConsoleRunnerPackagePath);

         if (!Directory.Exists(_nUnitConsolePath))
            throw new InvalidOperationException($"NUnit console not found in {_nUnitConsolePath}");

         _nUnitPath = Path.Combine(packagePath, NUnitPackagePath);

         if (!Directory.Exists(_nUnitPath))
            throw new InvalidOperationException($"NUnit not found in {_nUnitPath}");
      }

      public void Run()
      {
         RunInternal();
      }

      private void RunInternal()
      {
         var vm = new HyperV(_testIndex);

         var currentDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
         var testAssemblyDirectory = Path.Combine(currentDirectory, GetTestBinRelativePath());

         if (!Directory.Exists(testAssemblyDirectory))
            throw new InvalidOperationException($"The {_environment.TestSuite} assemblies were not found in " +
               $"{Path.GetFullPath(testAssemblyDirectory)}. Build the test solution first.");

         var testAssemblyNames = GetTestAssemblyNames();

         string guestTestPath = @"C:\Nunit";

         string softwareUnderTestFullPath = _softwareUnderTest;
         string softwareUnderTestName = Path.GetFileName(softwareUnderTestFullPath);

         string softwareUnderTestSilentParmas = $"/SILENT /LOG=\"{SetupLogPath}\"";

         string sslFolder = Path.Combine(TestSettings.GetTestFolder(), "SSL examples");

         vm.OpenVM(_environment.VMName);

         try
         {
            vm.RevertToSnapshot(_environment.SnapshotName);
            vm.SetCredentials(Username, Password);

            // Make sure we have an IP address.
            EnsureNetworkAccess(vm);

            // Set up test paths.
            vm.CreateDirectory(guestTestPath);
            vm.CreateDirectory(@"C:\Temp");

            foreach (var command in _environment.PreInstallCommands)
               vm.RunProgramInGuest(command.Executable, command.Parameters);

            foreach (var copyOperation in _environment.PreInstallFileCopy)
               vm.CopyFileToGuest(copyOperation.From, copyOperation.To);

            vm.CopyFolderToGuest(_nUnitConsolePath, guestTestPath);
            vm.CopyFolderToGuest(_nUnitPath, guestTestPath);

            foreach (var testAssemblyName in testAssemblyNames)
               vm.CopyFileToGuest(Path.Combine(testAssemblyDirectory, testAssemblyName), Path.Combine(guestTestPath, testAssemblyName));

            vm.CopyFileToGuest(Path.Combine(currentDirectory, RunTestScriptName), Path.Combine(guestTestPath, RunTestScriptName));

            // Other required stuff.
            vm.CopyFolderToGuest(sslFolder, @"C:\SSL examples");

            vm.CopyFileToGuest(softwareUnderTestFullPath, Path.Combine(guestTestPath, softwareUnderTestName));
            RunSetup(vm, Path.Combine(guestTestPath, softwareUnderTestName), softwareUnderTestSilentParmas);

            foreach (var copyOperation in _environment.PostInstallFileCopy)
               vm.CopyFileToGuest(copyOperation.From, copyOperation.To);

            foreach (var command in _environment.PostInstallCommands)
               vm.RunProgramInGuest(command.Executable, command.Parameters);

            bool useLocalVersion = false;

            if (useLocalVersion)
            {
               CopyLocalVersion(vm);
            }

            if (_environment.EnablePageHeap)
               EnablePageHeap(vm, guestTestPath);

            // Run NUnit
            string runTestParameters = $"{_environment.TestSuite}.dll";

            if (_environment.IncludeStressTests)
               runTestParameters += " IncludeStress";

            vm.RunProgramInGuest(Path.Combine(guestTestPath, RunTestScriptName), runTestParameters);

            // Collect results. The NUnit result is kept next to the log file of this run.
            string localResultFile = RunContext.GetResultFilePath(_environment);
            string localLogFile = Path.GetTempFileName() + ".log";
            vm.CopyFileToHost(Path.Combine(guestTestPath, "TestResult.xml"), localResultFile);
            vm.CopyFileToHost(Path.Combine(guestTestPath, "TestResult.log"), localLogFile);

            Logger.Info($"Test {_testIndex} - NUnit result saved to {localResultFile}");

            var doc = new XmlDocument();
            doc.Load(localResultFile);

            var failedAttribute = doc.LastChild?.Attributes?["failed"]?.Value;
            int failedCount = failedAttribute != null ? Convert.ToInt32(failedAttribute) : 0;

            if (failedCount == 0)
               return;

            string resultContent = File.ReadAllText(localResultFile);
            string logContent = File.ReadAllText(localLogFile);
            string failureSummary = NUnitResultParser.SummarizeFailures(doc);
            throw new TestFailedException($"{resultContent}\r\n\r\n{logContent}", failureSummary);
         }
         finally
         {
            try
            {
               vm.PowerOff();
            }
            catch (Exception ex)
            {
               Logger.Error(ex, "Unable to power off VM. Maybe it's not powered on?");
            }
         }
      }

      /// <summary>
      /// Where the assemblies of the suite under test are built.
      /// </summary>
      private string GetTestBinRelativePath()
      {
         switch (_environment.TestSuite)
         {
            case TestSuite.VolumeTests:
               return VolumeTestsBinRelativePath;
            default:
               return RegressionTestsBinRelativePath;
         }
      }

      /// <summary>
      /// The assemblies the suite needs in the guest. The volume tests use the shared
      /// infrastructure of the regression tests, so that assembly goes along as well.
      /// </summary>
      private string[] GetTestAssemblyNames()
      {
         switch (_environment.TestSuite)
         {
            case TestSuite.VolumeTests:
               return new[] { "VolumeTests.dll", "RegressionTests.dll", "Interop.hMailServer.dll" };
            default:
               return new[] { "RegressionTests.dll", "Interop.hMailServer.dll" };
         }
      }

      /// <summary>
      /// Turns page heap on for hMailServer.exe. gflags writes the setting to the registry,
      /// so the service has to be restarted for it to take effect.
      /// </summary>
      private void EnablePageHeap(HyperV vm, string guestTestPath)
      {
         string guestGFlagsPath = Path.Combine(guestTestPath, "gflags.exe");

         vm.CopyFileToGuest(_environment.GFlagsPath, guestGFlagsPath);
         vm.RunProgramInGuest(guestGFlagsPath, "/p /enable hMailServer.exe", true);

         vm.RunProgramInGuest(@"C:\Windows\System32\net.exe", "stop hMailServer");
         vm.RunProgramInGuest(@"C:\Windows\System32\net.exe", "start hMailServer");
      }

      /// <summary>
      /// Runs the setup program. Setup returns a non-zero exit code if the installation
      /// fails, for example if the database couldn't be upgraded.
      /// </summary>
      private void RunSetup(HyperV vm, string setupPath, string parameters)
      {
         try
         {
            vm.RunProgramInGuest(setupPath, parameters, true);
         }
         catch (Exception ex)
         {
            throw new Exception($"{ex.Message}{Environment.NewLine}{Environment.NewLine}{GetSetupLog(vm)}", ex);
         }
      }

      private string GetSetupLog(HyperV vm)
      {
         try
         {
            string localSetupLog = Path.GetTempFileName() + ".log";
            vm.CopyFileToHost(SetupLogPath, localSetupLog);

            return File.ReadAllText(localSetupLog);
         }
         catch (Exception ex)
         {
            return $"The setup log {SetupLogPath} could not be read: {ex.Message}";
         }
      }

      private void CopyLocalVersion(HyperV vm)
      {
         string currentDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);

         var localExecutable = Path.Combine(currentDir,
            @"..\..\..\..\..\..\source\Server\hMailServer\x64\Release\hMailServer.exe");

         if (!File.Exists(localExecutable))
         {
            throw new Exception($"The executable {localExecutable} could not be found.");
         }

         vm.RunScriptInGuest("NET STOP HMAILSERVER");
         vm.CopyFileToGuest(localExecutable, @"C:\Program Files\hMailServer\Bin\hMailServer.exe");
         vm.RunScriptInGuest("NET START HMAILSERVER");
      }

      private void Debug(string message)
      {
         // The test index tells the status board which row the message belongs to.
         var logEvent = new NLog.LogEventInfo(NLog.LogLevel.Debug, Logger.Name, $"[Test {_testIndex}] {message}");
         logEvent.Properties[TestBoardConsoleTarget.TestIndexProperty] = _testIndex;
         Logger.Log(logEvent);
      }

      private void EnsureNetworkAccess(HyperV vm)
      {
         Debug("Ensuring network access...");

         string pingResultData = string.Empty;

         var timeoutTime = DateTime.UtcNow.AddSeconds(60);

         while (DateTime.UtcNow < timeoutTime)
         {
            try
            {
               pingResultData = vm.RunScriptInGuest("ipconfig /renew; ping www.google.com -n 1");

               if (pingResultData.Contains("Reply from "))
                  return;
            }
            catch (Exception)
            {
            }

            Thread.Sleep(TimeSpan.FromSeconds(2));
         }

         throw new Exception($"No network access. Ping result: {pingResultData}");
      }
   }
}
