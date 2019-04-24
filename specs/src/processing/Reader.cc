#include <string.h>
#include <fstream>
#include "utils/ErrorReporting.h"
#include "Reader.h"

void ReadAllRecordsIntoReaderQueue(Reader* r)
{
	while (!r->endOfSource()) {
		r->readIntoQueue();
	}
}

Reader::~Reader()
{
	PSpecString ps;
	End();
	while (!m_queue.empty()) {
		m_queue.wait_and_pop(ps);
		delete ps;
	}
}

void Reader::selectStream(unsigned char idx)
{
	if (idx != DEFAULT_READER_IDX) {
		std::string err = "Attempted to select stream " + std::to_string(idx) + " when no secondary streams are defined.";
		MYTHROW(err);
	}
}

void Reader::End()
{
	if (mp_thread) {
		mp_thread->join();
	}
	delete mp_thread;
	mp_thread = NULL;
}

PSpecString Reader::get()
{
	PSpecString ret;
	if (m_pUnreadString) {
		ret = m_pUnreadString;
		m_pUnreadString = NULL;
		return ret;
	}
	if (eof()) {
		m_bRanDry = true;
		return NULL;
	}
	if (m_queue.wait_and_pop(ret)) {
		m_countUsed++;
		return ret;
	} else {
		m_bRanDry = true;
		return NULL;
	}
}

void Reader::pushBack(PSpecString ps)
{
	MYASSERT_WITH_MSG(m_pUnreadString==NULL, "Only one record can be UNREAD at a time");
	m_pUnreadString = ps;
}

void Reader::readIntoQueue()
{
	if (!endOfSource()) {
		PSpecString nextRecord = getNextRecord();
		if (nextRecord) {
			m_queue.push(nextRecord);
			m_countRead++;
		} else {
			m_queue.Done();
		}
	}
}

void Reader::Begin() {
	mp_thread = new std::thread(ReadAllRecordsIntoReaderQueue, this);
}


StandardReader::StandardReader() {
	m_File = &std::cin;
	m_NeedToClose = false;
	m_EOF = false;
	m_buffer = (char*)malloc(STANDARD_READER_BUFFER_SIZE);
}

StandardReader::StandardReader(std::istream* f) {
	MYASSERT(f!=NULL);
	m_EOF = false;
	if (!f->good()) {  // so it crashes if what we've been passed is not a stream pointer
		m_EOF = true;
	}
	m_File = f;
	m_NeedToClose = false;
	m_buffer = (char*)malloc(STANDARD_READER_BUFFER_SIZE);
}

StandardReader::StandardReader(std::string& fn) {
	std::ifstream* pInputFile = new std::ifstream(fn);
	m_File = pInputFile;
	if (!pInputFile->is_open()) {
		std::string err = "File not found: " + fn;
		MYTHROW(err);
	}
	m_NeedToClose = true;
	m_EOF = false;
	m_buffer = (char*)malloc(STANDARD_READER_BUFFER_SIZE);
}

StandardReader::~StandardReader() {
	if (m_NeedToClose) {
		std::ifstream* pInputFile = dynamic_cast<std::ifstream*>(m_File);
		pInputFile->close();
		delete pInputFile;
	}
	free(m_buffer);
}

bool StandardReader::endOfSource() {
	return m_EOF;
}

PSpecString StandardReader::getNextRecord() {
	std::string line;
	if (!std::getline(*m_File, line)) {
		m_EOF = true;
		return NULL;
	} else {
		// strip trailing newline if any
		if (line.back() == '\n') {
			line.pop_back();
		}
		return SpecString::newString(line);
	}
}

TestReader::TestReader(size_t maxLineCount)
{
	mp_arr = (SpecString**)malloc(sizeof(PSpecString) * maxLineCount);
	m_count = m_idx = 0;
	m_MaxCount = maxLineCount;
}

TestReader::~TestReader()
{
	if (mp_arr) {
		size_t i;
		for (i=0; i<m_count; i++) {
			delete mp_arr[i];
		}
		free(mp_arr);
	}
}

void TestReader::InsertString(const char* s)
{
	if (m_count >= m_MaxCount) {
		MYTHROW("Attempting to insert too many lines into TestReader");
	}
	mp_arr[m_count++] = SpecString::newString(s);
}

void TestReader::InsertString(PSpecString ps)
{
	if (m_count >= m_MaxCount) {
		MYTHROW("Attempting to insert too many lines into TestReader");
	}
	mp_arr[m_count++] = ps;
}

// #include <cstring>  // for memset
// #include "utils/ErrorReporting.h"

#define ITERATE_VALID_STREAMS(i)                \
	unsigned char i;                            \
	for (i=0 ; i < maxReaderIdx+1 ; i++) {   \
		if (NULL != readerArray[i]) {

#define ITERATE_VALID_STREAMS_END				\
		}                                       \
    }


multiReader::multiReader(Reader* pDefaultReader)
{
	memset(readerArray, 0, sizeof(readerArray));
	memset(stringArray, 0, sizeof(stringArray));
	readerIdx = DEFAULT_READER_IDX - 1;
	maxReaderIdx = readerIdx;
	readerArray[readerIdx] = pDefaultReader;
	bFirstGet = true;
}


multiReader::~multiReader()
{
	ITERATE_VALID_STREAMS(idx)
		delete readerArray[idx];
		readerArray[idx] = NULL;
		if (stringArray[idx]) {
			delete stringArray[idx];
		}
	ITERATE_VALID_STREAMS_END
}

void multiReader::addStream(unsigned char idx, std::istream* f)
{
	MYASSERT_WITH_MSG(idx>0 && idx <= MAX_INPUT_STREAMS, "Invalid input stream number");
	idx--;  // Set to C-style index
	MYASSERT_WITH_MSG(NULL==readerArray[idx], "Input stream is already defined");

	readerArray[idx] = new StandardReader(f);
	if (idx > maxReaderIdx) maxReaderIdx = idx;
}

void multiReader::addStream(unsigned char idx, std::string& fn)
{
	MYASSERT_WITH_MSG(idx>0 && idx <= MAX_INPUT_STREAMS, "Invalid input stream number");
	idx--;  // Set to C-style index
	MYASSERT_WITH_MSG(NULL==readerArray[idx], "Input stream is already defined");

	readerArray[idx] = new StandardReader(fn);
	if (idx > maxReaderIdx) maxReaderIdx = idx;
}

void multiReader::selectStream(unsigned char idx, PSpecString* ppRecord)
{
	MYASSERT_WITH_MSG(idx>0 && idx <= MAX_INPUT_STREAMS, "Invalid input stream number");
	idx--;  // Set to C-style index
	MYASSERT_WITH_MSG(NULL!=readerArray[idx], "Invalid input stream");

	if (readerIdx!=idx) {
		MYASSERT(NULL == stringArray[readerIdx]);
		stringArray[readerIdx] = *ppRecord;
		*ppRecord = stringArray[idx];
		stringArray[idx] = NULL;
		readerIdx = idx;
	}
}

void multiReader::Begin()
{
	ITERATE_VALID_STREAMS(idx)
		readerArray[idx]->Begin();
	ITERATE_VALID_STREAMS_END
}

PSpecString multiReader::get()
{
	PSpecString ret = readerArray[readerIdx]->get();
	if (!ret) return NULL;

	ITERATE_VALID_STREAMS(idx)
		if (stringArray[idx]) {
			MYASSERT(idx!=readerIdx);
			delete stringArray[idx];
			stringArray[idx] = readerArray[idx]->get();
			if (!stringArray[idx]) {
				std::string err = "Input stream " + std::to_string(idx+1) + " ran dry while active stream ("
						+ std::to_string(readerIdx+1) + ") still has records";
				MYTHROW(err);
			}
		} else {
			MYASSERT(idx==readerIdx || bFirstGet);
			/* ret has already been read, and the stringArray slot remains NULL */
			if (bFirstGet && idx!=readerIdx) {
				stringArray[idx] = readerArray[idx]->get();
				if (!stringArray[idx]) {
					std::string err = "Input stream " + std::to_string(idx+1) + " ran dry at the first record"
							" while active stream (" + std::to_string(readerIdx+1) + ") still has records";
					MYTHROW(err);
				}
			}
		}
	ITERATE_VALID_STREAMS_END

	bFirstGet = false;
	return ret;
}

bool multiReader::endOfSource()
{
	MYTHROW("multiReader::endOfSource() should not have been called.");
	return false;
}

PSpecString multiReader::getNextRecord()
{
	MYTHROW("multiReader::getNextRecord() should not have been called.");
	return NULL;
}
