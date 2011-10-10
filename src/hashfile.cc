#include "hashfile.h"

HashFile::HashFile(string path)
{
	setPath(path);
}

HashFile::~HashFile()
{
	file.close();
}

// change the working-directory for hashfile, and close the existing
// file if necessary

void HashFile::setPath(string path)
{
	if(file.is_open())
		file.close();

	filename = path + "HashFile.txt";

	file.open(filename.c_str(), fstream::in );

	// read first line (encoding data size) and set _data_region_ptr

	string line;
	getline(file, line);	
	data_size = atoi(line.c_str());
	_data_region_ptr = file.tellg();
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
	string key = get_key_at_index(index);

	// convert index to signed, to simplify handling of boundary condition below
	long long idx = index;

	while(idx >= 0 && idx < data_size && get_key_at_index(idx) == key)
		idx = idx + mode;

	return(idx - mode);
}

unsigned long HashFile::length()
{
	return data_size;
}

string HashFile::getKeyAtIndex(unsigned long index)
{
	string line = get_line_at_index(index);	
	return(line.substr(0,SHA_WIDTH));
}	

// returns set of values for specified key; utilizes binary search

set<string> HashFile::get(string key)
{
	return get(key, 0, data_size - 1);
}

// this Protected function allows children objects to specify the beginning and 
// ending indices to constrain the binary-search to

set<string> HashFile::get(string key, unsigned long window_low, unsigned long window_high)
{
	set<string> list;
	unsigned long idx = getIndexOfKey(key, window_low, window_high);

	while(idx < data_size && get_key_at_index(idx) == key)
	{
		list.insert(get_val_at_index(idx));
		idx++;
	}

	return list;
}

unsigned long HashFile::getIndexOfKey(string key)
{
	return getIndexOfKey(key, 0, data_size - 1);	
}

// this Protected function returns HashTable index of specified key. If specified key is not
// present, attempts to return the index of the next highest key, but will always stay in-bounds. 

unsigned long HashFile::getIndexOfKey(string key, unsigned long window_low, unsigned long window_high) 
{
	string curr_key;
	unsigned long mid;

	if(data_size == 0)
		return 0;
	
	while(window_low <= window_high)
	{
		mid = window_low + (window_high - window_low) / 2;
		curr_key = get_key_at_index(mid);

		if(key == curr_key)
			return(get_aligned_index(mid, -1));
	
		if(key < curr_key && mid == 0)
			return 0;
		
		else if(key < curr_key)
			window_high = mid - 1;
		else
			window_low = mid + 1;
	}

	// handle edge conditions
	if(window_low > data_size - 1)
		return data_size - 1;

	// if we are less than current key, simply roll-back index to first key occurrence
	if(key < curr_key)
		return(get_aligned_index(mid,-1));
	// otherwise roll forward to beginning of next key
	else
		return(get_aligned_index(mid,+1) + 1);

}

// copies the state of the HashFile to another directory

void HashFile::copyState(string dir_path)
{
	file.close();
	file_copy(filename.c_str(),(dir_path+"HashFile.txt").c_str());
	file.open(filename.c_str(), fstream::in);
}

// moves the state of the HashFile to another directory

void HashFile::moveState(string dir_path_init, string dir_path_final)
{
	file.close();
	rename((dir_path_init + "HashFile.txt").c_str(), (dir_path_final + "HashFile.txt").c_str());
	setPath(dir_path_final);
}

// perform a merge-sort of the HashFile and a compacted LogFile
// and use the appropriate logic for annotation / unannotations
void HashFile::commit(string newPath, LogFile &log, bool reverseLog = false)
{
	vector<Log::command> logEntries = log.readEntries();	
	unsigned long entriesDeleted = 0;

	if(!reverseLog)
		sort(logEntries.begin(), logEntries.end(), Log::sortA);
	else
		sort(logEntries.begin(), logEntries.end(), Log::sortC);
						
	unsigned long hashIdx = 0, logIdx = 0;
	string hashLine, key, val, logLine, logFirst, logSecond;
	vector<string> commit_lines;

	string newFilename = newPath + "HashFile.txt";
	fstream newFile(newFilename.c_str(), fstream::out | fstream::trunc);

	string blank(LINE_WIDTH - 1, ' ');
	blank += "\n";
	newFile.write(blank.c_str(), blank.size());

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
				swapString(logFirst, logSecond);

			logLine = logFirst + " " + logSecond + "\n";
		}

		// we have finished writing from hash-file; simply flush remaining log file	
		if(hashIdx == data_size)
		{
			newFile.write(logLine.c_str(), logLine.size());
			logIdx++;
			continue;
		}	
		// we have finished writing log file; simply flush remaining hash-file
		if(logIdx == logEntries.size())
		{
			newFile.write(hashLine.c_str(), hashLine.size());
			hashIdx++;
			continue;
		}

		// if we have matching entries between hashfile & logfile,
		// at the very least we need to increment BOTH indices.
		// futhermore if the log is 'U', we skip writing anything to disk
		if( key == logFirst && val == logSecond)
		{
			entriesDeleted++;

			if(logEntries[logIdx].cmd == "A")
				newFile.write(hashLine.c_str(), hashLine.size());
			else
				entriesDeleted++;

			hashIdx++;
			logIdx++;

			continue;
		}
		// if the hashfile has a smaller value than the logfile,
		// write hashline to disk and increment hash pointer
		if( key < logFirst || key == logFirst && val < logSecond )
		{
			newFile.write(hashLine.c_str(), hashLine.size());
			hashIdx++;
			continue;
		}
		else // else do the equivalent for logfile
		{
			newFile.write(logLine.c_str(), logLine.size());
			logIdx++;
		}
	}
	
	stringstream ss;
    ss << data_size + logEntries.size() - entriesDeleted;
    string length = ss.str();

	newFile.seekp(0);
	newFile.write(length.c_str(), length.size());

	newFile.flush();
	newFile.close();
}
