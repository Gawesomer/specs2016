#include <assert.h>
#include <string.h>  // for memcpy
#include "../processing/Config.h"
#include "../utils/ErrorReporting.h"
#include "SpecString.h"

void StdSpecString::Overlay(PSpecString pss, size_t offset, void* pPadChar)
{
	StdSpecString* pSmallString = dynamic_cast<StdSpecString*>(pss);
	assert(pSmallString!=NULL);

	size_t endPos = offset + pSmallString->length();
	if (endPos > m_str.length()) {
		char padChar = *((char*)pPadChar);
		m_str.resize(endPos, padChar);
	}

	memcpy((void*)(m_str.c_str() + offset), pSmallString->data(), pSmallString->length());
}

void StdSpecString::Overlay(SpecString& ss, size_t offset, void* pPadChar)
{
	Overlay(&ss, offset, pPadChar);
}

void StdSpecString::_serialize(std::ostream& os) const
{
	static char asciiXlate[] = "................................ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~................................................................................................................................";
	if (g_bOutputTranslatedAscii) {
		for (size_t i = 0; i < m_str.length(); i++) os << asciiXlate[(unsigned char)(m_str[i])];
	} else {
		os << m_str;
	}
}

PSpecString SpecString::newString()
{
	if (g_bSupportUTF8) {
		MYTHROW("UTF-8 not supported yet");
		return NULL; // appease the compiler warning
	} else {
		return new StdSpecString();
	}
}

PSpecString SpecString::newString(const char* pstrz)
{
	if (g_bSupportUTF8) {
		MYTHROW("UTF-8 not supported yet");
		return NULL; // appease the compiler warning
	} else {
		return new StdSpecString(pstrz);
	}
}

PSpecString SpecString::newString(const char* pstrz, size_t len)
{
	if (g_bSupportUTF8) {
		MYTHROW("UTF-8 not supported yet");
		return NULL; // appease the compiler warning
	} else {
		return new StdSpecString(pstrz, len);
	}
}

