#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include "logfile.h"

#ifndef HASHFILE_H
#define HASHFILE_H

#define SHA_WIDTH 40

using namespace std;

class HashFile
{
	public:
		HashFile(){}
		HashFile(string filename);
		void setFilename(string filename);
		string getFilename();
		std::vector<string> get(string key);
		void commit(string filename, LogFile &logFile, bool);
		int length();
	private:
		int get_aligned_index(int index, int mode);
		string get_line_at_index(int index);
		string get_key_at_index(int index);
		string get_val_at_index(int index);
		void write_hash_file(string newFilename, vector<string> &commit_lines);

		fstream file;
		string filename;
		int _data_region_ptr, data_size;		
		const static int LINE_WIDTH = SHA_WIDTH * 2 + 2;
};

HashFile::HashFile(string fname)
{
	setFilename(fname);
}

void HashFile::setFilename(string fname)
{
	filename = fname;
	file.open(filename.c_str(), fstream::in );

	// read first line, which encodes data size
	string line;
	getline(file, line);	
	data_size = atoi(line.c_str());
	_data_region_ptr = file.tellg();

	file.close();
}

string HashFile::getFilename()
{
	return filename;
}
int HashFile::length()
{
	return data_size;
}

string HashFile::get_line_at_index(int index)
{
	string line;
	file.seekg(_data_region_ptr + LINE_WIDTH * index);
	getline(file, line);
	return line;
}

string HashFile::get_key_at_index(int index)
{
	string line = get_line_at_index(index);
	return(line.substr(0,SHA_WIDTH));
}


string HashFile::get_val_at_index(int index)
{
	string line = get_line_at_index(index);
	return(line.substr(SHA_WIDTH + 1,SHA_WIDTH));
}

// mode -1: get index corresponding to beginning of this key entry
// mode +1: get index corresponding to end of this key entry

int HashFile::get_aligned_index(int index, int mode = -1)
{
	string key = get_key_at_index(index);
	while(index >= 0 && index < data_size && get_key_at_index(index) == key)
		index = index + mode;
	return(index - mode);
}

// returns vector of values for specified key; utilizes binary search
vector<string> HashFile::get(string key)
{
	string curr_key;
	int window_low = 0, window_high = data_size - 1, mid_begin, mid_end;
	vector<string> list;
	file.open(filename.c_str(), fstream::in);

	//window_low and window_high always point to the beginning of a key/value section
	//mid_begin and mid_end are the encompassing indices of the key corresponding to mid
	while(window_low <= window_high)
	{
		mid_begin = get_aligned_index(window_low + (window_high - window_low) / 2, -1);
		mid_end = get_aligned_index(window_low + (window_high - window_low) / 2, +1);
		curr_key = get_key_at_index(mid_begin);
		if(key == curr_key)
			break;

		if(key < curr_key)
			window_high = get_aligned_index(mid_begin - 1);	
		else
			window_low = get_aligned_index(mid_end + 1);
	}
	
	//nothing found, return empty list
	if( key != curr_key)
	{
		file.close();
		return list;
	}

	// generate list of values for specified key
	for(int i=mid_begin; i<= mid_end; i++)
		list.push_back(get_val_at_index(i));

	file.close();
	return list;	
}


void swap(string &first, string &second)
{
	string tmp = first;
	first = second;
	second = tmp;
}

//helper function for HashFile::commit; we do not know the final # of entries 
//in the hashtable until we are done merging
void HashFile::write_hash_file(string newFilename, vector<string> &commit_lines)
{
	fstream newfile(newFilename.c_str(), fstream::out | fstream::trunc);
	
	stringstream ss;
    ss << commit_lines.size();
    string length = ss.str() + "\n";

    //first write # of entries to file

    newfile.write(length.c_str(), length.size());

    //now write each individual entry
    for(int i=0; i<commit_lines.size(); i++)
    	newfile.write(commit_lines[i].c_str(), commit_lines[i].size());

    newfile.flush();
    newfile.close();
}

// perform a merge-sort of the HashFile and LogFile
// and use the appropriate logic for annotation / unannotations
void HashFile::commit(string newFilename, LogFile &log, bool reverseLog = false)
{
	vector<Log::command> logEntries = log.readEntries();	
	if(!reverseLog)
		sort(logEntries.begin(), logEntries.end(), Log::sortA);
	else
		sort(logEntries.begin(), logEntries.end(), Log::sortC);
			
	file.open(filename.c_str(), fstream::in);
			
	int hashIdx = 0, logIdx = 0;
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

	file.close();
	
	write_hash_file(newFilename, commit_lines);
}


#endif







