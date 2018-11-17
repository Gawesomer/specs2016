#ifndef SPECS2016__SPECITEMS__ITEM__H
#define SPECS2016__SPECITEMS__ITEM__H

#include <vector>
#include <string>
#include "cli/tokens.h"
#include "processing/conversions.h"
#include "processing/StringBuilder.h"
#include "processing/ProcessingState.h"

class InputPart {
public:
	virtual ~InputPart() {}
	virtual std::string Debug() = 0;
	virtual PSpecString getStr(ProcessingState& pState) = 0;
};

class LiteralPart : public InputPart {
public:
	LiteralPart(std::string& s) {m_Str = s;}
	virtual ~LiteralPart() {}
	virtual std::string Debug();
	virtual PSpecString getStr(ProcessingState& pState);
private:
	std::string m_Str;
};

class RangePart : public InputPart {
public:
	RangePart(int _first, int _last) {_from=_first; _to=_last;}
	virtual ~RangePart() {}
	virtual std::string Debug();
	virtual PSpecString getStr(ProcessingState& pState) = 0;
protected:
	int _from;
	int _to;
};

class RegularRangePart : public RangePart {
public:
	RegularRangePart(int _first, int _last) : RangePart(_first,_last) {}
	virtual ~RegularRangePart() {}
	virtual std::string Debug();
	virtual PSpecString getStr(ProcessingState& pState);
};

class WordRangePart : public RangePart {
public:
	WordRangePart(int _first, int _last, char sep=0) : RangePart(_first,_last) {m_WordSep = sep;}
	virtual ~WordRangePart() {}
	virtual std::string Debug();
	virtual PSpecString getStr(ProcessingState& pState);
private:
	char m_WordSep;
};

class FieldRangePart : public RangePart {
public:
	FieldRangePart(int _first, int _last, char sep=0) : RangePart(_first,_last) {m_FieldSep = sep;}
	virtual ~FieldRangePart() {}
	virtual std::string Debug();
	virtual PSpecString getStr(ProcessingState& pState);
private:
	char m_FieldSep;
};

class SubstringPart : public InputPart {
public:
	SubstringPart(RangePart* _sub, InputPart* _big) {mp_SubPart = _sub; mp_BigPart = _big;}
	virtual ~SubstringPart();
	virtual std::string Debug();
	virtual PSpecString getStr(ProcessingState& pState);
private:
	RangePart* mp_SubPart;
	InputPart* mp_BigPart;
};

#define NUMBER_PART_FIELD_LEN  10
class NumberPart : public InputPart {
public:
	NumberPart() {m_Num = 0;}
	virtual ~NumberPart() {}
	virtual std::string Debug();
	virtual PSpecString getStr(ProcessingState& pState);
private:
	unsigned long m_Num;
};


enum ApplyRet {
	ApplyRet__Continue,
	ApplyRet__Write,
	ApplyRet__Read,
	ApplyRet__ReadStop,
	ApplyRet__Last
};

#define POS_SPECIAL_VALUE_NEXTWORD  0xffffffff
#define POS_SPECIAL_VALUE_NEXTFIELD 0xfffffffe
#define POS_SPECIAL_VALUE_NEXT      0xfffffffd
#define MAX_OUTPUT_POSITION         65536

class Item {
public:
	virtual ~Item() {}
	virtual std::string Debug() = 0;
	virtual ApplyRet apply(ProcessingState& pState, StringBuilder* pSB) = 0;
};

typedef Item* PItem;

class DataField : public Item {
public:
	DataField();
	virtual ~DataField();
	void parse(std::vector<Token> &tokenVec, unsigned int& index);
	virtual std::string Debug();
	virtual ApplyRet apply(ProcessingState& pState, StringBuilder* pSB);
private:
	InputPart* getInputPart(std::vector<Token> &tokenVec, unsigned int& index, char _wordSep=0, char _fieldSep=0);
	SubstringPart* getSubstringPart(std::vector<Token> &tokenVec, unsigned int& index);
	void stripString(PSpecString &pOrig);
	char m_label;
	InputPart *m_InputPart;
	size_t m_outStart;  /* zero is a special value meaning no output */
	size_t m_maxLength; /* zero is a special value meaning no limit  */
	bool   m_strip;
	StringConversions m_conversion;
	outputAlignment m_alignment;
};

class TokenItem : public Item {
public:
	TokenItem(Token& t);
	virtual ~TokenItem();
	Token* getToken()   {return mp_Token;}
	virtual std::string Debug();
	virtual ApplyRet apply(ProcessingState& pState, StringBuilder* pSB);
private:
	Token* mp_Token;
};

#endif
