#include "utils/ErrorReporting.h"
#include "processing/Config.h"
#include "specItems.h"

ALUCounters g_counters;
itemGroup::itemGroup()
{
	m_items.clear();
	bNeedRunoutCycle = false;
	bFoundSelectSecond = false;
}

void itemGroup::addItem(Item *pItem)
{
	m_items.insert(m_items.end(), pItem);
}

void itemGroup::Compile(std::vector<Token> &tokenVec, unsigned int& index)
{
	while (index < tokenVec.size()) {
		switch (tokenVec[index].Type()) {
		case TokenListType__EOF:
			bNeedRunoutCycle = true;
		case TokenListType__FIELDSEPARATOR:
		case TokenListType__WORDSEPARATOR:
		case TokenListType__PAD:
		case TokenListType__READ:
		case TokenListType__READSTOP:
		case TokenListType__WRITE:
		{
			TokenItem *pItem = new TokenItem(tokenVec[index++]);
			addItem(pItem);
			break;
		}
			/* check here for other types of items, whether plainItem, ifGroup, whileGroup */
		case TokenListType__RANGELABEL:
		case TokenListType__RANGE:
		case TokenListType__WORDRANGE:
		case TokenListType__FIELDRANGE:
		case TokenListType__LITERAL:
		case TokenListType__SUBSTRING:
		case TokenListType__NUMBER:
		case TokenListType__TODCLOCK:
		case TokenListType__DTODCLOCK:
		case TokenListType__ID:
		case TokenListType__PRINT:
		{
			DataField *pItem = new DataField;
			pItem->parse(tokenVec, index);
			addItem(pItem);
			break;
		}
		case TokenListType__SET:
		{
			MYASSERT(index < tokenVec.size());
			try {
				SetItem* pItem = new SetItem(tokenVec[index].Literal());
				index++;
				addItem(pItem);
			} catch(const SpecsException& e) {
				if (g_bVerbose) {
					std::cerr << "While parsing statement, got: " << e.what(true) << "\n";
				}
				std::string err = "Error in statement in "+ tokenVec[index].HelpIdentify();
				MYTHROW(err);
			}
			break;
		}
		case TokenListType__IF:
		case TokenListType__ELSEIF:
		case TokenListType__WHILE:
		{
			MYASSERT(index < tokenVec.size());
			if (TokenListType__WHILE == tokenVec[index].Type()) {
				MYASSERT(TokenListType__DO == tokenVec[index+1].Type());
			} else {
				MYASSERT(TokenListType__THEN == tokenVec[index+1].Type());
			}
			try {
				ConditionItem* pItem = new ConditionItem(tokenVec[index].Literal());
				if (TokenListType__ELSEIF == tokenVec[index].Type()) {
					pItem->setElseIf();
				}
				else if (TokenListType__WHILE == tokenVec[index].Type()) {
					pItem->setWhile();
				}
				index++;
				addItem(pItem);
			} catch (const SpecsException& e) {
				if (g_bVerbose) {
					std::cerr << "While parsing conditional expression, got: " << e.what(true) << "\n";
				}
				std::string err = "Error in conditional expression " + tokenVec[index].HelpIdentify();
				MYTHROW(err);
			}
			break;
		}
		case TokenListType__THEN:
		{
			ConditionItem* pItem = new ConditionItem(ConditionItem::PRED_THEN);
			index++;
			addItem(pItem);
			break;
		}
		case TokenListType__ELSE:
		{
			ConditionItem* pItem = new ConditionItem(ConditionItem::PRED_ELSE);
			index++;
			addItem(pItem);
			break;
		}
		case TokenListType__ENDIF:
		{
			ConditionItem* pItem = new ConditionItem(ConditionItem::PRED_ENDIF);
			index++;
			addItem(pItem);
			break;
		}
		case TokenListType__DO:
		{
			ConditionItem* pItem = new ConditionItem(ConditionItem::PRED_DO);
			index++;
			addItem(pItem);
			break;
		}
		case TokenListType__DONE:
		{
			ConditionItem* pItem = new ConditionItem(ConditionItem::PRED_DONE);
			index++;
			addItem(pItem);
			break;
		}
		default:
			std::string err = std::string("Unhandled token type ")
				+ TokenListType__2str(tokenVec[index].Type())
				+ " at argument " + std::to_string(tokenVec[index].argIndex());
			MYTHROW(err);
		}
	}
}

std::string itemGroup::Debug()
{
	std::string ret = "itemGroup has " + std::to_string(m_items.size()) + " items:\n";
	int idx = 1;
	for (std::vector<PItem>::iterator it = m_items.begin(); it != m_items.end(); it++, idx++) {
		PItem pIt = *it;
		ret += std::to_string(idx) + ". ";
		ret += pIt->Debug();
		ret += "\n";
	}

	return ret;
}

bool itemGroup::processDo(StringBuilder& sb, ProcessingState& pState, Reader* pRd, Writer* pWr)
{
	bool bSomethingWasDone = false;
	int i = 0;
	bool isEOFCycle = false;
	PSpecString ps; // Used for processing READ and READSTOP tokens
	bool processingContinue = true;

	if (pState.isRunOut()) {
		// Find the EOF token
		for ( ;  i < m_items.size(); i++) {
			TokenItem* pTok = dynamic_cast<TokenItem*>(m_items[i]);
			if (pTok && (TokenListType__EOF == pTok->getToken()->Type())) {
				i++;  // So we start with the one after the EOF
				break;
			}
		}
		isEOFCycle = true;
	}

	itemLoop:
	processingContinue = true;
	for ( ; processingContinue && i<m_items.size(); i++) {
		PItem pit = m_items[i];
		if (!pit->ApplyUnconditionally() && !pState.needToEvaluate()) {
			continue;
		}
		ApplyRet aRet = pit->apply(pState, &sb);
		switch (aRet) {
		case ApplyRet__Continue:
			bSomethingWasDone = true;
			break;
		case ApplyRet__Write:
			if (bSomethingWasDone) {
				pWr->Write(sb.GetString());
			} else {
				pWr->Write(SpecString::newString());
			}
			bSomethingWasDone = false;
			break;
		case ApplyRet__Read:
		case ApplyRet__ReadStop:
			ps = pRd->get();
			if (!ps) {
				if (aRet==ApplyRet__Read) {
					ps = SpecString::newString();
				} else {
					processingContinue = false; // Stop processing if no extra record is available
				}
			}
			pState.setString(ps);
			break;
		case ApplyRet__EnterLoop:
			pState.pushLoop(i);
			break;
		case ApplyRet__DoneLoop:
			i = pState.getLoopStart() - 1;  // subtracting 1 because the for loop increments
			break;
		case ApplyRet__EOF:
			processingContinue = false;
			break;
		default:
			std::string err = "Unexpected return code from TokenItem::apply: ";
			err += std::to_string(aRet);
			MYTHROW(err);
		}
	}

	if (isEOFCycle && bFoundSelectSecond) {
		isEOFCycle = false;
		i = 0;
		goto itemLoop;
	}
	return bSomethingWasDone;
}

void itemGroup::process(StringBuilder& sb, ProcessingState& pState, Reader& rd, Writer& wr)
{
	PSpecString ps;

	while ((ps=rd.get())) {
		pState.setString(ps);
		pState.incrementCycleCounter();

		try {
			if (processDo(sb,pState, &rd, &wr)) {
				wr.Write(sb.GetString());
			}
		} catch (const SpecsException& e) {
			std::cerr << "Exception processing line " << rd.countUsed() << ": " << e.what(true) << "\n";
		}
	}

	if (!bNeedRunoutCycle) {
		return;
	}

	// run-out cycle
	pState.setString(NULL);
	try {
		if (processDo(sb, pState, &rd, &wr)) {
			wr.Write(sb.GetString());
		}
	} catch (const SpecsException& e) {
		std::cerr << "Exception processing the run-out cycle: " << e.what(true) << "\n";
	}
}

bool itemGroup::readsLines()
{
	for (PItem pItem : m_items) {
		if (pItem->readsLines()) return true;
	}
	return false;
}




TokenItem::TokenItem(Token& t)
{
	mp_Token = new Token(t);
}

TokenItem::~TokenItem()
{
	if (mp_Token) {
		delete mp_Token;
		mp_Token = NULL;
	}
}

std::string TokenItem::Debug()
{
	return mp_Token->Debug();
}

ApplyRet TokenItem::apply(ProcessingState& pState, StringBuilder* pSB)
{
	switch (mp_Token->Type()) {
	case TokenListType__FIELDSEPARATOR:
		pState.setFSChar(mp_Token->Literal()[0]);
		return ApplyRet__Continue;
	case TokenListType__WORDSEPARATOR:
		pState.setWSChar(mp_Token->Literal()[0]);
		return ApplyRet__Continue;
	case TokenListType__PAD:
		pState.setPadChar(mp_Token->Literal()[0]);
		return ApplyRet__Continue;
	case TokenListType__READ:
		return ApplyRet__Read;
	case TokenListType__READSTOP:
		return ApplyRet__ReadStop;
	case TokenListType__WRITE:
		return ApplyRet__Write;
	case TokenListType__EOF:
		return ApplyRet__EOF;
	default:
		std::string err = "Unhandled TokenItem type " + TokenListType__2str(mp_Token->Type());
		MYTHROW(err);
	}
}


SetItem::SetItem(std::string& _statement)
{
	m_rawExpression = _statement;
	AluVec expr;
	MYASSERT(parseAluStatement(_statement, m_key, &m_oper, expr));
	MYASSERT(convertAluVecToPostfix(expr, m_RPNExpression, true));
}

SetItem::~SetItem()
{
	for (AluUnit* unit : m_RPNExpression) {
		delete unit;
	}
}

ApplyRet SetItem::apply(ProcessingState& pState, StringBuilder* pSB)
{
	ALUPerformAssignment(m_key, &m_oper, m_RPNExpression, &g_counters);
	return ApplyRet__Continue;
}


ConditionItem::ConditionItem(std::string& _statement)
{
	m_pred = PRED_IF;
	m_rawExpression = _statement;
	AluVec expr;
	MYASSERT(parseAluExpression(_statement, expr));
	MYASSERT(convertAluVecToPostfix(expr, m_RPNExpression, true));
}

ConditionItem::ConditionItem(ConditionItem::predicate _p)
{
	MYASSERT(_p != PRED_IF);
	m_pred = _p;
}

ConditionItem::~ConditionItem()
{
	if (m_pred == PRED_IF) {
		for (AluUnit* unit : m_RPNExpression) {
			delete unit;
		}
	}
}

std::string ConditionItem::Debug()
{
	static const std::string sThen("THEN");
	static const std::string sElse("ELSE");
	static const std::string sEndIf("ENDIF");
	static const std::string sDo("DO");
	static const std::string sDone("DONE");

	switch(m_pred) {
	case PRED_THEN:
		return sThen;
	case PRED_ELSE:
		return sElse;
	case PRED_ENDIF:
		return sEndIf;
	case PRED_DO:
		return sDo;
	case PRED_DONE:
		return sDone;
	case PRED_IF: {
		std::string ret = "IF(" + m_rawExpression + ")";
		return ret;
	}
	case PRED_ELSEIF: {
		std::string ret = "ELSEIF(" + m_rawExpression + ")";
		return ret;
	}
	case PRED_WHILE: {
		std::string ret = "WHILE(" + m_rawExpression + ")";
		return ret;
	}
	}
	return std::string(""); // Issue #14
}

void ConditionItem::setElseIf()
{
	MYASSERT(PRED_IF == m_pred);
	m_pred = PRED_ELSEIF;
}

void ConditionItem::setWhile()
{
	MYASSERT(PRED_IF == m_pred);
	m_pred = PRED_WHILE;
}

ApplyRet ConditionItem::apply(ProcessingState& pState, StringBuilder* pSB)
{
	ApplyRet ret = ApplyRet__Continue;

	switch (m_pred) {
	case PRED_IF: {
		if (pState.needToEvaluate()) {
			ALUValue* exprResult = evaluateExpression(m_RPNExpression, &g_counters);
			pState.setCondition(exprResult->getBool());
			delete exprResult;
		} else {
			pState.observeIf();
		}
		break;
	}
	case PRED_THEN:
		break;
	case PRED_ELSE:
		pState.observeElse();
		break;
	case PRED_ELSEIF: {
		bool bNeedToEvaluate;
		pState.observeElseIf(bNeedToEvaluate);
		if (bNeedToEvaluate) {
			ALUValue* exprResult = evaluateExpression(m_RPNExpression, &g_counters);
			pState.setCondition(exprResult->getBool());
			delete exprResult;
		}
		break;
	}
	case PRED_ENDIF:
		pState.observeEndIf();
		break;
	case PRED_WHILE: {
		if (pState.needToEvaluate()) {
			ALUValue* exprResult = evaluateExpression(m_RPNExpression, &g_counters);
			if (exprResult->getBool()) {
				ret = ApplyRet__EnterLoop;
			} else {
				pState.observeWhile();
			}
		} else {
			pState.observeWhile();
		}
		break;
	}
	case PRED_DO:
		break;
	case PRED_DONE:
		if (pState.runningOutLoop()) {
			pState.observeDone();
		} else {
			ret = ApplyRet__DoneLoop;
		}
		break;
	}

	return ret;
}
