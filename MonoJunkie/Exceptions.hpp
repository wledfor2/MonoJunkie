#pragma once

#include <exception>
#include <string>
#include <tchar.h>
#include "Utility.hpp"

//base exception that all of our exceptions derive from, provides a wide character string message
class BaseMonoJunkieException {

private:

	//error message
	std::wstring message;

public:

	//we are operating under the assumption that this conversion will never fail, since it is given by the programmer
	BaseMonoJunkieException(const std::string& message) : BaseMonoJunkieException(NarrowToWide(message)) {
	}

	BaseMonoJunkieException(const std::wstring& message) {
		this->message = message;
	}

	//gets error message
	virtual const wchar_t* what() const {
		return message.c_str();
	}

};

//thrown by MonoInternals on an unrecoverable error
class MonoInternalsException : public BaseMonoJunkieException {
public:
	MonoInternalsException(const std::string& message) : BaseMonoJunkieException(message) {}
	MonoInternalsException(const std::wstring& message) : BaseMonoJunkieException(message) {}
};

//thrown by InjectAssembly on unrecoverable error
class InjectionException : public BaseMonoJunkieException {
public:
	InjectionException(const std::string& message) : BaseMonoJunkieException(message) {}
	InjectionException(const std::wstring& message) : BaseMonoJunkieException(message) {}
};

//thrown by ConfigurationString when conversion fails
class ConfigurationStringException : public BaseMonoJunkieException {
public:
	ConfigurationStringException(const std::wstring& s) : BaseMonoJunkieException(_T("Failed to convert the wide string \"" + s + _T("\" to a narrow string."))) {}
	ConfigurationStringException(const std::string& s) : BaseMonoJunkieException("Failed to convert the narrow string \"" + s + "\" into a wide string") {}
};
