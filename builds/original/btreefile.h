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
		BTreeFile();
		BTreeFile(int min_children); 
		BTreeFile(string path);
		BTreeFile(string path, int min_children);
		void setPath(string path);
		set<string> get(string key);
		void commit();

	private:
		void createTableLine(string mask, int &line_cursor);
		string getLineInTable(int line);
		string getEntryInTable(int line, int line_index);
		unsigned long getEntryLeftNumber(string entry);
		unsigned long getEntryRightNumber(string entry);

		void write_header(int n, bool truncate);
		void write_line_at_index(int index, string line);

		string indexFilename;
		fstream fs;
		string path;

		const static int MASK_SIZE = 1, ENTRY_WIDTH = 22, NUM_WIDTH = (ENTRY_WIDTH - 2 )/2;

		int min_children_per_node;
		int LINE_WIDTH, _table_region_ptr, table_size, last_table_line_written;
};


BTreeFile::BTreeFile(int min_children)
{
	min_children_per_node = min_children;
}

BTreeFile::BTreeFile(string path, int minChildrenPerNode = 128) : HashFile(path)
{
	min_children_per_node = minChildrenPerNode;
	setPath(path);
}

void BTreeFile::setPath(string Path)
{
	path = Path;
	HashFile::setPath(path);
	indexFilename = path + "BTreeFile.txt";

	LINE_WIDTH = ENTRY_WIDTH * pow(16.0, (int)MASK_SIZE) + 1;

	fstream file(indexFilename.c_str(), fstream::in );

	// read first line, which encodes table size
	string line;	
	getline(file, line);	
	table_size = atoi(line.c_str());
	
	_table_region_ptr = file.tellg();

	file.close();
}

void BTreeFile::write_header(int n, bool truncate)
{
	string tableSize = convertIntToString(n);
	tableSize.append(LINE_WIDTH - tableSize.length() - 1, ' ');
	tableSize += "\n";

	fstream file;
	
	if(truncate)
		file.open(indexFilename.c_str(), fstream::out | fstream::trunc);
	else
		file.open(indexFilename.c_str(), fstream::in | fstream::out);

	file.write(tableSize.c_str(), tableSize.size());
	_table_region_ptr = file.tellp();

	file.flush();
	file.close();
}

// write to the specified index in the table; however, if this index
// is out of bounds with regards to the current file size, add the appropriate
// empty spacing

void BTreeFile::write_line_at_index(int index, string line)
{
	fstream file;

	assert(line.size() == LINE_WIDTH);

	if(last_table_line_written < index - 1)
	{
		file.open(indexFilename.c_str(), fstream::out | fstream::app);
		file.width(LINE_WIDTH);
		file.fill(' ');

		while(last_table_line_written < index - 1)
		{
			file << ' ';
			last_table_line_written++;
		}		
		file.flush();
		file.close();
	}	
	else if(last_table_line_written == index - 1)
		last_table_line_written++;

	file.open(indexFilename.c_str(), fstream::in | fstream::out);
	file.seekp(LINE_WIDTH * index + _table_region_ptr);
	file.write(line.c_str(), line.size());
	file.flush();
	file.close();
}


string BTreeFile::getLineInTable(int line_index)
{
	string line;
	fs.seekg(_table_region_ptr + line_index * LINE_WIDTH);
	getline(fs, line);
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
	string num = entry.substr(2 + NUM_WIDTH, NUM_WIDTH)
	;
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
	fs.open(indexFilename.c_str(), fstream::in);
	
	int mask_index = 0, table_line = 0, table_index, begin_index, end_index;
	string masked_key, table_entry;

	do
	{
		masked_key = key.substr(mask_index, MASK_SIZE);
		table_index = convertHexToInt(masked_key);
		table_entry = getEntryInTable(table_line, table_index);
		table_line = getEntryRightNumber(table_entry);

		mask_index += MASK_SIZE;	
	} while(table_entry[0] == 'T');

	fs.close();
	// table_line now contains index into HashFile of where to begin/end search

	unsigned int low = getEntryLeftNumber(table_entry);
	unsigned int high = getEntryRightNumber(table_entry);

	// call HashFile:: classes to extract set of values 
}

void BTreeFile::createTableLine(string mask, int &line_cursor)
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
			createTableLine(new_mask, line_cursor);

			// create a pointer to the recursively created sub-table
			line += "T " + convertIntToString(line_cursor_stored, ENTRY_WIDTH - 2);
		}
	}

	line += "\n";

	write_line_at_index(curr_line, line);
}

void BTreeFile::commit()
{
	//since this data-structure is dependent on a coherent HashFile, we commit it first
	//HashFile::commit(path);

	int line_cursor = 0;
	last_table_line_written = -1;


	//save space for the table-size, which goes on the first-line
	write_header(0, /*truncate existing file*/ true);

	//create the table recursively
	createTableLine(string(""), line_cursor);

	//and finally write the table-size to the first-line
	write_header(line_cursor, false);

}
