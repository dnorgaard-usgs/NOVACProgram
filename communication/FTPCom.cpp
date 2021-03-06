#include "StdAfx.h"
#include "ftpcom.h"
#include "../Common/common.h"
#include "Winsock2.h"

using namespace Communication;

CFTPCom::CFTPCom(void)
{
    m_FtpConnection = nullptr;
    m_InternetSession = nullptr;
}

CFTPCom::~CFTPCom(void)
{
}

//Connect to FTP server
//return 0 - fail
//return 1 - success
//return 2 - ftp address parsing problem
//return 3 - can not connect to internet
//return 4 - ftp exception
int CFTPCom::Connect(LPCTSTR siteName, LPCTSTR userName, LPCTSTR password, int timeout, int portNumber, bool passiveMode)
{
    INTERNET_PORT port = portNumber;
    DWORD dwServiceType = AFX_INET_SERVICE_FTP;
    CString strServer;
    CString strObject;
    CString urlAddress = _T("ftp://");
    urlAddress += siteName;
    m_FTPSite.Format("%s", siteName);

    // If already connected, then re-connect
    if (m_FtpConnection != nullptr)
    {
        m_FtpConnection->Close();
        delete m_FtpConnection;
        m_FtpConnection = nullptr;
    }

    if (!AfxParseURL(siteName, dwServiceType, (CString)siteName, strObject, port))
    {
        // try adding the "ftp://" protocol

        if (!AfxParseURL(urlAddress, dwServiceType, (CString)siteName, strObject, port))
        {
            m_ErrorMsg = TEXT("Can not parse  ftp address");
            ShowMessage(m_ErrorMsg);
            return 2;
        }
    }

    if (m_InternetSession == nullptr)
    {
        m_InternetSession = new CInternetSession(nullptr, 1, INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    }

    // Alert the user if the internet session could
    // not be started and close app
    if (!m_InternetSession)
    {
        m_ErrorMsg = TEXT("FAIL TO CONNECT INTERNET");
        ShowMessage(m_ErrorMsg);
        return 3;
    }

    if (dwServiceType == INTERNET_SERVICE_FTP)// && !siteName.IsEmpty())
    {
        try
        {
            m_InternetSession->SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, timeout * 1000);
            m_InternetSession->SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, timeout * 1000);
            m_InternetSession->SetOption(INTERNET_OPTION_SEND_TIMEOUT, timeout * 1000);

            BOOL bPassiveMode = (passiveMode) ? TRUE : FALSE;
            m_FtpConnection = m_InternetSession->GetFtpConnection(siteName, userName, password, portNumber, bPassiveMode);
            //m_ErrorMsg.Format("CONNECTED to FTP server: %s", siteName);
            //ShowMessage(m_ErrorMsg);
            return 1;
        }
        catch (CInternetException* pEx)
        {
            // catch errors from WinINet
            TCHAR szErr[255];
            if (pEx->GetErrorMessage(szErr, 255))
            {
                m_ErrorMsg.Format("FTP error from connecting %s: %s", (LPCSTR)m_FTPSite, (LPCSTR)szErr);
                ShowMessage(m_ErrorMsg);
            }
            else
            {
                m_ErrorMsg.Format("FTP exception");
                ShowMessage(m_ErrorMsg);
            }
            pEx->Delete();

            m_FtpConnection = nullptr;
            return 4;
        }
    }
    return 1;
}

int CFTPCom::Disconnect()
{
    if (m_FtpConnection != nullptr)
    {
        m_FtpConnection->Close();
        delete m_FtpConnection;
        m_FtpConnection = nullptr;
    }

    if (m_InternetSession != nullptr)
    {
        m_InternetSession->Close();
        delete m_InternetSession;
        m_InternetSession = nullptr;
    }

    return 1;
}


int CFTPCom::UpdateRemoteFile(LPCTSTR localFile, LPCTSTR remoteFile)
{
    if (m_FtpConnection == nullptr)
    {
        ShowMessage("ERROR: Attempted to update file using FTP while not connected!");
        return 0;
    }

    // If the file exists, remove it first...
    if (FindFile((CString&)remoteFile))
    {
        m_FtpConnection->Remove(remoteFile);
    }

    // Upload the file
    return m_FtpConnection->PutFile(localFile, remoteFile);
}

bool CFTPCom::DownloadAFile(LPCTSTR remoteFile, LPCTSTR fileFullName)
{
    // Check that we're connected...
    if (m_FtpConnection == nullptr)
    {
        ShowMessage("ERROR: Attempted to upload file using FTP while not connected!");
        return false;
    }

    CString msg;
    msg.Format("Trying to download %s", fileFullName);
    ShowMessage(msg);

    try
    {
        // Try to download the file
        BOOL r = m_FtpConnection->GetFile(remoteFile, fileFullName, FALSE, FILE_ATTRIBUTE_NORMAL, FTP_TRANSFER_TYPE_BINARY | INTERNET_FLAG_RELOAD);
        bool result = (r == TRUE);

        if (!result)
        { 
            // this means something went wrong
            int ftpError = GetLastError();
            if (ftpError != 0)
            {
                msg.Format("FTP error happened when downloading %s from %s: %d", (LPCSTR)fileFullName, (LPCSTR)m_FTPSite, ftpError);
                ShowMessage(msg);
                DWORD code;
                DWORD size_needed = 0;
                InternetGetLastResponseInfo(&code, nullptr, &size_needed);
                char *message = (char*)malloc(size_needed + 1);
                InternetGetLastResponseInfo(&code, message, &size_needed);
                msg.Format("Error message :%s", message);
                ShowMessage(msg);
            }
        }
        else {
            // SUCCESS!!
            msg.Format("Finish downloading %s", fileFullName);
            ShowMessage(msg);
            return result;
        }
    }
    catch (CInternetException* pEx)
    {
        // catch errors from WinINet
        TCHAR szErr[255];
        if (pEx->GetErrorMessage(szErr, 255))
        {
            m_ErrorMsg.Format("FTP error happened when downloading %s from %s: %s", (LPCSTR)fileFullName, (LPCSTR)m_FTPSite, (LPCSTR)szErr);
            ShowMessage(m_ErrorMsg);
        }
        else
        {
            m_ErrorMsg.Format("FTP exception");
            ShowMessage(m_ErrorMsg);
        }
        pEx->Delete();

        m_FtpConnection = nullptr;

        return false;
    }

    return false;
}

int CFTPCom::UploadFile(LPCTSTR localFile, LPCTSTR remoteFile)
{
    int result;

    // Check that we are connected
    if (m_FtpConnection == nullptr)
    {
        ShowMessage("ERROR: Attempted to upload file using FTP while not connected!");
        return 0;
    }

    // See if we can find the file on the remote computer, if so
    //	then we can't upload it...
    if (FindFile((CString&)remoteFile))
    {
        return 1;
    }

    try {
        // Try to upload the file
        result = m_FtpConnection->PutFile(localFile, remoteFile);

    }
    catch (CInternetException *pEx) {
        // catch errors from WinINet
        TCHAR szErr[255];
        if (pEx->GetErrorMessage(szErr, 255))
        {
            m_ErrorMsg.Format("FTP error happened when uploading %s to %s: %s", (LPCSTR)localFile, (LPCSTR)m_FTPSite, (LPCSTR)szErr);
            ShowMessage(m_ErrorMsg);
        }
        else
        {
            m_ErrorMsg.Format("FTP exception");
            ShowMessage(m_ErrorMsg);
        }
        pEx->Delete();
    }
    return result;
}

int CFTPCom::CreateDirectory(LPCTSTR remoteDirectory)
{
    // Check that we are connected
    if (m_FtpConnection == nullptr) {
        ShowMessage("ERROR: Attempted to create directory using FTP while not connected!");
        return 0;
    }

    int result = m_FtpConnection->CreateDirectory(remoteDirectory);
    return result;
}

bool CFTPCom::SetCurDirectory(CString curDirName)
{
    if (m_FtpConnection == nullptr) {
        ShowMessage("ERROR: Attempted to set directory using FTP while not connected!");
        return false; // cannot connect...
    }

    BOOL result = m_FtpConnection->SetCurrentDirectory(curDirName);
    return (result == TRUE);
}

bool CFTPCom::FindFile(const CString& fileName)
{
    if (m_FtpConnection == nullptr)
    {
        ShowMessage("ERROR: Attempted to find file using FTP while not connected!");
        return false; // cannot connect...
    }

    // use a file find object to enumerate files
    CFtpFileFind finder(m_FtpConnection);
    BOOL result = finder.FindFile(_T(fileName));
    if (0 == result)
    {
        DWORD retcode = GetLastError();
        //ShowMessage("Could not find remote file");
        return false;
    }

    return true;
}

// @return 0 if fail...
BOOL CFTPCom::DeleteFolder(const CString& folder)
{
    // Check
    if (m_FtpConnection == nullptr) {
        ShowMessage("ERROR: Attempted to delete folder using FTP while not connected");
        return 0;
    }

    // Remove the directory
    BOOL result = m_FtpConnection->RemoveDirectory(folder);
    return result;
}

BOOL CFTPCom::EnterFolder(const CString& folder)
{
    // Check...
    if (m_FtpConnection == nullptr)
    {
        ShowMessage("ERROR: Attempted to enter folder using FTP while not connected");
        return FALSE;
    }

    // Set the current directory, this returns 0 on failure.
    if (0 == m_FtpConnection->SetCurrentDirectory(folder))
    {
        return FALSE;
    }

    // Get the current directory, this returns 0 on failure.
    CString currentFolder;
    if (0 == m_FtpConnection->GetCurrentDirectory(currentFolder))
    {
        return FALSE;
    }

    // The top-level directory is treated separately due to different responses from different instruments
    if (Equals(folder, "//"))
    {
        if (Equals(currentFolder, "/") || Equals(currentFolder, "//"))
        {
            CString msg;
            msg.Format("Get into folder %s", (LPCSTR)folder);
            ShowMessage(msg);
            return TRUE;
        }
        else
        {
            CString msg;
            msg.Format("Can not get into folder %s", (LPCSTR)folder);
            ShowMessage(msg);
            return FALSE;
        }
    }

    // The response we want to have...
    CString folderResponseOption1, folderResponseOption2;
    folderResponseOption1.Format("/%s/", (LPCSTR)folder);
    folderResponseOption2.Format("/%s", (LPCSTR)folder);

    // Compare if the returned string is the same as what we want...
    // If a relative directory is passed into this function
    // the Equals below will compare it agains a full path and return false.
    // Misleading 'Can not get into folder' message displays even if the
    // change directory was successful. Best to handle messages externally.
    if (Equals(currentFolder, folderResponseOption1) ||
        Equals(currentFolder, folderResponseOption2))
    {
        CString msg;
        msg.Format("Get into folder %s", (LPCSTR)folder);
        ShowMessage(msg);
        return TRUE;
    }
    else
    {
        CString msg;
        msg.Format("Can not get into folder %s", (LPCSTR)folder);
        ShowMessage(msg);
        return FALSE;
    }
}

BOOL CFTPCom::GotoTopDirectory()
{
    CString folder("//");
    return EnterFolder(folder);
}

void CFTPCom::ReadResponse(CInternetFile* file)
{
    char readBuf[256];
    CString str, restStr;
    unsigned int rd = 0;

    // Check input-parameter...
    if (file == nullptr) {
        return;
    }

    ULONGLONG a = file->GetLength();
    do
    {
        rd = file->Read(readBuf, 256);
        if (rd > 0)
        {
            str.Format("%s", readBuf);
            restStr.Append(str);
        }
    } while (rd > 0);
}
