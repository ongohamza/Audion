#include <windows.h>
#include <softpub.h>
#include <stdio.h>
#include <wintrust.h>

int wmain(int argc, wchar_t **argv)
{
    WINTRUST_FILE_INFO file = {0};
    WINTRUST_DATA data = {0};
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status;

    if (argc != 2)
    {
        fwprintf(stderr, L"usage: verify_trust.exe <file>\n");
        return 2;
    }

    file.cbStruct = sizeof(file);
    file.pcwszFilePath = argv[1];
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

    status = WinVerifyTrust(NULL, &action, &data);
    wprintf(L"%08lx\n", status);

    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(NULL, &action, &data);
    return status == ERROR_SUCCESS ? 0 : 1;
}
