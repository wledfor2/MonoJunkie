#include <Windows.h>
#include <string>
#include <tchar.h>
#include <cstdlib>
#include <sstream>
#include <codecvt>
#include "Utility.hpp"

bool FileExists(const std::wstring& path) {

	//Retrieve win32 file attributes for the given path
	DWORD fileAttributes = GetFileAttributesW(path.c_str());

	//the file exists if the path is short enough (using MAX_PATH to be safe -- some methods accept larger), exists, and is not a directory.
	return path.length() < MAX_PATH && fileAttributes != INVALID_FILE_ATTRIBUTES && !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY);

}

//get error message for a Windows API error
std::wstring GetLastErrorString() {

	LPWSTR errorMessage;
	DWORD errorCode;

	//get the error that occurred
	errorCode = GetLastError();

	//get the static error message for the error
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errorMessage, 0, NULL);

	//get a a copy of the error message as a C++ string
	std::wstring errs = errorMessage;

	//Destroy native string holding the error message, we're done with it
	LocalFree(errorMessage);

	return errs;

}

//get error message for a Windows kernel error
std::wstring GetNTErrorString(NTSTATUS errorCode) {

	//Load NTDLL, where kernel error messages are described
	HMODULE messagesHandle = LoadLibraryW(_T("NTDLL.DLL"));

	//buffer allocated by FormatMessageW, we must free when done
	LPWSTR errorMessage;

	//get the static error message for the error
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS, messagesHandle, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errorMessage, 0, NULL);

	//get a a copy of the error message as a C++ string
	std::wstring errs = errorMessage;

	//Destroy native string holding the error message, we're done with it
	LocalFree(errorMessage);

	//Close handle to NTDLL
	FreeLibrary(messagesHandle);

	return errs;

}

//get error message for a Windows kernel error
std::string GetNTErrorStringA(NTSTATUS errorCode) {

	//Load NTDLL, where kernel error messages are described
	HMODULE messagesHandle = LoadLibraryW(_T("NTDLL.DLL"));

	//buffer allocated by FormatMessageW, we must free when done
	LPSTR errorMessage;

	//get the static error message for the error
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS, messagesHandle, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &errorMessage, 0, NULL);

	//get a a copy of the error message as a C++ string
	std::string errs = errorMessage;

	//Destroy native string holding the error message, we're done with it
	LocalFree(errorMessage);

	//Close handle to NTDLL
	FreeLibrary(messagesHandle);

	return errs;

}

std::wstring GetAbsolutePath(const std::wstring& relativePath) {

	//C++ string with the absolute path of the given relative path, or an empty C++ string if the function fails
	std::wstring absolutePath = _T("");

	//the absolute path passed out by 
	wchar_t rawAbsolutePath[MAX_PATH];

	//Get the absolute path from the given relative path
	if (GetFullPathNameW(relativePath.c_str(), MAX_PATH, rawAbsolutePath, NULL)) {

		//copy C absolute path to C++ string
		absolutePath = rawAbsolutePath;

	}

	//return the absolute path, or "" if no absolute path was retrieved.
	return absolutePath;

}

std::wstring GetFileName(const std::wstring& path) {

	//stream to build filename output
	std::wstringstream out(_T(""));

	//allocate buffers for filename and extension
	wchar_t* fname = new wchar_t[_MAX_FNAME];
	wchar_t* fext = new wchar_t[_MAX_EXT];

	//split the given path into filename and extension
	if (!_wsplitpath_s(path.c_str(), NULL, 0, NULL, 0, fname, _MAX_FNAME, fext, _MAX_EXT)) {

		//append extension to filename
		out << fname << fext;

	}

	//cleanup
	delete fname;
	delete fext;

	return out.str();

}

std::wstring NarrowToWide(const std::string& s) {

	//NOTE: When visual studio string mode is set to Unicode, std::string is UTF-8, while std::wstring is UCS2/UTF-16
	return std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>("").from_bytes(s);

}

std::string WideToNarrow(const std::wstring& s) {

	//NOTE: When visual studio string mode is set to Unicode, std::string is UTF-8, while std::wstring is UCS2/UTF-16
	return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>("").to_bytes(s);

}
