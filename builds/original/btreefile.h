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
		BTreeFile(string path, int min_children);
		~BTreeFile();
		void setPath(string path);
		set<string> get(string key);
		void commit(string newPath, LogFile &log, bool reverseLog);
		void moveState(string dir_path_init, string dir_path_final);
		void copyState(string newPath);
	private:
		void createTableLine(string newPath, string mask, int &line_cursor);
		string getLineInTable(int line);
		string getEntryInTable(int line, int line_index);
		unsigned long getEntryLeftNumber(string entry);
		unsigned long getEntryRightNumber(string entry);

		void write_header(string newPath, int n, bool truncate);
		void write_line_at_index(string newPath, int index, string line);

		string filename;
		fstream file;
		string path;

		const static int MASK_SIZE = 2, NUM_WIDTH = 10, ENTRY_WIDTH = 2 * NUM_WIDTH + 2;

		int min_children_per_node;
		int LINE_WIDTH, _table_region_ptr, table_size, last_table_line_written;
};


BTreeFile::BTreeFile(string path, int minChildrenPerNode = 32) : HashFile()
{
	min_children_per_node = minChildrenPerNode;
	setPath(path);
}

BTreeFile::~BTreeFile()
{
	file.close();
}

void BTreeFile::setPath(string Path)
{
	path = Path;
	HashFile::setPath(path);
	filename = path + "BTreeFile.txt";

	LINE_WIDTH = ENTRY_WIDTH * pow(16.0, (int)MASK_SIZE) + 1;

	file.open(filename.c_str(), fstream::in );

	// read first line, which encodes table size
	string line;	
	getline(file, line);	
	table_size = atoi(line.c_str());

	_table_region_ptr = file.tellg();
}

void BTreeFile::write_header(string newPath, int n, bool truncate)
{
	string tableSize = convertIntToString(n);
	tableSize.append(LINE_WIDTH - tableSize.length() - 1, ' ');
	tableSize += "\n";

	fstream newFile;
	string newFilename = newPath + "BTreeFile.txt";

	if(truncate)
		newFile.open(newFilename.c_str(), fstream::out | fstream::trunc);
	else
		newFile.open(newFilename.c_str(), fstream::in | fstream::out);

	newFile.write(tableSize.c_str(), tableSize.size());
	_table_region_ptr = newFile.tellp();

	newFile.flush();
	newFile.close();
}

// write to the specified index in the table; however, if this index
// is out of bounds with regards to the current file size, add the appropriate
// empty spacing

void BTreeFile::write_line_at_index(string newPath, int index, string line)
{
	assert(line.size() == LINE_WIDTH);

	fstream newFile;
	string newFilename = newPath + "BTreeFile.txt";

	if(last_table_line_written < index - 1)
	{
		newFile.open(newFilename.c_str(), fstream::out | fstream::app);
		newFile.width(LINE_WIDTH);
		newFile.fill(' ');

		while(last_table_line_written < index - 1)
		{
			newFile << ' ';
			last_table_line_written++;
		}		
		newFile.flush();
		newFile.close();
	}	
	else if(last_table_line_written == index - 1)
		last_table_line_written++;

	newFile.open(newFilename.c_str(), fstream::in | fstream::out);
	newFile.seekp(LINE_WIDTH * index + _table_region_ptr);
	newFile.write(line.c_str(), line.size());
	newFile.flush();
	newFile.close();
}


string BTreeFile::getLineInTable(int line_index)
{
	string line;
	file.seekg(_table_region_ptr + line_index * LINE_WIDTH);
	getline(file, line);
	return line;
}

string BTreeFile::getEntryInTable(int line_num, int line_index)
{
	string line = getLineInTable(line_num);
	string entry = line.substr(line_index * ENTRY_WIDTH, ENTRY_WIDTH);
	return entry;
}

unsigned long BTreeFile::getEntryRightNumber(string entry)
{
	unsigned long n;
	string num = entry.substr(2 + NUM_WIDTH, NUM_WIDTH);
	stringstream(num) >> n;
	return(n);
}

unsigned long BTreeFile::getEntryLeftNumber(string entry)
{
	unsigned long n;
	string num = entry.substr(2, NUM_WIDTH);
	stringstream(num) >> n;
	return(n);
}

// iteratively follow the pointers in the table until we get to a line marked
// 'XXXXX...' (no key) or 'L [address1][address2]' (line #'s surrounding key in HashFile)

set<string> BTreeFile::get(string key)
{
	set<string> list;
	int mask_index = 0, table_line = 0, table_index, begin_index, end_index;
	string masked_key, table_entry;

	if(table_size == 0)
		return list;
	
	do
	{
		masked_key = key.substr(mask_index, MASK_SIZE);
		table_index = convertHexToInt(masked_key);
		table_entry = getEntryInTable(table_line, table_index);
		table_line = getEntryRightNumber(table_entry);
		mask_index += MASK_SIZE;	
	} while(table_entry[0] == 'T');

	if(table_entry[0] == 'X')
		return list;
	// table_line now contains index into HashFile of where to begin/end search

	unsigned long low = getEntryLeftNumber(table_entry);
	unsigned long high = getEntryRightNumber(table_entry);

	// call HashFile:: classes to extract set of values 
	return HashFile::get(key, low, high);
}

void BTreeFile::createTableLine(string newPath, string mask, int &line_cursor)
{	
	// line_cursor lets us know the next free line available in the table
	int curr_line = line_cursor;
	line_cursor++;
	string line;

	// iterate through all integers covering the mask
	// for each mask:
	//     1.  if the key does not exist in HashTable, mark entry as 'XX..'
	//     2.  if the key spans less than min_children_per_node entries, mark entry as 'LL [index_begin][index_end]'
	//     3.  else mark entry as 'TT [table-line]' and recursively generate sub-table
	for(int i=0; i<pow(16.0, (int)MASK_SIZE); i++)
	{
		string new_mask = mask + convertIntToHex(i, MASK_SIZE);

		//new_mask_begin = new_mask + 0000000000...
		//new_mask_end   = new_mask + ffffffffff...
		string new_mask_begin = new_mask;
		string new_mask_end = new_mask;

		new_mask_begin.append(SHA_WIDTH - new_mask.size(), '0');
		new_mask_end.append(SHA_WIDTH - new_mask.size(), 'f');		

		unsigned long index_begin = HashFile::getIndexOfKey(new_mask_begin);

		// check if this mask is out of our data-range
		if(index_begin >= HashFile::length())
		{
			line.append(ENTRY_WIDTH, 'X');
			continue;
		}

		string key = HashFile::getKeyAtIndex(index_begin);

		// check if this mask is even present in HashFile
		if(key.substr(0, new_mask.size()) != new_mask)
		{			
			line.append(ENTRY_WIDTH, 'X'); // 16-char entry
			continue;
		}

		int index_end = HashFile::getIndexOfKey(new_mask_end);

		// only create a sub-table through recursion, if the range covered by the 
		// new mask exceeds min_children_per_node. Otherwise, record the line# in the table
		// note that 'children' corresponds to unique key/val pairs; may infact cover the same key more than once
		if(index_end - index_begin < min_children_per_node)
			line += "L " + convertIntToString(index_begin, (ENTRY_WIDTH-2)/2) + convertIntToString(index_end, (ENTRY_WIDTH-2)/2);
		else
		{
			int line_cursor_stored = line_cursor;
			createTableLine(newPath, new_mask, line_cursor);

			// create a pointer to the recursively created sub-table
			line += "T " + convertIntToString(line_cursor_stored, ENTRY_WIDTH - 2);
		}
	}

	line += "\n";

	write_line_at_index(newPath, curr_line, line);
}

void BTreeFile::commit(string newPath, LogFile &log, bool reverseLog = false)
{
	//since this data-structure is dependent on a coherent HashFile, we commit it first
	HashFile::commit(newPath, log, reverseLog);
	HashFile::setPath(newPath);

	int line_cursor = 0;
	last_table_line_written = -1;

	//save space for the table-size, which goes on the first-line
	write_header(newPath, 0, /*truncate existing file*/ true);

	//create the table recursively
	createTableLine(newPath, string(""), line_cursor);

	//and finally write the table-size to the first-line
	write_header(newPath, line_cursor, false);

}


void BTreeFile::copyState(string dir_path)
{
	HashFile::copyState(dir_path);

	file.close();
	file_copy(filename.c_str(),(dir_path+"BTreeFile.txt").c_str());
	file.open(filename.c_str(), fstream::in);
}

void BTreeFile::moveState(string dir_path_init, string dir_path_final)
{
	HashFile::moveState(dir_path_init, dir_path_final);

	file.close();
	rename((dir_path_init + "BTreeFile.txt").c_str(), (dir_path_final + "BTreeFile.txt").c_str());
	setPath(dir_path_final);
}
