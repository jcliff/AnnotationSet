#include <fstream>
#include <string>
#include <vector>
#include <tr1/unordered_map>

#ifndef LOGFILE_H
#define LOGFILE_H

#define SHA_WIDTH 40

using namespace std;
using namespace tr1;

namespace Log{
	typedef struct 
	{
		string cmd; // can be 'A' or 'U' for Annotate & Unannotate
		string A;
		string C;

		string toString()
		{
			return( cmd + " " + A + " " + C );
		}
	} command;

	bool sortA(const command& d1, const command& d2);

	bool sortC(const command& d1, const command& d2);
};

class LogFile
{
	public:
		LogFile(){}
		LogFile(string path);
		void setPath(string path);
		string getFilename();
		vector<Log::command> readEntries();
		void addEntry(string cmd, string A, string C);
		void clear();

	private:
		string filename;	
		fstream file;	
};

#endif