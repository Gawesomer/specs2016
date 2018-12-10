#include <iostream>
#include <string>
#include <iomanip>
#include "cli/tokens.h"
#include "specitems/specItems.h"
#include "processing/Config.h"
#include "processing/ProcessingState.h"
#include "processing/StringBuilder.h"

#define VERIFY(sp,ex) {          \
		PSpecString ps = runTestOnExample(sp, "The quick brown fox jumped over the   lazy dog");  \
		testCount++;                            \
		std::cout << "Test #" << std::setfill('0') << std::setw(3) << testCount << " ";     \
		if (!ps) {                              \
			std::cout << "*** NOT OK ***: Got (NULL); Expected: <" << ex << ">\n"; \
			errorCount++;                       \
		} else {                                \
			if (ps->Compare(ex)) {              \
				std::cout << "*** NOT OK ***:\n\tGot <" << ps->data() << ">\n\tExp <" << ex << ">\n"; \
				errorCount++;                   \
			} else {                            \
				std::cout << "***** OK *****: <" << ex << ">\n"; \
			}                                   \
		}                                       \
}

#define VERIFY2(sp,ln,ex) {          \
		PSpecString ps = runTestOnExample(sp, ln);  \
		testCount++;                            \
		std::cout << "Test #" << std::setfill('0') << std::setw(3) << testCount << " ";     \
		if (!ps) {                              \
			std::cout << "*** NOT OK ***: Got (NULL); Expected: <" << ex << ">\n"; \
			errorCount++;                       \
		} else {                                \
			if (ps->Compare(ex)) {              \
				std::cout << "*** NOT OK ***:\n\tGot <" << ps->data() << ">\n\tExp <" << ex << ">\n"; \
				errorCount++;                   \
			} else {                            \
				std::cout << "***** OK *****: <" << ex << ">\n"; \
			}                                   \
		}                                       \
}

PSpecString runTestOnExample(const char* _specList, const char* _example)
{
	ProcessingState ps;

	TestReader tRead(5);
	char* example = strdup(_example);
	char* ln = strtok(example, "\n");
	while (ln) {
		tRead.InsertString(ln);
		ln = strtok(NULL, "\n");
	}

	PSpecString pFirstLine = tRead.getNextRecord();
	ps.setString(pFirstLine);
	char* specList = (char*)_specList;

	std::vector<Token> vec = parseTokens(1, &specList);
	normalizeTokenList(&vec);
	itemGroup ig;

	unsigned int index = 0;
	ig.Compile(vec,index);

	StringBuilder sb;

	PSpecString result = NULL;
	if (ig.processDo(sb, ps, &tRead, NULL)) {
		result = sb.GetString();
	}

	free(example);

	return result;
}

int main(int argc, char** argv)
{
	ProcessingState ps;
	int errorCount = 0;
	int testCount  = 0;

	readConfigurationFile();

	// Connect the ALU to the processing state
	ProcessingStateFieldIdentifierGetter fiGetter(&ps);
	setFieldIdentifierGetter(&fiGetter);

	VERIFY("w1 1", "The"); // Test #1
	VERIFY("7-17 1", "ick brown f"); // Test #2
	VERIFY("2;-2 1", "he quick brown fox jumped over the   lazy do"); // Test #3
	VERIFY("20-* 1", " jumped over the   lazy dog"); // Test #4
	VERIFY("w3-5 1", "brown fox jumped"); // Test #5
	VERIFY("word 3-5 1", "brown fox jumped"); // Test #6
	VERIFY("word 8-9 1", "lazy dog"); // Test #7
	VERIFY("word 8-* 1", "lazy dog"); // Test #8
	VERIFY("w1 1 w3 nw w6 n", "The brownover"); // Test #9
	VERIFY("w1 2 w3 4 w7-8 12", " Thbrown   the   lazy"); // Test #10
	VERIFY("w1 1 w3 nf", "The\tbrown"); // Test #11
	VERIFY("substring 2-4 of word 3 1", "row"); // Test #12
	VERIFY("substring word 3;-2 of 1-* 1", "brown fox jumped over the   lazy"); // Test #13
	VERIFY("substring wordsep e w3 of 1-* 1", "d ov"); // Test #14
	VERIFY("substring fieldsep e w3 of 1-* 1", "brown"); // Test #15
	VERIFY("substring fieldsep / / field 4 of w7-* 1", "lazy"); // Test #16
	VERIFY("w1 1.5 w2 n.5 w3 n.4 w4 n","The  quickbrowfox"); // Test #17
	VERIFY("w1 1.8 right w2 n.8 center w3 n left", "     The quick  brown"); // Test #18
	VERIFY("33.6 strip 1.6 right", "   the"); // Test #19
	VERIFY("w1 C2X 1 w7 C2B nw /30313233/ X2CH nw","546865 011101000110100001100101 0123"); // Test #20
	VERIFY2("fs = field 1 1 field 2 6 field 3 11 field 4 16", "a=b", "a    b         "); // Test #21
	VERIFY2("fs = field 1 1 field 2 6 field 3 11 field 4 16", "=a", "     a         ");  // Test #22
	VERIFY2("fs = field 1 1 field 2 6 field 3 11 field 4 16", "==a=b", "          a    b"); // Test #23
	VERIFY("word -2 1", "lazy"); // Test #24
	VERIFY("word 2;-2 1", "quick brown fox jumped over the   lazy"); // Test #25
	VERIFY("word 2.3 1", "quick brown fox"); // Test #26
	VERIFY2("substring fieldsep . field 1 of substr word 1 of fs = f 1   1", "  a = 17", "a"); // Test #27
	VERIFY2("substring fieldsep . field 1 of substr word 1 of fs = f 1   1", "x.18= 23", "x"); // Test #28
	VERIFY("number 1 w2 nw number strip nw", "         1 quick 1"); // Test #29
	VERIFY2("wordseparator e w2 1 w4 nw", "Hope is the thing with feathers", " is th ath"); // Test #30
	VERIFY2("w1 1 read w1 10", "Once there were green fields\nKissed by the sun","Once     Kissed"); // Test #31
	VERIFY("pad /q/ w1 1.10 left pad /w/ w2 11.10 center pad /e/ w3 21.10 right","Theqqqqqqqwwquickwwweeeeebrown"); // Test #32
	VERIFY("2:3 1 -7:-5 nw -2:* nw", "he azy og"); // Test #33
	VERIFY("k: w2 . ID k 1", "quick"); // Test #34
	VERIFY2("1-* tf2i %FT%H:%M:%S.%6f a: ID a ti2f /%A, %B %drd, %Y; %M minutes past the %Hth hour/ 1", "2018-11-23T14:43:43.126573","Friday, November 23rd, 2018; 43 minutes past the 14th hour"); // Test #35

	if (errorCount) {
		std::cout << '\n' << errorCount << '/' << testCount << " tests failed.\n";
	} else {
		std::cout << "\nAll tests passed.\n";
	}

	return (errorCount==0) ? 0 : 4;
}
