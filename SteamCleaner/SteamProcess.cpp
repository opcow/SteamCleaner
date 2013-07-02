/*
	SteamCleaner Steam popup auto-closer.
    Copyright (C) 2013  Mitch Crane

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <windows.h>
#include <psapi.h>

bool CheckProcessName(const DWORD processID, const TCHAR *checkProcessName)
{
    TCHAR processName[MAX_PATH];

    // Get a handle to the process.
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

	// Get the process name.
    if (NULL != hProcess)
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if (0 != EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
        {
			if (0 != GetModuleBaseName(hProcess, hMod, processName, sizeof(processName)/sizeof(TCHAR)))
			{
				WCHAR * c = processName;
				while (*c != L'\0')
					*c++ = towlower(*c);
				return !wcsncmp(processName, checkProcessName, MAX_PATH);
			}
		}
	    CloseHandle(hProcess);
    }
	return false;
}

DWORD FindProcess(const WCHAR *name)
{
    DWORD pProcessIds[1024];
    DWORD pBytesReturned;

    EnumProcesses(pProcessIds, sizeof(pProcessIds), &pBytesReturned);

    DWORD numProcesses = pBytesReturned / sizeof(DWORD);

    for (UINT i = 0; i < numProcesses; i++)
    {
        if(pProcessIds[i] != 0)
        {
            if (CheckProcessName(pProcessIds[i], name))
                return pProcessIds[i];
        }
    }
    return 0;
}
