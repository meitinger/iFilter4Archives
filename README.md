# iFilter4Archives

## Description
This library uses 7-Zip to provide an iFilter for various archive formats,
including (but not limited to) `.7z`, `.rar`, `.tar` and `.gz` files, allowing
Windows Search and other programs (e.g. SQL Server) to find compressed files
and archives based on their content.

## Build
The build system uses `cmake` and requires at least version 3.15. It creates
`iFilter4Archives.dll` in `./out/build/[platform]-[configuration]/`, which is
statically linked to the MSVC runtime.

To build the Windows Installer package, which bundles 7-Zip together with
the iFilter, the [Windows Installer Toolset](https://wixtoolset.org/) version
3.10 or later is required. Also, the contents of 7-Zip's `.exe` installer must
be extracted to either `./installer/src/x86/` or `./installer/src/x64/`,
depending on the targeted architecture.

The installer is not MUI. Each `./installer/7-Zip.[culture].wxl` file results
in a `./out/build/[platform]-[configuration]/installer/[culture]/7-Zip.msi`.

## Installation
Either build the Windows Installer package and install it or copy the `7z.dll`
library from a 7-Zip installation or 7-Zip's `.exe` installer together with
`iFilter4Archives.dll` into a directory of your choice and run
```shell
regsvr32 iFilter4Archives.dll
```
in that directory.

## Settings
Under `HKEY_LOCAL_MACHINE\SOFTWARE\iFilter4Archives` a couple of tweaks can be
set using the following `DWORD` values:
- `ConcurrentFilterThreads`: Sets the amount of threads the library uses per
  input file, i.e. the number of contained files it scans simultaneously.
  Defaults to the number of available hardware threads.
- `MaximumFileSize`: Specified the maximum size up to which a contained file
  will be scanned, in megabytes. This should be equal to the Windows Search
  setting `MaxDownloadSize`.
  Defaults to `16` megabytes.
- `MaximumBufferSize`: Specified the maximum size up to which a contained file
  will be placed in memory, in bytes. Files above that size will be
  temporarily extracted to disk instead. This value, multiplied by
  `ConcurrentFilterThreads`, should not exceed the Windows Search setting
  `FilterProcessMemoryQuota`.
  Default to `4194304` bytes.
- `RecursionDepthLimit`: Limits the amount of archive file recursions, after
  which no additionally contained archive file will be scanned.
  Defaults to `1`.

The iFilter that used to scan a contained file depends on the following
settings and in that order:
- `IgnoreRegisteredPersistentHandlerIfArchive`: If set to `1`, 7-Zip will be
  used to scan the contained file if it's a known compressed or archive type,
  ignoring any other iFilter that might be registered with that type.
  Defaults to `0`.
- `IgnoreNullPersistentHandler`: If set to `1`, which is the default, the
  Windows _"Dummy iFilter"_, which only collects file properties, will not be
  invoked even if a type is associated with that handler. These properties are
  of no use since they only apply for a single file, e.g. the last time a file
  was modified.
- `UseInternalPersistentHandlerIfNoneRegistered`: If set to `1`, which is the
  default, 7-Zip will be used if no iFilter is associated with a contained
  file and it's a known compressed or archive type.
