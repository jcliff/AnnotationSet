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

		int min_children_per_node;
		int LINE_WIDTH, _table_region_ptr;
		unsigned long table_size, last_table_line_written;
};


BTreeFile::BTreeFile(string path, int minChildrenPerNode = 64) : HashFile()
{
	min_children_per_node = minChildrenPerNode;
	LINE_WIDTH = ENTRY_WIDTH * pow(16.0, (int)MASK_SIZE);
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
	
	if(file.is_open())
		file.close();

	file.open(filename.c_str(), fstream::in | fstream::binary);

	if(!file.good())
		return;

	// read first 4 bytes, which encodes table size
	table_size = 0;
	file.read((char *) &table_size, 4);
	_table_region_ptr = file.tellg();
}

void BTreeFile::write_header(string newPath, unsigned long size, bool truncate)
{
	fstream newFile;
	string newFilename = newPath + "BTreeFile.txt";

	if(truncate)
		newFile.open(newFilename.c_str(), fstream::out | fstream::trunc | fstream::binary);
	else
		newFile.open(newFilename.c_str(), fstream::in | fstream::out | fstream::binary);

	newFile.write((char *) &size, 4);
	_table_region_ptr = newFile.tellp();

	newFile.flush();
	newFile.close();
}

// write to the specified index in the table; however, if this index
// is out of bounds with regards to the current file size, add the appropriate
// empty spacing

void BTreeFile::write_line_at_index(string newPath, unsigned long index, char *line)
{
	fstream newFile;
	string newFilename = newPath + "BTreeFile.txt";

	if(last_table_line_written < index - 1)
	{
		newFile.open(newFilename.c_str(), fstream::out | fstream::app | fstream::binary);
		char fill = 0x00;

		while(last_table_line_written < index - 1)
		{
			for(int i = 0; i < LINE_WIDTH; i++)
				newFile.write(&fill, 1);

			last_table_line_written++;
		}		
		newFile.flush();
		newFile.close();
	}	
	else if(last_table_line_written == index - 1)
		last_table_line_written++;

	newFile.open(newFilename.c_str(), fstream::in | fstream::out | fstream::binary);
	newFile.seekp(LINE_WIDTH * index + _table_region_ptr);
	newFile.write(line, LINE_WIDTH);
	newFile.flush();
	newFile.close();
}


string BTreeFile::getLineInTable(unsigned long line_index)
{
	string line;
	file.seekg(_table_region_ptr + line_index * LINE_WIDTH);
	getline(file, line);
	return line;
}

void BTreeFile::getEntryInTable(char *buf, unsigned long line_num, int line_index)
{
	 
	file.seekg(_table_region_ptr + line_num * LINE_WIDTH + line_index * ENTRY_WIDTH);
	file.read(buf, ENTRY_WIDTH);
}

// iteratively follow the pointers in the table until we get to a line marked
// 'XXXXX...' (no key) or 'L [address1][address2]' (line #'s surrounding key in HashFile)

set<string> BTreeFile::get(string key)
{
	set<string> list;
	string masked_key;
	int mask_index = 0, table_index;
	unsigned long table_line = 0, begin_index, end_index;

	char *table_entry = new char[ENTRY_WIDTH];

	if(table_size == 0)
		return list;
	
	do
	{
		masked_key = key.substr(mask_index, MASK_SIZE);
		table_index = convertHexToInt(masked_key);
		getEntryInTable(table_entry, table_line, table_index);

		table_line = 0;
		memcpy(&table_line, &table_entry[NUM_WIDTH + 1], NUM_WIDTH);
		mask_index += MASK_SIZE;	

	} while(table_entry[0] == TABLE_PTR_FLAG);

	if(table_entry[0] == EMPTY_FLAG)
		return list;

	// table_line now contains index into HashFile of where to begin/end search
	unsigned long low = 0, high = 0;
	memcpy(&low, &table_entry[1], NUM_WIDTH);
	memcpy(&high, &table_entry[NUM_WIDTH+1], NUM_WIDTH);

	// call HashFile:: classes to extract set of values 
	return HashFile::get(key, low, high);
}

void BTreeFile::createTableLine(string newPath, string mask, unsigned long &line_cursor)
{	
	// line_cursor lets us know the next free line available in the table
	unsigned long curr_line_pos = line_cursor;
	line_cursor++;
	char *line = new char[LINE_WIDTH];
	char *entry = new char[ENTRY_WIDTH];

	// iterate through all integers covering the mask
	// for each mask:
	//     1.  if the key does not exist in HashTable, mark entry as 'XX..'
	//     2.  if the key spans less than min_children_per_node entries, mark entry as 'L [index_begin][index_end]'
	//     3.  else mark entry as 'T [table-line]' and recursively generate sub-table
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

		string key = HashFile::getKeyAtIndex(index_begin);

		// check if this mask is even present in HashFile
		if(key.substr(0, new_mask.size()) != new_mask)
		{			
			entry[0] = EMPTY_FLAG;
			continue;
		}

		unsigned long index_end = HashFile::getIndexOfKey(new_mask_end);

		// only create a sub-table through recursion, if the range covered by the 
		// new mask exceeds min_children_per_node. Otherwise, record the line# in the table
		// note that 'children' corresponds to unique key/val pairs; may infact cover the same key more than once
		if(index_end - index_begin < min_children_per_node)
		{
			entry[0] = LINE_IDX_FLAG;
			memcpy(&entry[1], &index_begin, NUM_WIDTH);
			memcpy(&entry[1+NUM_WIDTH], &index_end, NUM_WIDTH);
		}
		else
		{
			unsigned long line_cursor_stored = line_cursor;
			createTableLine(newPath, new_mask, line_cursor);

			// create a pointer to the recursively created sub-table
			entry[0] = TABLE_PTR_FLAG;
			memcpy(&entry[1+NUM_WIDTH], &line_cursor_stored, NUM_WIDTH);
		}

		memcpy(&line[i * ENTRY_WIDTH], entry, ENTRY_WIDTH);
	}

	write_line_at_index(newPath, curr_line_pos, line);
}

void BTreeFile::commit(string newPath, LogFile &log, bool reverseLog = false)
{
	//since this data-structure is dependent on a coherent HashFile, we commit it first
	HashFile::commit(newPath, log, reverseLog);
	HashFile::setPath(newPath);

	unsigned long line_cursor = 0;
	last_table_line_written = -1;

	//create new file
	write_header(newPath, 0, /*truncate existing file*/ true);

	if(HashFile::length() != 0)
	{
		//create the table recursively
		createTableLine(newPath, string(""), line_cursor);

		//and finally write the table-size to the first-line_cursor_stored
		write_header(newPath, line_cursor, false);
	}
	//revert to initial path
	HashFile::setPath(path);
}


void BTreeFile::copyState(string dir_path)
{
	HashFile::copyState(dir_path);

	file.close();
	file_copy(filename.c_str(),(dir_path+"BTreeFile.txt").c_str());
	file.open(filename.c_str(), fstream::in | fstream::binary);
}

void BTreeFile::moveState(string dir_path_init, string dir_path_final)
{
	HashFile::moveState(dir_path_init, dir_path_final);

	file.close();
	rename((dir_path_init + "BTreeFile.txt").c_str(), (dir_path_final + "BTreeFile.txt").c_str());
	setPath(dir_path_final);
}
