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
		virtual ~HashFile();
		virtual void setPath(string path);
		virtual set<string> get(string key);
		virtual void commit(string filename, LogFile &logFile, bool);
		virtual void copyState(string newDirPath);
		virtual void moveState(string dirPathInit, string dirPathFinal);

	protected:
		unsigned long getIndexOfKey(string key, unsigned long window_low, unsigned long window_high);
		string getKeyAtIndex(unsigned long index);
		set<string> get(string key, unsigned long window_low, unsigned long window_high);
		unsigned long getIndexOfKey(string key);
		unsigned long length();

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


#endif







