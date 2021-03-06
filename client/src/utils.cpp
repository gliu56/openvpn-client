#include "utils.h"
//#include <WinSock2.h>
#include <Windows.h>
#include <psapi.h>
#include <Shlobj.h>

#pragma comment (lib, "Advapi32.lib")

extern bool g_bPortable;

QString Utils::dataDirectory()
{
    //
    // Return the path to the data directory
    //

    //
    QDir dir (QCoreApplication::applicationDirPath());

    return dir.absolutePath() + QLatin1String("/data");
}

bool Utils::isLegalFileName(QString nameToCheck)
{

    if (nameToCheck.isEmpty()) {
        return false;
    }

    QMap<QString, int> invalidNames;
    //
    invalidNames.insert(QLatin1String("CON"), 1);
    invalidNames.insert(QLatin1String("PRN"), 2);
    invalidNames.insert(QLatin1String("AUX"), 3);
    invalidNames.insert(QLatin1String("NUL"), 4);
    //
    invalidNames.insert(QLatin1String("COM1"), 5);
    invalidNames.insert(QLatin1String("COM2"), 6);
    invalidNames.insert(QLatin1String("COM3"), 7);
    invalidNames.insert(QLatin1String("COM4"), 8);
    invalidNames.insert(QLatin1String("COM5"), 9);
    invalidNames.insert(QLatin1String("COM6"), 10);
    invalidNames.insert(QLatin1String("COM7"), 11);
    invalidNames.insert(QLatin1String("COM8"), 12);
    invalidNames.insert(QLatin1String("COM9"), 13);
    //
    invalidNames.insert(QLatin1String("LPT1"), 14);
    invalidNames.insert(QLatin1String("LPT2"), 15);
    invalidNames.insert(QLatin1String("LPT3"), 16);
    invalidNames.insert(QLatin1String("LPT4"), 17);
    invalidNames.insert(QLatin1String("LPT5"), 18);
    invalidNames.insert(QLatin1String("LPT6"), 19);
    invalidNames.insert(QLatin1String("LPT7"), 20);
    invalidNames.insert(QLatin1String("LPT8"), 21);
    invalidNames.insert(QLatin1String("LPT9"), 22);

    //
    if (invalidNames.contains(nameToCheck)) {
        return false;
    }

    // Normal char check
    LPCWSTR filename = (const wchar_t*) nameToCheck.utf16();
    WCHAR valid_invalid[MAX_PATH];
    wcscpy_s(valid_invalid, filename);

    int result = PathCleanupSpec(nullptr, valid_invalid);

    // If return value is non-zero, or if 'valid_invalid'
    // is modified, file-name is assumed invalid
    return result == 0 && wcsicmp(valid_invalid, filename)==0;
}

QString Utils::userApplicationDataDirectory()
{
    //
    // Returns the users appdata directory
    //

    if(g_bPortable)
    {
        QDir dir (QCoreApplication::applicationDirPath());
        return dir.absolutePath();
    }


    QString directoryPath (QString("%1")
                           .arg(enviromentValue("APPDATA")));

    directoryPath.append(QLatin1String("/Securepoint SSL VPN"));

    // Replace windows \ with qt /
    directoryPath.replace("\\", "/");

    //
    return directoryPath;
}

QString Utils::enviromentValue(const QString &name)
{
    //
    // Returns the value of the given enviroment variable
    //

    wchar_t *buffer = 0;
    size_t bufferSize = 0;

    QString value;
    // Read the enviroment variable
    if (_wdupenv_s(&buffer, &bufferSize, name.toStdWString().c_str()) == 0) {
        if (bufferSize > 0) {
            // Read and assign value
            // -1 is to remove the ending \0
            value = QString::fromWCharArray(buffer, bufferSize - 1);
        }

        // Cleanup
        delete buffer;
    }

    return value;
}

bool Utils::isUserAdmin ()
{
    struct Data
    {
        PACL   pACL;
        PSID   psidAdmin;
        HANDLE hToken;
        HANDLE hImpersonationToken;
        PSECURITY_DESCRIPTOR     psdAdmin;
        Data() : pACL(NULL), psidAdmin(NULL), hToken(NULL),
            hImpersonationToken(NULL), psdAdmin(NULL)
        {}
        ~Data()
        {
            if (pACL)
                LocalFree(pACL);
            if (psdAdmin)
                LocalFree(psdAdmin);
            if (psidAdmin)
                FreeSid(psidAdmin);
            if (hImpersonationToken)
                CloseHandle (hImpersonationToken);
            if (hToken)
                CloseHandle (hToken);
        }
    } data;

    BOOL   fReturn = FALSE;
    DWORD  dwStatus;
    DWORD  dwAccessMask;
    DWORD  dwAccessDesired;
    DWORD  dwACLSize;
    DWORD  dwStructureSize = sizeof(PRIVILEGE_SET);

    PRIVILEGE_SET   ps;
    GENERIC_MAPPING GenericMapping;
    SID_IDENTIFIER_AUTHORITY SystemSidAuthority = SECURITY_NT_AUTHORITY;

    const DWORD ACCESS_READ  = 1;
    const DWORD ACCESS_WRITE = 2;

    if (!OpenThreadToken (GetCurrentThread(), TOKEN_DUPLICATE|TOKEN_QUERY, TRUE, &data.hToken))
    {
        if (GetLastError() != ERROR_NO_TOKEN) {
            return false;
        }

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE|TOKEN_QUERY, &data.hToken)) {
            return false;
        }
    }

    if (!DuplicateToken (data.hToken, SecurityImpersonation, &data.hImpersonationToken)) {
        return false;
    }

    if (!AllocateAndInitializeSid(&SystemSidAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &data.psidAdmin)) {
        return false;
    }

    data.psdAdmin = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
    if (data.psdAdmin == NULL) {
        return false;
    }

    if (!InitializeSecurityDescriptor(data.psdAdmin, SECURITY_DESCRIPTOR_REVISION)) {
        return false;
    }

    // Compute size needed for the ACL.
    dwACLSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(data.psidAdmin) - sizeof(DWORD);

    data.pACL = (PACL)LocalAlloc(LPTR, dwACLSize);
    if (data.pACL == NULL) {
        return false;
    }

    if (!InitializeAcl(data.pACL, dwACLSize, ACL_REVISION2)) {
        return false;
    }

    dwAccessMask = ACCESS_READ | ACCESS_WRITE;

    if (!AddAccessAllowedAce(data.pACL, ACL_REVISION2, dwAccessMask, data.psidAdmin)) {
        return false;
    }

    if (!SetSecurityDescriptorDacl(data.psdAdmin, TRUE, data.pACL, FALSE)) {
        return false;
    }

    // AccessCheck validates a security descriptor somewhat; set the group
    // and owner so that enough of the security descriptor is filled out
    // to make AccessCheck happy.

    SetSecurityDescriptorGroup(data.psdAdmin, data.psidAdmin, FALSE);
    SetSecurityDescriptorOwner(data.psdAdmin, data.psidAdmin, FALSE);

    if (!IsValidSecurityDescriptor(data.psdAdmin)) {
        return false;
    }

    dwAccessDesired = ACCESS_READ;

    GenericMapping.GenericRead    = ACCESS_READ;
    GenericMapping.GenericWrite   = ACCESS_WRITE;
    GenericMapping.GenericExecute = 0;
    GenericMapping.GenericAll     = ACCESS_READ | ACCESS_WRITE;

    if (!AccessCheck(data.psdAdmin, data.hImpersonationToken, dwAccessDesired, &GenericMapping, &ps, &dwStructureSize, &dwStatus, &fReturn)){
        return false;
    }

    return fReturn;
}

bool Utils::isX64Platform()
{
    //
    // Returns true if we are running on a x64 platform
    // Needed at leas XP wit SP 2
    //

    auto Isx64 = []() -> bool {
        typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

        LPFN_ISWOW64PROCESS fnIsWow64Process;

        BOOL bIsWow64 = FALSE;

        fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(
            GetModuleHandle(TEXT("kernel32")),"IsWow64Process");

        if(NULL != fnIsWow64Process) {
            if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64)) {
                // handle error
            }
        }

        return (bIsWow64 == TRUE);
    };

    return Isx64();
}

bool Utils::isWindows10Platform()
{
    NTSTATUS(WINAPI *RtlGetVersion)(LPOSVERSIONINFOEXW);
    OSVERSIONINFOEXW osInfo;

    *(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion");

    if (RtlGetVersion) {
        osInfo.dwOSVersionInfoSize = sizeof(osInfo);
        RtlGetVersion(&osInfo);
qDebug() << osInfo.dwMajorVersion;
        if (osInfo.dwMajorVersion < 10.0) {
            return false;
        }
    }

    return true;
}


Utils::Utils()
{
}

std::string Utils::GetProcessNameFromPID( DWORD processID )
{
    char szProcessName[MAX_PATH] = ("<unknown>");

    // Get a handle to the process.

    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, processID );

    // Get the process name.

    if (NULL != hProcess )
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod),
             &cbNeeded) )
        {
            GetModuleBaseNameA( hProcess, hMod, szProcessName,
                               sizeof(szProcessName)/sizeof(char) );
        }
    }

    // Release the handle to the process.

    CloseHandle( hProcess );

    return szProcessName;
}


bool Utils::IsProcessRunning(const char* processName)
{
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    unsigned int i;

    if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
    {
        return 1;
    }


    // Calculate how many process identifiers were returned.

    cProcesses = cbNeeded / sizeof(DWORD);

    // Print the name and process identifier for each process.

    for ( i = 0; i < cProcesses; i++ )
    {
        if( aProcesses[i] != 0 )
        {
            if(GetProcessNameFromPID(aProcesses[i]) == processName) {
                return true;
            }
        }
    }

    return false;
}

bool Utils::IsVPNServiceRunning()
{
    SC_HANDLE hScManager =  OpenSCManagerA(0, // local computer or add computer name here
                                         0, // SERVICES_ACTIVE_DATABASE database is opened by default.
                                         GENERIC_READ); // onyl read info
    if(0 != hScManager) {
        SC_HANDLE hSvc = OpenServiceA(hScManager,    // service manager
                                     "Securepoint VPN",     // service name
                                     GENERIC_READ); // onyl read info
        if(0 != hSvc) {
            SERVICE_STATUS_PROCESS sInfo;
            DWORD bytesNeeded = 0;
            //
            if(QueryServiceStatusEx(hSvc,                   // A handle to the service.
                                    SC_STATUS_PROCESS_INFO, // info requested
                                    (LPBYTE)&sInfo,                 // structure to load info to
                                    sizeof(sInfo),          // size of the buffer
                                    &bytesNeeded)) {
                // Default is true, otherwise myReturn is set by IsProcessRunning
                bool myReturn (true);
                //
                if(sInfo.dwCurrentState != SERVICE_RUNNING) {
                    myReturn = IsProcessRunning("SPSSLVpnService.exe");
                }

                // Cleanup service and manager
                CloseServiceHandle(hSvc);
                CloseServiceHandle(hScManager);

                //
                return myReturn;
            }

            // Cleanup service and manager
            CloseServiceHandle(hSvc);
            CloseServiceHandle(hScManager);
        } else {
            // Cleanup service manager
            CloseServiceHandle(hScManager);
        }
    } else {
        // Check the process
        return IsProcessRunning("SPSSLVpnService.exe");
    }

    return false;
}

bool Utils::StartNetman()
{
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (schSCManager == NULL) {
        return FALSE;
    }

    SC_HANDLE schService  = OpenService(schSCManager, L"Netman", SERVICE_ALL_ACCESS);
    if (schService == NULL) {
        CloseServiceHandle(schSCManager);
        return FALSE;
    }


    BOOL ret = StartService(schService, 0, NULL);
    DWORD dwError = GetLastError();

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    return (ret || dwError == ERROR_SERVICE_ALREADY_RUNNING);
}
