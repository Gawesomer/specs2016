#include "utils/ErrorReporting.h"
#include "processing/Config.h"
#include "specItems.h"

ALUCounters g_counters;

struct predicateStackItem {
	ConditionItem* pred;
	unsigned int   argIndex;
};

itemGroup::itemGroup()
{
	m_items.clear();
	bNeedRunoutCycle = false;
	bNeedRunoutCycleFromStart = false;
	bFoundSelectSecond = false;
}

itemGroup::~itemGroup()
{
	while (!m_items.empty()) {
		PItem pItem = m_items[0];
		m_items.erase(m_items.begin());
		delete pItem;
	}
}

void itemGroup::addItem(PItem pItem)
{
	m_items.insert(m_items.end(), pItem);
}

void itemGroup::Compile(std::vector<Token> &tokenVec, unsigned int& index)
{
	predicateStackItem predicateStack[MAX_DEPTH_CONDITION_STATEMENTS];
	unsigned int predicateStackIdx = 0;

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
		case TokenListType__UNREAD:
		case TokenListType__REDO:
		case TokenListType__NOWRITE:
		case TokenListType__ABEND:
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
		case TokenListType__TIMEDIFF:
		case TokenListType__ID:
		case TokenListType__PRINT:
		{
			DataField *pItem = new DataField;
			pItem->parse(tokenVec, index);
			if (pItem->forcesRunoutCycle()) {
				bNeedRunoutCycleFromStart = bNeedRunoutCycle = true;
			}
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
		case TokenListType__ASSERT:
		{
			MYASSERT(index < tokenVec.size());
			if (TokenListType__WHILE == tokenVec[index].Type()) {
				if (TokenListType__DO != tokenVec[index+1].Type()) {
					std::string err = "Missing DO after WHILE at index " + std::to_string(tokenVec[index].argIndex());
					if (tokenVec[index].Literal().length() > 0) {
						err += " with condition \"" + tokenVec[index].Literal() + "\"";
					}
					MYTHROW(err);
				}
			} else if (TokenListType__ASSERT != tokenVec[index].Type()) {
				if (TokenListType__THEN != tokenVec[index+1].Type()) {
					std::string err = "Missing THEN after IF at index " + std::to_string(tokenVec[index].argIndex());
					if (tokenVec[index].Literal().length() > 0) {
						err += " with condition \"" + tokenVec[index].Literal() + "\"";
					}
					MYTHROW(err);
				}
			}
			try {
				ConditionItem* pItem = new ConditionItem(tokenVec[index].Literal());
				if (TokenListType__IF == tokenVec[index].Type()) {
					MYASSERT_WITH_MSG((predicateStackIdx+1)<MAX_DEPTH_CONDITION_STATEMENTS, "Too many nested conditions");
					predicateStack[predicateStackIdx].pred = pItem;
					predicateStack[predicateStackIdx].argIndex = tokenVec[index].argIndex();
					predicateStackIdx++;
				}
				else if (TokenListType__ELSEIF == tokenVec[index].Type()) {
					pItem->setElseIf();
				}
				else if (TokenListType__WHILE == tokenVec[index].Type()) {
					pItem->setWhile();
					MYASSERT_WITH_MSG((predicateStackIdx+1)<MAX_DEPTH_CONDITION_STATEMENTS, "Too many nested conditions");
					predicateStack[predicateStackIdx].pred = pItem;
					predicateStack[predicateStackIdx].argIndex = tokenVec[index].argIndex();
					predicateStackIdx++;
				}
				else if (TokenListType__ASSERT == tokenVec[index].Type()) {
					pItem->setAssert();
				}

				if (pItem->forcesRunoutCycle()) {
					bNeedRunoutCycleFromStart = bNeedRunoutCycle = true;
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
			if (0 == predicateStackIdx) {
				std::string err = "ENDIF without IF at index " + std::to_string(tokenVec[index].argIndex());
				MYTHROW(err);
			}
			// Pop a value from the predicate stack. Should be an IF
			predicateStackIdx--;
			if (ConditionItem::PRED_IF != predicateStack[predicateStackIdx].pred->pred()) {
				std::string err = "Mismatched predicates: ENDIF at index " +
						std::to_string(tokenVec[index].argIndex()) +
						" does not match " + predicateStack[predicateStackIdx].pred->Debug() +
						" at index " + std::to_string(predicateStack[predicateStackIdx].argIndex);
				MYTHROW(err);
			}
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
			if (0 == predicateStackIdx) {
				std::string err = "DONE without WHILE at index " + std::to_string(tokenVec[index].argIndex());
				MYTHROW(err);
			}
			// Pop a value from the predicate stack. Should be an IF
			predicateStackIdx--;
			if (ConditionItem::PRED_WHILE != predicateStack[predicateStackIdx].pred->pred()) {
				std::string err = "Mismatched predicates: DONE at index " +
						std::to_string(tokenVec[index].argIndex()) +
						" does not match " + predicateStack[predicateStackIdx].pred->Debug() +
						" at index " + std::to_string(predicateStack[predicateStackIdx].argIndex);
				MYTHROW(err);
			}
			ConditionItem* pItem = new ConditionItem(ConditionItem::PRED_DONE);
			index++;
			addItem(pItem);
			break;
		}
		case TokenListType__BREAK:
		{
			BreakItem* pItem = new BreakItem(tokenVec[index].Literal()[0]);
			index++;
			addItem(pItem);
			break;
		}
		case TokenListType__SELECT:
		{
			SelectItem* pItem = new SelectItem(tokenVec[index].Literal(), false /* bIsOutput */);
			index++;
			addItem(pItem);
			if (pItem->isSelectSecond()) setRegularRunAtEOF();
			break;
		}
		case TokenListType__OUTSTREAM:
		{
			SelectItem* pItem = new SelectItem(tokenVec[index].Literal(), true /* bIsOutput */);
			index++;
			addItem(pItem);
			break;
		}
		case TokenListType__REQUIRES:
		{
			if (!configSpecLiteralExists(tokenVec[index].Literal())) {
				std::string err = "Missing required configured literal <" + tokenVec[index].Literal() + ">";
				MYTHROW(err);
			}
			index++;
			break;
		}
		default:
			std::string err = std::string("Unhandled token type ")
				+ TokenListType__2str(tokenVec[index].Type())
				+ " at argument " + std::to_string(tokenVec[index].argIndex());
			MYTHROW(err);
		}
	}

	if (predicateStackIdx > 0) {
		predicateStackIdx--;
		std::string err = "Predicate " + predicateStack[predicateStackIdx].pred->Debug() +
				" at index " + std::to_string(predicateStack[predicateStackIdx].argIndex) +
				" is not terminated";
		MYTHROW(err);
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

bool itemGroup::processDo(StringBuilder& sb, ProcessingState& pState, Reader* pRd, classifyingTimer& tmr)
{
	bool bSomethingWasDone = false;
	int i = 0;
	bool isEOFCycle = false;
	PSpecString ps; // Used for processing READ and READSTOP tokens
	bool processingContinue = true;
	bool suspendUntilBreak = false;

	if (pState.isRunOut()) {
		// Find the EOF token
		for ( ; !bNeedRunoutCycleFromStart && i < m_items.size() && !bFoundSelectSecond; i++) {
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
		if (pState.inputStreamHasChanged()) {
			multiReader* pmRead = dynamic_cast<multiReader*>(pRd);
			MYASSERT_WITH_MSG(NULL != pmRead, "Stream selected in non-multi-stream specification");
			pState.setFirst();
			PSpecString ps = pState.currRecord();
			pmRead->selectStream(pState.getActiveInputStream(), &ps);
			pState.resetInputStreamFlag();
			pState.setStringInPlace(ps);
		}
		PItem pit = m_items[i];
		if (!pit->ApplyUnconditionally() && !pState.needToEvaluate()) {
			continue;
		}
		if (suspendUntilBreak && !pit->isBreak()) {
			continue;
		}
		if (pit->isBreak()) {
			suspendUntilBreak = false;
		}
		ApplyRet aRet = pit->apply(pState, &sb);
		switch (aRet) {
		case ApplyRet__ContinueWithDataWritten:
			bSomethingWasDone = true;
		case ApplyRet__Continue:
			break;
		case ApplyRet__Write:
			if (bSomethingWasDone) {
				pState.getCurrentWriter()->Write(sb.GetString());
			} else {
				pState.getCurrentWriter()->Write(SpecString::newString());
			}
			bSomethingWasDone = false;
			break;
		case ApplyRet__ReDo:
			if (bSomethingWasDone) {
				ps = sb.GetString();
			} else {
				ps = SpecString::newString();
			}
			pState.setString(ps, false);
			pState.setFirst();
			break;
		case ApplyRet__Read:
		case ApplyRet__ReadStop:
			MYASSERT_WITH_MSG(pState.getActiveInputStation() != STATION_SECOND, "Cannot READ or READSTOP during SELECT SECOND");
			ps = pRd->get(tmr);
			if (!ps) {
				if (aRet==ApplyRet__Read) {
					ps = SpecString::newString();
				} else {
					processingContinue = false; // Stop processing if no extra record is available
				}
			} else {
				pState.incrementExtraReads();
			}
			pState.setString(ps, false);
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
		case ApplyRet__UNREAD:
			if (!pRd->hasRunDry()) {
				pRd->pushBack(pState.extractCurrentRecord());
			}
			break;
		case ApplyRet__Break:
			suspendUntilBreak = true;
			break;
		default:
			std::string err = "Unexpected return code from TokenItem::apply: ";
			err += std::to_string(aRet);
			MYTHROW(err);
		}
	}

	return bSomethingWasDone;
}

void itemGroup::process(StringBuilder& sb, ProcessingState& pState, Reader& rd, classifyingTimer& tmr)
{
	PSpecString ps;

	while ((ps=rd.get(tmr))) {
		pState.setString(ps);
		pState.setFirst();
		pState.incrementCycleCounter();

		if (processDo(sb,pState, &rd, tmr)) {
			PSpecString pOutString = sb.GetString();
			if (pState.shouldWrite()) {
				tmr.changeClass(timeClassOutputQueue);
				pState.getCurrentWriter()->Write(pOutString);
				tmr.changeClass(timeClassProcessing);
			} else {
				delete pOutString;
				pState.resetNoWrite();
			}
		}

		if (DEFAULT_READER_IDX != pState.getActiveInputStream()) {
			pState.setStream(DEFAULT_READER_IDX);
		}
		pState.setActiveWriter(1);
	}

	if (!bNeedRunoutCycle) {
		tmr.changeClass(timeClassDraining);
		return;
	}

	// run-out cycle
	pState.setString(NULL);
	pState.setFirst();
	if (processDo(sb, pState, &rd, tmr)) {
		pState.getCurrentWriter()->Write(sb.GetString());
	}

	tmr.changeClass(timeClassDraining);
}

bool itemGroup::readsLines()
{
	bool bInRedo[MAX_DEPTH_CONDITION_STATEMENTS];
	unsigned int bInRedoIdx = 0;
	bInRedo[bInRedoIdx] = false;
	for (PItem pItem : m_items) {
		// Check if we need to go up or down a level
		ConditionItem* pCond = dynamic_cast<ConditionItem*>(pItem);
		if (pCond) {
			switch (pCond->pred()) {
			case ConditionItem::PRED_THEN:
			case ConditionItem::PRED_DO:
				MYASSERT_WITH_MSG((bInRedoIdx+1)<MAX_DEPTH_CONDITION_STATEMENTS, "Too many nested conditions");
				bInRedoIdx++;
				bInRedo[bInRedoIdx] = bInRedo[bInRedoIdx-1];
				break;
			case ConditionItem::PRED_DONE:
			case ConditionItem::PRED_ENDIF:
				MYASSERT_WITH_MSG(bInRedoIdx>0, "Too many ends of conditions");
				bInRedoIdx--;
				break;
			default:
				break;
			}
		}

		// Check if we are starting a REDO so all ranges can be ignored.
		TokenItem* pToken = dynamic_cast<TokenItem*>(pItem);
		if (pToken && TokenListType__REDO==pToken->getToken()->Type()) {
			bInRedo[bInRedoIdx] = true;
		}

		if (!bInRedo[bInRedoIdx] && pItem->readsLines()) return true;
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
	case TokenListType__NOWRITE:
		pState.setNoWrite();
		return ApplyRet__Continue;
	case TokenListType__ABEND: {
		std::string err = "ABEND: " + mp_Token->Literal();
		MYABEND(err);
	}
		return ApplyRet__Continue;
	case TokenListType__READ:
		return ApplyRet__Read;
	case TokenListType__READSTOP:
		return ApplyRet__ReadStop;
	case TokenListType__WRITE:
		return ApplyRet__Write;
	case TokenListType__EOF:
		return ApplyRet__EOF;
	case TokenListType__UNREAD:
		return ApplyRet__UNREAD;
	case TokenListType__REDO:
		return ApplyRet__ReDo;
	default:
		std::string err = "Unhandled TokenItem type " + TokenListType__2str(mp_Token->Type());
		MYTHROW(err);
	}
}

bool TokenItem::readsLines() {
	switch (mp_Token->Type()) {
	case TokenListType__READ:
	case TokenListType__READSTOP:
	case TokenListType__EOF:
		return true;
	default:
		return false;
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

bool SetItem::readsLines()
{
	return AluExpressionReadsLines(m_RPNExpression);
}


ConditionItem::ConditionItem(std::string& _statement)
{
	m_pred = PRED_IF;
	m_rawExpression = _statement;
	AluVec expr;
	MYASSERT(parseAluExpression(_statement, expr));
	if (expressionIsAssignment(expr)) {
		AluUnit* aUnit = expr[0];
		AluUnitCounter* pCounterUnit = dynamic_cast<AluUnitCounter*>(aUnit);
		MYASSERT(NULL != pCounterUnit);
		m_counter = pCounterUnit->getKey();
		delete aUnit;
		m_assnOp = dynamic_cast<AluAssnOperator*>(expr[1]);
		MYASSERT(NULL != m_assnOp);
		expr.erase(expr.begin(), expr.begin()+2);
		m_isAssignment = true;
	} else {
		m_isAssignment = false;
	}
	MYASSERT(convertAluVecToPostfix(expr, m_RPNExpression, true));
}

ConditionItem::ConditionItem(ConditionItem::predicate _p) : m_counter(0), m_assnOp(NULL)
{
	MYASSERT(_p != PRED_IF);
	m_isAssignment = false;
	m_pred = _p;
}

ConditionItem::~ConditionItem()
{
	if ((m_pred == PRED_IF) || (m_pred == PRED_ELSEIF) || (m_pred == PRED_WHILE) || (m_pred == PRED_ASSERT)) {
		for (AluUnit* unit : m_RPNExpression) {
			delete unit;
		}
		if (m_isAssignment) {
			delete m_assnOp;
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
		std::string ret = "IF (" + m_rawExpression + ")";
		return ret;
	}
	case PRED_ELSEIF: {
		std::string ret = "ELSEIF (" + m_rawExpression + ")";
		return ret;
	}
	case PRED_WHILE: {
		std::string ret = "WHILE (" + m_rawExpression + ")";
		return ret;
	}
	case PRED_ASSERT: {
		std::string ret = "ASSERT " + m_rawExpression;
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

void ConditionItem::setAssert()
{
	MYASSERT(PRED_IF == m_pred);
	m_pred = PRED_ASSERT;
}

bool ConditionItem::evaluate()
{
	bool ret;
	if (m_isAssignment) {
		ALUPerformAssignment(m_counter, m_assnOp, m_RPNExpression, &g_counters);
		ret = g_counters.getPointer(m_counter)->getBool();
	} else {
		ALUValue* exprResult = evaluateExpression(m_RPNExpression, &g_counters);
		ret = exprResult->getBool();
		delete exprResult;
	}

	return ret;
}

ApplyRet ConditionItem::apply(ProcessingState& pState, StringBuilder* pSB)
{
	ApplyRet ret = ApplyRet__Continue;

	switch (m_pred) {
	case PRED_IF: {
		if (pState.needToEvaluate()) {
			pState.setCondition(evaluate());
		} else {
			pState.observeIf();
		}
		break;
	}
	case PRED_ASSERT: {
		if (!evaluate()) {
			std::string err = "ASSERTION failed: " + m_rawExpression;
			MYABEND(err);
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
			pState.setCondition(evaluate());
		}
		break;
	}
	case PRED_ENDIF:
		pState.observeEndIf();
		break;
	case PRED_WHILE: {
		if (pState.needToEvaluate()) {
			if (evaluate()) {
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

bool ConditionItem::readsLines()
{
	return AluExpressionReadsLines(m_RPNExpression);
}

BreakItem::BreakItem(char identifier)
{
	m_identifier = identifier;
}

std::string BreakItem::Debug()
{
	std::string ret("BREAK:");
	ret += m_identifier;
	return ret;
}

ApplyRet BreakItem::apply(ProcessingState& pState, StringBuilder* pSB)
{
	if (pState.breakEstablished(m_identifier)) {
		return ApplyRet__Continue;
	} else {
		return ApplyRet__Break;
	}
}

SelectItem::SelectItem(std::string& st, bool bIsOutput)
{
	bOutput = bIsOutput;
	if (st=="FIRST") {
		MYASSERT(!bIsOutput);
		m_stream = STATION_FIRST;
	} else if (st=="SECOND") {
		MYASSERT(!bIsOutput);
		m_stream = STATION_SECOND;
	} else if (st=="err") {
		MYASSERT(bIsOutput);
		m_stream = STATION_STDERR;
	} else if (st.length()==1) {
		m_stream = st[0] - '0';
		MYASSERT(m_stream >= DEFAULT_READER_IDX);
		MYASSERT(m_stream <= MAX_INPUT_STREAMS);
	} else {
		std::string err = "Invalid ";
		if (bIsOutput)
			err += "output";
		else err += "input";
		err += " stream " + st;
		MYTHROW(err);
	}
}

std::string SelectItem::Debug()
{
	if (m_stream == STATION_SECOND) {
		MYASSERT(!bOutput);
		return "SELECT:SECOND";
	} else if (m_stream == STATION_STDERR) {
		MYASSERT(bOutput);
		return "OUTSTREAM:STDERR";
	}
	std::string ret;
	ret = bOutput ? "OUTSTREAM:" : "SELECT:";
	ret += std::to_string(m_stream);
	return ret;
}

ApplyRet SelectItem::apply(ProcessingState& pState, StringBuilder* pSB)
{
	if (m_stream == STATION_FIRST) {
		pState.setFirst();
	} else if (m_stream == STATION_SECOND) {
		pState.setSecond();
	} else if (m_stream>=0 && m_stream<=MAX_INPUT_STREAMS) {
		if (bOutput) {
			pState.setActiveWriter(m_stream);
		} else {
			MYASSERT(m_stream >= DEFAULT_READER_IDX);
			pState.setStream(m_stream);
		}
	} else {
		MYTHROW("Invalid SelectItem");
	}
	return ApplyRet__Continue;
}
