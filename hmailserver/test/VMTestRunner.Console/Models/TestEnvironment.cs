using System.Collections.Generic;

namespace VMTestRunner.Console
{
   public class TestEnvironment
   {
      public string OperatingSystem { get; set; }

      /// <summary>
      /// Identifies the test in the status board and in the JSON result file.
      /// </summary>
      public string Name => BaseName;

      /// <summary>
      /// The name of the test as configured, without the run number. This is the name
      /// the --test command line parameter selects on.
      /// </summary>
      public string BaseName { get; set; }

      public string Description { get; set; }

      public string SnapshotName { get; set; }

      public string VMName { get; set; }

      /// <summary>
      /// False for tests that are too slow to belong in a normal run. They only run
      /// when selected by name with --test.
      /// </summary>
      public bool Enabled { get; set; } = true;

      /// <summary>
      /// Which test assembly to run in the guest.
      /// </summary>
      public TestSuite TestSuite { get; set; } = TestSuite.RegressionTests;

      /// <summary>
      /// Turns on page heap for hMailServer.exe before the tests run, to catch heap
      /// corruption. Makes the run considerably slower.
      /// </summary>
      public bool EnablePageHeap { get; set; }

      /// <summary>
      /// Host path of gflags.exe, used to turn page heap on.
      /// </summary>
      public string GFlagsPath { get; set; }

      public bool IncludeStressTests { get; set; }

      public List<InstallCommand> PostInstallCommands { get; } = new List<InstallCommand>();

      public List<FileCopyCommand> PostInstallFileCopy { get; } = new List<FileCopyCommand>();

      public List<FileCopyCommand> PreInstallFileCopy { get; } = new List<FileCopyCommand>();

      public List<InstallCommand> PreInstallCommands { get; } = new List<InstallCommand>();
   }
}
