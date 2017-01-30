#pragma once

#include <ostream>
#include <string>
#include <codecvt>
#include <tchar.h>
#include "Exceptions.hpp"

#define COMMAND_LINE_USAGE _T("MonoJunkie -dll <dll name> -namespace <namespace name> -class <class name> -method <method name> -exe <exe name>")

//We wrap all strings in this class for two reasons:
//First, Mono expects UTF-8 narrow character strings (UTF-8 const char*).
//Second, we want to be able to print wide character strings for our convenience.
struct ConfigurationString {

	//wide and narrow version of each string
	std::wstring wide;
	std::string narrow;

	//Create a ConfigurationString with the given wide value, which is also converted to narrow and stored
	ConfigurationString(const std::wstring& wide) {

		//store wide string and convert wide string to a narrow string
		this->wide = wide;
		this->narrow = WideToNarrow(wide);

	}

	//create an empty ConfigurationString (default value)
	ConfigurationString() {
		wide = _T("");
		narrow = "";
	}

	//returns true if the contained wide string is empty
	bool empty() const { return wide.empty(); }

	//convenience operators for getting/setting wide/narrow string
	ConfigurationString* operator=(std::wstring const& s) { update(s); return this; }
	ConfigurationString* operator=(std::string const& s) { update(s); return this; }
	operator std::wstring() { return wide; }
	operator std::string() { return narrow; }
	operator const wchar_t*() { return wide.c_str(); }
	operator const char*() { return narrow.c_str(); }
	
	//convenience comparison operators
	bool operator==(const std::wstring& w) { return wide == w; }
	bool operator==(const std::string& n) { return narrow == n; }
	bool operator==(const wchar_t* w) { return wide == w; }
	bool operator==(const char* n) { return narrow == n; }

private:

	//updates values
	void update(const std::string& s) {

		//NOTE: When visual studio string mode is set to Unicode, std::string is UTF-8, while std::wstring is UCS2/UTF-16
		std::wstring conversionResult = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>("").from_bytes(s);

		//Check if conversion failed
		if (conversionResult != _T("") && s == "") {

			throw ConfigurationStringException(s);

		} else {

			this->narrow = s;
			this->wide = conversionResult;

		}

	}
	void update(const std::wstring& s) {

		//NOTE: When visual studio string mode is set to Unicode, std::string is UTF-8, while std::wstring is UCS2/UTF-16
		std::string conversionResult = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>("").to_bytes(s);

		//Check if conversion failed
		if (conversionResult != "" && s == _T("")) {

			throw ConfigurationStringException(s);

		} else {

			this->narrow = conversionResult;
			this->wide = s;

		}

	}

};

//convenience conatenation 
std::wstring operator+(ConfigurationString& lhs, const std::wstring& rhs) { return lhs.wide + rhs; }
std::string operator+(ConfigurationString& lhs, const std::string& rhs) { return lhs.narrow + rhs; }
std::wstring operator+(const std::wstring& lhs, ConfigurationString& rhs) { return lhs + rhs.wide; }
std::string operator+(const std::string& lhs, ConfigurationString& rhs) { return lhs + rhs.narrow; }

//convenience operators to allow ConfigurationString output via streams
std::wostream& operator<<(std::wostream& out, const ConfigurationString& s) { return (out << s.wide); }
std::ostream& operator<<(std::ostream& out, const ConfigurationString& s) { return (out << s.narrow); }

//Injection configuration used by various functions. Parsed by ParseCommandLine.
//All strings are wrapped in ConfigurationString. See above for why!
struct Configuration {

	//namespace the DLL initialization class resides in
	ConfigurationString targetNamespace;

	//class the DLL initialization method resides in, or the class the static method the user wants to call is in.
	ConfigurationString targetClass;

	//name of the DLL initialization method, or the static method the user wants to call.
	ConfigurationString targetMethod;

	//name/path to of the target EXE we are injecting into
	ConfigurationString targetProcessEXE;

	//path/filename to the DLL we are injecting
	ConfigurationString assemblyPath;
	ConfigurationString assemblyFileName;

	//true if an error occurred during parsing. If true, only hadError and errorString is guaranteed to be valid. Everything else is undefined.
	bool hadError = false;

	//Error message. Only valid when hadError is true.
	std::wstring errorString;

	//convenience function that flags an error and sets the error message
	void onError(const std::wstring& errs) {
		hadError = true;
		errorString = errs;
	}

};
