#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "libs\SimpleString.h"
#include "version.h"

typedef enum {
	MODE_NORMAL = 0,
	MODE_CMD_C,
	MODE_CMD_K
} LAUNCHMODE, *PLAUNCHMODE;

#ifndef SEE_MASK_NOASYNC
#define SEE_MASK_NOASYNC 0x00000100
#endif

#define countof(x) (sizeof(x)/sizeof(x[0]))

VOID WINAPI PrintErrorAndExit( );
__forceinline BOOL IsFlag( PCTSTR pszArg );
__forceinline BOOL CheckFlagI( PCTSTR pszArg, TCHAR ch );
__forceinline BOOL ReadEnvironmentVariable( PCTSTR pszName, PTSTR pszBuffer, DWORD cchBuffer );

#pragma comment(linker, "/entry:elevate")
void elevate( )
{
	LAUNCHMODE mode = MODE_NORMAL;
	BOOL fWait = FALSE;
	BOOL fNoPushD = FALSE;
	BOOL fUnicode = FALSE;
	BOOL fShowUsage = FALSE;

	BOOL fInQuotes = FALSE;
	PTSTR pszCmdLine = GetCommandLine();

	if (!pszCmdLine)
		ExitProcess(~1);

	// Skip past the program name; i.e., the first (and possibly quoted) token.
	// This is exactly how Microsoft's own CRT discards the first token.

	while (*pszCmdLine > TEXT(' ') || (*pszCmdLine && fInQuotes))
	{
		if (*pszCmdLine == TEXT('\"'))
			fInQuotes = ~fInQuotes;

		++pszCmdLine;
	}

	// Process the flags; when this loop ends, the pointer position will be at
	// the start of the command line that elevate will execute.

	while (TRUE)
	{
		// Skip past any white space preceding the token.
		while (*pszCmdLine && *pszCmdLine <= TEXT(' '))
			++pszCmdLine;

		if (!IsFlag(pszCmdLine))
			break;
		else if (mode == MODE_NORMAL && CheckFlagI(pszCmdLine, TEXT('c')))
			mode = MODE_CMD_C;
		else if (mode == MODE_NORMAL && CheckFlagI(pszCmdLine, TEXT('k')))
			mode = MODE_CMD_K;
		else if (!fNoPushD && CheckFlagI(pszCmdLine, TEXT('n')))
			fNoPushD = TRUE;
		else if (!fUnicode && CheckFlagI(pszCmdLine, TEXT('u')))
			fUnicode = TRUE;
		else if (!fWait && CheckFlagI(pszCmdLine, TEXT('w')))
			fWait = TRUE;
		else
			fShowUsage = TRUE;

		pszCmdLine += 2;
	}

	if (fShowUsage || ((fNoPushD || fUnicode) && mode == MODE_NORMAL) || (*pszCmdLine == 0 && mode != MODE_CMD_K))
	{
		static const TCHAR szUsageTemplateGeneral[] = TEXT("  -%c  %s.\n");

		_tprintf(TEXT("Usage: elevate [(-c | -k) [-n] [-u]] [-w] command\n\n"));
		_tprintf(TEXT("Options:\n"));
		_tprintf(szUsageTemplateGeneral, TEXT('c'), TEXT("Launches a terminating command processor; equivalent to \"cmd /c command\""));
		_tprintf(szUsageTemplateGeneral, TEXT('k'), TEXT("Launches a persistent command processor; equivalent to \"cmd /k command\""));
		_tprintf(szUsageTemplateGeneral, TEXT('n'), TEXT("When using -c or -k, do not pushd the current directory before execution"));
		_tprintf(szUsageTemplateGeneral, TEXT('u'), TEXT("When using -c or -k, use Unicode; equivalent to \"cmd /u\""));
		_tprintf(szUsageTemplateGeneral, TEXT('w'), TEXT("Waits for termination; equivalent to \"start /wait command\""));

		ExitProcess(~1);
	}
	else
	{
		BOOL fSuccess;

		TCHAR szBuffer[MAX_PATH];
		PTSTR pszParamBuffer = NULL;
		PTSTR pszParams;

		SHELLEXECUTEINFO sei;
		ZeroMemory(&sei, sizeof(sei));
		sei.cbSize = sizeof(SHELLEXECUTEINFO);
		sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
		sei.lpVerb = TEXT("runas");
		sei.nShow = SW_SHOWNORMAL;

		if (mode == MODE_NORMAL)
		{
			/* Normal mode: Through the insertion of NULL characters, splice the
			 * remainder of the pszCmdLine string into program and parameters,
			 * while also removing quotes from the program parameter.
			 *
			 * This eliminates the need to copy strings around, avoiding all
			 * associated issues (e.g., accounting for buffer sizes).
			 */

			// Step 1: Strip quotes and walk to the end of the program string.

			sei.lpFile = pszCmdLine;
			fInQuotes = FALSE;

			while (*pszCmdLine > TEXT(' ') || (*pszCmdLine && fInQuotes))
			{
				if (*pszCmdLine == TEXT('\"'))
				{
					fInQuotes = ~fInQuotes;

					// If we just entered quotes, scooch past the opening quote,
					// and if we are exiting quotes, delete the closing quote.

					if (fInQuotes)
						++sei.lpFile;
					else
						*pszCmdLine = 0;
				}

				++pszCmdLine;
			}

			// Step 2: Walk to the start of the parameters string, replacing
			// the preceding whitespace with string-delimiting NULLs.

			while (*pszCmdLine && *pszCmdLine <= TEXT(' '))
				*pszCmdLine++ = 0;

			sei.lpParameters = pszCmdLine;
		}
		else
		{
			/* ComSpec mode: The entire remainder of pszCmdLine (plus a
			 * preceding switch) is the parameters string, and the program
			 * string will, if possible, come from %ComSpec%.
			 */

			sei.lpFile = (ReadEnvironmentVariable(TEXT("ComSpec"), szBuffer, countof(szBuffer))) ?
				szBuffer :
				TEXT("cmd.exe"); // Fallback

			if (!fNoPushD)
			{
				// We want <pushd "CurrentDirectory" & command>

				UINT_PTR cchCmdLine = SSLen(pszCmdLine);
				UINT32 cchDirectory = GetCurrentDirectory(0, NULL);

				if (!cchDirectory)
					PrintErrorAndExit();

				if (pszParamBuffer = LocalAlloc(LMEM_FIXED, (cchDirectory + cchCmdLine + 20) * sizeof(TCHAR)))
				{
					#define SZ_PUSHD_PRE TEXT("pushd \"")
					#define CCH_PUSHD_PRE 7
					#define SZ_PUSHD_POST TEXT("\" & ")
					#define CCH_PUSHD_POST 4

					pszParams = pszParamBuffer;

					// I like reducing the number of linked import dependencies
					pszParamBuffer += 6;
					pszParamBuffer = SSChainNCpy(pszParamBuffer, SZ_PUSHD_PRE, CCH_PUSHD_PRE);
					pszParamBuffer += GetCurrentDirectory(cchDirectory, pszParamBuffer);
					pszParamBuffer = SSChainNCpy(pszParamBuffer, SZ_PUSHD_POST, CCH_PUSHD_POST);
					SSChainNCpy(pszParamBuffer, pszCmdLine, cchCmdLine + 1);
				}
				else
				{
					PrintErrorAndExit();
				}
			}
			else
			{
				pszParams = pszCmdLine - 6;
			}

			if (fUnicode)
				SSCpy4Ch(pszParams, '/', 'u', ' ', '/');
			else
				SSCpy4Ch(pszParams, ' ', ' ', ' ', '/');

			pszParams[4] = (mode == MODE_CMD_C) ? TEXT('c') : TEXT('k');
			pszParams[5] = TEXT(' ');

			sei.lpParameters = pszParams;
		}

		fSuccess = ShellExecuteEx(&sei);

		if (!fNoPushD)
			LocalFree(pszParams);

		if (fSuccess)
		{
			// Success: Wait, if necessary, and clean up.

			if (sei.hProcess)
			{
				if (fWait)
					WaitForSingleObject(sei.hProcess, INFINITE);

				CloseHandle(sei.hProcess);
			}

			ExitProcess(0);
		}
		else
		{
			PrintErrorAndExit();
		}
	}
}

VOID WINAPI PrintErrorAndExit( )
{
	DWORD dwErrorCode = GetLastError();

	if (dwErrorCode)
	{
		PTSTR pszErrorMessage = NULL;

		DWORD cchMessage = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dwErrorCode,
			0,
			(PTSTR)&pszErrorMessage,
			0,
			NULL
		);

		if (cchMessage)
			_ftprintf(stderr, TEXT("%s"), pszErrorMessage);
		else
			_ftprintf(stderr, TEXT("Error [%08X].\n"), dwErrorCode);

		LocalFree(pszErrorMessage);
	}
	else
	{
		_ftprintf(stderr, TEXT("Unspecified error.\n"));
	}

	ExitProcess(1);
}

__forceinline BOOL IsFlag( PCTSTR pszArg )
{
	return(
		(pszArg[0] | 0x02) == TEXT('/') &&
		(pszArg[1]       ) != 0 &&
		(pszArg[2]       ) <= TEXT(' ')
	);
}

__forceinline BOOL CheckFlagI( PCTSTR pszArg, TCHAR ch )
{
	return(
		(pszArg[1] | 0x20) == ch
	);
}

__forceinline BOOL ReadEnvironmentVariable( PCTSTR pszName, PTSTR pszBuffer, DWORD cchBuffer )
{
	// A simple GetEnvironmentVariable wrapper with error checking
	DWORD cchCopied = GetEnvironmentVariable(pszName, pszBuffer, cchBuffer);
	return(cchCopied && cchCopied < cchBuffer);
}
