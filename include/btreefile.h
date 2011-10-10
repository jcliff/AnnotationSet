#include <set>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <assert.h>

#include "hashfile.h"
#include <math.h>

using namespace std;

#define SHA_WIDTH 40

class BTreeFile : public HashFile
{
	public:
		BTreeFile(string path, unsigned long minChildrenPerNode = 128);
		~BTreeFile();
		void setPath(string path);
		set<string> get(string key);
		void commit(string newPath, LogFile &log, bool reverseLog);
		void moveState(string dirPathInit, string dirPathFinal);
		void copyState(string newPath);
	private:
		void createTableLine(string newPath, string mask, unsigned long &line_cursor);
		string getLineInTable(unsigned long line);
		void getEntryInTable(char *buf, unsigned long line, int line_index);
		unsigned long getEntryLeftNumber(string entry);
		unsigned long getEntryRightNumber(string entry);

		void write_header(string newPath, unsigned long n, bool truncate);
		void write_line_at_index(string newPath, unsigned long index, char *line);

		string filename;
		fstream file;
		string path;

		const static int MASK_SIZE = 2, NUM_WIDTH = 4, FLAG_WIDTH = 1, ENTRY_WIDTH = 2 * NUM_WIDTH + 1;
		const static char LINE_IDX_FLAG = 0x01, TABLE_PTR_FLAG = 0x02, EMPTY_FLAG = 0x00;

		int LINE_WIDTH, _table_region_ptr;
		unsigned long table_size, last_table_line_written, min_children_per_node;
};


