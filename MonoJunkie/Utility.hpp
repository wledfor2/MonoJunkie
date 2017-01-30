#pragma once

#include <Windows.h>
#include <iomanip>
#include <sstream>
#include <string>

#define __DELETE_IF_NOT_NULL(v) \
	if (v != nullptr) { \
		delete v; \
	}

#define __LOG_ADDRESS(name, var) (std::wcout << name << _T(": ") << ToHex(var) << std::endl)

#ifdef _WIN64
#define HEX_WIDTH 16
#else
#define HEX_WIDTH 8
#endif

//Returns true if the given file exists and is not a directory
bool FileExists(const std::wstring&);

//Gets the last win32 error message as a C++ string
std::wstring GetLastErrorString();

//Gets the absolute path for the given relative path, or returns an empty string on failure
std::wstring GetAbsolutePath(const std::wstring&);

//Get filename from path, returns an empty string if filename could not be extracted
std::wstring GetFileName(const std::wstring&);

//Similar to GetLastErrorString, except it takes a Kernel NTSTATUS as an argument, and returns a corresponding error message describing it.
std::wstring GetNTErrorString(NTSTATUS);
std::string GetNTErrorStringA(NTSTATUS);

//converts an std::string to an std::wstring, or an empty string on error
std::wstring NarrowToWide(const std::string&);

//converts an std::wstring to std::string and returns the result, or an empty string on error
std::string WideToNarrow(const std::wstring&);

//converts a value of type T to a string representing the hexadecimal value, or an empty string if it could not be converted
template<typename T> std::wstring ToHex(const T& value) {

	std::wostringstream out;

	//string as hex, or an empty string if it could not be converted
	std::wstring s = _T("");

	//Attempt to convert the given value to hex
	if (out << _T("0x") << std::hex << std::uppercase << std::setw(HEX_WIDTH) << std::setfill(_T('0')) << value) {

		//conversion worked, return the hex
		s = out.str();

	}

	//return the hex or empty string
	return s;

}