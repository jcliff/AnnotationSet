#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <set>
#include <sys/stat.h>
#include <assert.h>
#include "logfile.h"
#include "utils.h"

#ifndef HASHFILE_H
#define HASHFILE_H

#define SHA_WIDTH 40

using namespace std;

class HashFile
{
	public:
		HashFile(){}
		HashFile(string path);
		~HashFile();
		void setPath(string path);
		string getFilename();
		set<string> get(string key);
		void commit(string filename, LogFile &logFile, bool);
		unsigned long length();
		void copyState(string newDirPath);
		void moveState(string dir_path_init, string dir_path_final);

	protected:
		 unsigned long getIndexOfKey(string key);
		 string getKeyAtIndex(unsigned long index);

	private:
		void write_hash_file(string newFilename, vector<string> &commit_lines);
		unsigned long get_aligned_index(unsigned long index, int mode);
		string get_line_at_index(unsigned long index);
		string get_key_at_index(unsigned long index);
		string get_val_at_index(unsigned long index);

		fstream file;
		string filename;
		int _data_region_ptr;
		unsigned long data_size;		
		const static int LINE_WIDTH = SHA_WIDTH * 2 + 2;
};

HashFile::HashFile(string path)
{
	setPath(path);
}

HashFile::~HashFile()
{
	file.close();
}

void HashFile::setPath(string path)
{
	filename = path + "HashFile.txt";
	file.open(filename.c_str(), fstream::in );

	// read first line, which encodes data size
	string line;
	getline(file, line);	
	data_size = atoi(line.c_str());
	_data_region_ptr = file.tellg();
//	file.close();
}

string HashFile::getFilename()
{
	return filename;
}
unsigned long HashFile::length()
{
	return data_size;
}

string HashFile::get_line_at_index(unsigned long index)
{
	string line;
	file.seekg(_data_region_ptr + LINE_WIDTH * index);
	getline(file, line);
	return line;
}

string HashFile::get_key_at_index(unsigned long index)
{

	string line = get_line_at_index(index);
	return(line.substr(0,SHA_WIDTH));

}


string HashFile::get_val_at_index(unsigned long index)
{
	string line = get_line_at_index(index);
	return(line.substr(SHA_WIDTH + 1,SHA_WIDTH));
}

// mode -1: get index corresponding to beginning of this key entry
// mode +1: get index corresponding to end of this key entry

unsigned long HashFile::get_aligned_index(unsigned long index, int mode = -1)
{
	assert(index >=0);
	assert(index < data_size);
			
	string key = get_key_at_index(index);
	while(index >= 0 && index < data_size && get_key_at_index(index) == key)
		index = index + mode;
	return(index - mode);
}

// returns vector of values for specified key; utilizes binary search
set<string> HashFile::get(string key)
{
	set<string> list;
	int idx = getIndexOfKey(key);

	while(idx < data_size && get_key_at_index(idx) == key)
	{
		list.insert(get_val_at_index(idx));
		idx++;
	}

	return list;
}

// returns HashTable index of specified key. If specified key is not
// present, attempts to return the index of the next highest key, but
// will always stay in-bounds. 

unsigned long HashFile::getIndexOfKey(string key) 
{
	string curr_key;
	unsigned long window_low = 0, window_high = data_size - 1, mid;

	if(data_size == 0)
		return 0;
		
	while(window_low <= window_high)
	{
		mid = window_low + (window_high - window_low) / 2;
		curr_key = get_key_at_index(mid);

		if(key == curr_key)
			return(get_aligned_index(mid, -1));
	
		if(key < curr_key && window_high == 0)
			break;
		else if(key < curr_key)
			window_high = mid - 1;
		else
			window_low = mid + 1;
	}

	// handle edge conditions
	if(window_high < 0)
		return 0;
	if(window_low > data_size - 1)
		return data_size - 1;

	// if we are less than current key, simply roll-back index to first key occurrence
	if(key < curr_key)
		return(get_aligned_index(mid,-1));
	// otherwise roll forward to beginning of next key
	else
		return(get_aligned_index(mid,+1) + 1);

}

string HashFile::getKeyAtIndex(unsigned long index)
{
	string line = get_line_at_index(index);	
	return(line.substr(0,SHA_WIDTH));
}	


void swap(string &first, string &second)
{
	string tmp = first;
	first = second;
	second = tmp;
}

void HashFile::copyState(string dir_path)
{
	file.close();
	file_copy(filename.c_str(),(dir_path+"HashFile.txt").c_str());
	file.open(filename.c_str(), fstream::in);
}

void HashFile::moveState(string dir_path_init, string dir_path_final)
{
	file.close();
	rename((dir_path_init + "HashFile.txt").c_str(), (dir_path_final + "HashFile.txt").c_str());
	file.open((dir_path_final + "HashFile.txt").c_str(), fstream::in);
}

//helper function for HashFile::commit; we do not know the final # of entries 
//in the hashtable until we are done merging
void HashFile::write_hash_file(string newPath, vector<string> &commit_lines)
{
	string newFilename = newPath + "HashFile.txt";
	fstream newfile(newFilename.c_str(), fstream::out | fstream::trunc);
	
	stringstream ss;
    ss << commit_lines.size();
    string length = ss.str() + "\n";

    //first write # of entries to file

    newfile.write(length.c_str(), length.size());

    //now write each individual entry
    for(unsigned long i=0; i<commit_lines.size(); i++)
     	newfile.write(commit_lines[i].c_str(), commit_lines[i].size());

    newfile.flush();
    newfile.close();
}

// perform a merge-sort of the HashFile and LogFile
// and use the appropriate logic for annotation / unannotations
void HashFile::commit(string newPath, LogFile &log, bool reverseLog = false)
{
	vector<Log::command> logEntries = log.readEntries();	
	if(!reverseLog)
		sort(logEntries.begin(), logEntries.end(), Log::sortA);
	else
		sort(logEntries.begin(), logEntries.end(), Log::sortC);
			
//	file.open(filename.c_str(), fstream::in);
			
	unsigned long hashIdx = 0, logIdx = 0;
	string hashLine, key, val, logLine, logFirst, logSecond;
	vector<string> commit_lines;

	while(hashIdx < data_size || logIdx < logEntries.size())
	{
		if(hashIdx < data_size)
		{
			hashLine = get_line_at_index(hashIdx) + "\n";
			key = get_key_at_index(hashIdx);
			val = get_val_at_index(hashIdx);
		}

		if(logIdx < logEntries.size())
		{
			logFirst = logEntries[logIdx].A;
			logSecond = logEntries[logIdx].C;
	
			if(reverseLog)	
				swap(logFirst, logSecond);

			logLine = logFirst + " " + logSecond + "\n";
		}

		// we have finished writing from hash-file; simply flush remaining log file	
		if(hashIdx == data_size)
		{
			commit_lines.push_back(logLine);
			logIdx++;
			continue;
		}	
		// we have finished writing log file; simply flush remaining hash-file
		if(logIdx == logEntries.size())
		{
			commit_lines.push_back(hashLine);
			hashIdx++;
			continue;
		}

		// if we have matching entries between hashfile & logfile,
		// at the very least we need to increment BOTH indices.
		// futhermore if the log is 'U', we skip writing anything to disk
		if( key == logFirst && val == logSecond)
		{
			if(logEntries[logIdx].cmd == "A")
				commit_lines.push_back(hashLine);

			hashIdx++;
			logIdx++;

			continue;
		}
		// if the hashfile has a smaller value than the logfile,
		// write hashline to disk and increment hash pointer
		if( key < logFirst || key == logFirst && val < logSecond )
		{
			commit_lines.push_back(hashLine);
			hashIdx++;
			continue;
		}
		else // else do the equivalent for logfile
		{
			commit_lines.push_back(logLine);
			logIdx++;
		}
	}
	
	write_hash_file(newPath, commit_lines);
}


#endif







