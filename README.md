hMailServer
===========

hMailServer is an open source email server for Microsoft Windows.

_This is a fork of hMailServer._ There’s no guarantee of active development, but anyone is welcome to use it if they find it helpful.

Building hMailServer
====================

Environment set up
---------------------

**Required software**

   * An installed version of hMailServer 5.7 (configured with a database)
   * Visual Studio 2019 Community edition
   * InnoSetup 5.5.4a (non-unicode version)
   * [Perl ActiveState ActivePerl Community Edition 32 bit works fine](https://www.activestate.com/activeperl/downloads)
   
**NOTE**

You should not be compiling hMailServer on a computer which already runs a production version of hMailServer. When compiling hMailServer, the compilation will stop any already running version of hMailServer, and will register the compiled version as the hMailServer version on the machine (configuring the Windows service). This means that if you are running a production version of hMailServer on the machine, this version will stop running if you compile hMailServer. If this happens, the easiest path is to reinstall the production version.

Installing Visual Studio 2019 Community edition
----------------------------------------------

1. Download [Visual Studio 2019](https://visualstudio.microsoft.com/vs/) and launch the installation.
2. Select the following _Workloads_
  * .NET desktop development
  * Desktop development with C++
3. Select the following _Individual components_
  * C++ ATL for latest v142 build tools (x86 & x64)
  * Windows 10 SDK (10.0.18362.0)

3rd party libraries
-------------------

Some 3rd party libraries which hMailServer relies on are large and updated frequently. Rather than including these large libraries into the hMailServer git repository, they have to be downloaded and built, currently manually. When you build hMailServer, Visual Studio will use a system environment variable, named hMailServerLibs, to locate these libraries.

Create an environment variable named hMailServerLibs pointing at a folder where you will store hMailServer libraries, such as C:\Dev\hMailLibs.

Building OpenSSL
----------------
hMailServer currently uses OpenSSL 3.5.8. The helper script downloads a clean OpenSSL 3.5.x source tree, imports the Visual Studio 2019 x64 build environment, and installs the result under `%hMailServerLibs%\openssl-<Version>\out64`.

Prerequisites:
- The environment variable `hMailServerLibs`.
- Perl on PATH.
- Visual Studio 2019 with the x64 C++ build tools.

Run from the repository root:

   <pre>
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File libraries\build-openssl.ps1 -Version 3.5.8
   </pre>

Only OpenSSL 3.5.x is supported by this helper.


Building PostgreSQL
-------------------
hMailServer talks to PostgreSQL through libpq. libpq is built by the `libraries\build-pgsql.ps1`
script, which downloads the requested version into %hMailServerLibs%\postgresql-&lt;Version&gt;,
generates the `src\tools\msvc\config.pl` that links libpq against a previously built OpenSSL, and
builds `libpq.dll` / `libpq.lib` into `postgresql-&lt;Version&gt;\Release\libpq`.

Prerequisites:
- The environment variable hMailServerLibs (see above).
- A matching OpenSSL build (`openssl-&lt;Version&gt;\out64`) already present - build it first with the OpenSSL script above.
- Perl (e.g. [Strawberry Perl](https://strawberryperl.com/)) on PATH - required by PostgreSQL's build.pl.
- Visual Studio 2019 with the x64 C++ build tools.

Run, from the repository root:

   <pre>
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File libraries\build-pgsql.ps1 -Version 15.19
   </pre>

The script auto-detects the OpenSSL version to link against from the hMailServer project; pass
`-OpenSSLVersion 3.5.8` to override it. Only PostgreSQL 15.x and 16.x are supported (17 removed
the `src\tools\msvc\build.pl` build system this relies on).

Building Boost
--------------
hMailServer currently uses Boost 1.92.0. The helper script downloads a clean source tree and builds the static, multithreaded x64 libraries used by hMailServer.

Prerequisites:
- The environment variable `hMailServerLibs`.
- Visual Studio 2019 with the x64 C++ build tools.

Run from the repository root:

   <pre>
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File libraries\build-boost.ps1 -Version 1.92.0
   </pre>

Pass `-Toolset <name>` to override `msvc-14.2`, or `-Jobs <n>` to change the number of parallel compilations.

Building hMailServer
--------------------

Visual Studio 2019 must be started with _Run as Administrator_.

1. Download the source code from this Git repository.
2. Compile the solution hmailserver\source\Server\hMailServer\hMailServer.sln.
   This will build the hMailServer server-part (hMailServer.exe)
3. Compile the solution hmailserver\source\Tools\hMailServer Tools.sln.
   This will build hMailServer related tools, such as hMailServer Administrator and hMailServer DB Setup.
4. Compile hmailserver\installation\hMailServer.iss (using InnoSetup)
   This will build the hMailServer installation program.

Running in Debug
----------------

If you want to run hMailServer in debug mode in Visual Studio, add the command argument /debug. You find this setting in the Project properties, under Configuration Properties -> Debugging.

Running tests
-------------

hMailServer source code contains a number of automated tests which excercises the basic functionality. When adding new features or fixing bugs, corresponding tests should be added. hMailServer tests are implemented using NUnit. To run them in Visual Studio, follow these steps:

NOTE: When running tests, your local hMailServer installation will be updated with test accounts. Existing domains and accounts are deleted. Each tests prepares the server configuration in different ways. In other words, do not run the automated tests in an environment where you need to preserve hMailServer data.

1. Make sure hMailServer.exe is built and can be run. The tests will launch the service.
2. Open the test solution, `\hmailserver\test\hMailServer Tests.sln`
3. In Visual Studio, select Test Explorer from the View-menu. 
4. Locate a test to run under "RegressionTests"
5. Right-click on a test or test category and select "Run".

You can also navigate to the source code for a test, right-click anywhere and select "Run Test(s)" to run it.
