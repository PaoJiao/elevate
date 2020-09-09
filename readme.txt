Command-Line UAC Elevation Utility
Version 1.3
<http://code.kliu.org/misc/elevate/>


Introduction
============

This utility executes a command with UAC privilege elevation.  This is useful
for working inside command prompts or with batch files.


Usage
=====

Synopsis:
  elevate [(-c | -k) [-n] [-u]] [-w] command

Options:
  -c  Launches a terminating command processor; equivalent to "cmd /c command".
  -k  Launches a persistent command processor; equivalent to "cmd /k command".
  -n  When using -c or -k, do not pushd the current directory before execution.
  -u  When using -c or -k, use Unicode; equivalent to "cmd /u".
  -w  Waits for termination; equivalent to "start /wait command".

Notes:
  Both the hyphen (e.g., -w) and slash (e.g., /w) forms of switches are valid.

  When -k is specified, command is optional.  Omitting command in this case will
  simply open an elevated command prompt.

  Normally, an elevated command processor will not honor the current directory
  of an unelevated parent process, thus potentially creating problems with
  relative paths.  To address this problem, when the -c or -k switches are used,
  elevate will issue a pushd command to the new command processor to ensure that
  it uses the current directory of its parent process.  Specifying the -n switch
  will disable this feature.

Examples:
  elevate taskmgr
  elevate -k
  elevate /w HashCheckInstall.exe
  elevate -k sfc /scannow
  elevate /c del %SystemRoot%\Temp\*.*
  elevate -c -w copy foo*.* bar


Why this utility?
=================

There are other similar utilities available; for example:
* <http://wintellect.com/cs/blogs/jrobbins/archive/2007/03/27/elevate-a-process-at-the-command-line-in-vista.aspx>
* <http://jpassing.com/2007/12/08/launch-elevated-processes-from-the-command-line/>

Features that set this utility apart from the rest:

* Correct handling of command line parameters; the command line parameters are
  passed along for execution verbatim, without being chopped up and reassembled.

* The ability to launch the command processor using /c instead of /k.

* The ability to launch the command processor in the current directory.

* Better error messages to make troubleshooting execution problems easier.

* Native C code avoids the burdensome .NET startup overhead and allows for a
  much smaller executable file size.
