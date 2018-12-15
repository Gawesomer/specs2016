#include <iomanip>
#include <cmath>
#include <ctime>
#include "cli/tokens.h"
#include "processing/Config.h"
#include "specitems/specItems.h"
#include "processing/StringBuilder.h"
#include "processing/Reader.h"
#include "processing/Writer.h"
#include "utils/TimeUtils.h"
#include "utils/ErrorReporting.h"

std::string getNextArg(int& argc, char**& argv)
{
	argc--; argv++;
	MYASSERT(argc>0);
	return std::string(argv[0]);
}

#define NEXTARG getNextArg(argc, argv)
#define X(nm,typ,defval,ssw,cliswitch,oval) \
	if (0==strcmp(argv[0], "--"#cliswitch) ||       \
			0==strcmp(argv[0], "-"#ssw)) {  		\
		g_##nm = oval;								\
		goto CONTINUE;								\
	}

bool parseSwitches(int& argc, char**& argv)
{
	/* Skip the program name */
	argc--; argv++;

	while (argc>0) {
		if (argv[0][0]!='-') break;

		CONFIG_PARAMS

CONTINUE:
		argc--; argv++;
	}

	return true;
}

int main (int argc, char** argv)
{
	readConfigurationFile();

	if (!parseSwitches(argc, argv)) { // also skips the program name
		return -4;
	}

	std::vector<Token> vec;
	if (g_specFile != "") {
		vec = parseTokensFile(g_specFile);
	} else {
		vec = parseTokens(argc, argv);
	}

	normalizeTokenList(&vec);
	itemGroup ig;
	StringBuilder sb;
	ProcessingState ps;
	StandardReader *pRd;
	SimpleWriter *pWr;

	unsigned int index = 0;
	try {
		ig.Compile(vec, index);
	}  catch (const SpecsException& e) {
		std::cerr << "Error while parsing command-line arguments: " << e.what(true) << "\n";
		if (g_bVerbose) {
			std::cerr << "\nProcessing stopped at index " << index
					<< '/' << vec.size() << ":\n";
			for (int i=0; i<vec.size(); i++) {
				std::cerr << i+1 << ". " << vec[i].Debug() << "\n";
			}
			std::cerr << "\n" << ig.Debug();
		}
		exit (0);
	}

#ifdef DEBUG
	std::cerr << "After parsing, index = " << index << "/" << vec.size() << "\n";

	std::cerr << ig.Debug();
#endif

	// Connect the ALU to the processing state
	ProcessingStateFieldIdentifierGetter fiGetter(&ps);
	setFieldIdentifierGetter(&fiGetter);

	unsigned long readLines;
	unsigned long usedLines;
	unsigned long generatedLines;
	unsigned long writtenLines;
	uint64_t timeAtStart = specTimeGetTOD();
	std::clock_t clockAtStart = clock();

	if (ig.readsLines() || g_bForceFileRead) {
		pRd = new StandardReader();
		pWr = new SimpleWriter;

		pRd->Begin();
		pWr->Begin();

		try {
			ig.process(sb, ps, *pRd, *pWr);
		} catch (const SpecsException& e) {
			std::cerr << "Runtime error after reading " << pRd->countRead() << " lines and using " << pRd->countUsed() <<".\n";
			std::cerr << e.what() << "\n";
		}

		pRd->End();
		readLines = pRd->countRead();
		usedLines = pRd->countUsed();
		delete pRd;
		pWr->End();
		generatedLines = pWr->countGenerated();
		writtenLines = pWr->countWritten();
		delete pWr;
		vec.clear();
	} else {
		TestReader tRead(5);
		ig.processDo(sb, ps, &tRead, NULL);
		std::cout << *sb.GetString() << "\n";
		readLines = 0;
		usedLines = 0;
		generatedLines = 1;
		writtenLines = 1;
	}

	uint64_t timeAtEnd = specTimeGetTOD();
	std::clock_t clockAtEnd = clock();

	if (g_bPrintStats) {
		std::cerr << "\n";
		std::cerr << "Read  " << readLines << " lines.";
		if (readLines!=usedLines) {
			std::cerr << " " << usedLines << "were used.";
		}
		std::cerr << "\nWrote " << generatedLines << " lines.";
		if (readLines!=usedLines) {
			std::cerr << " " << writtenLines << "were written out.";
		}
		uint64_t runTimeMicroSeconds = timeAtEnd - timeAtStart;
		std::cerr << "\nRun Time: " << runTimeMicroSeconds / 1000000 << "." <<
				std::setfill('0') << std::setw(6) << runTimeMicroSeconds % 1000000
				<< " seconds.\n";

		ALUFloat duration = (ALUFloat(1) * (clockAtEnd-clockAtStart)) / CLOCKS_PER_SEC;
		std::cerr << "CPU Time: " << std::floor(duration) << "." <<
				std::setfill('0') << std::setw(6) <<
				u_int64_t((duration-std::floor(duration)+0.5) * 1000000) << " seconds.\n";
	}

	return 0;
}