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

	bool sortA(const command& d1, const command& d2)
	{
		if(d1.A == d2.A)
 			return(d1.C < d2.C);
 		return d1.A < d2.A;
	}

	bool sortC(const command& d1, const command& d2) 
	{
		if(d1.C == d2.C)
			return(d1.A < d2.A);

		return d1.C < d2.C;	
	}

	command opposite(command c)
	{
		command opposite = c;
		opposite.cmd = (c.cmd == "A" ? "U" : "A");
		return opposite;
	}
};

class LogFile
{
	public:
		LogFile(){}
		LogFile(string filename);
		void setFilename(string filename);
		string getFilename();
		vector<Log::command> readEntries();
		void addEntry(string cmd, string A, string C);
		void clear();

	private:
		string filename;	
		fstream file;	
};

LogFile::LogFile(std::string fname)
{
	setFilename(fname);
}

void LogFile::setFilename(string fname)
{
	filename = fname;
}

string LogFile::getFilename()
{
	return filename;
}

void LogFile::clear()
{
	unlink(filename.c_str());
}

void LogFile::addEntry(string cmd, string A, string C)
{
	file.open(filename.c_str(), fstream::out | fstream::app);

	string line = cmd + " " + A + " " + C + "\n";

	file.write(line.c_str(), line.size());
	file.flush();
	file.close();
}

vector<Log::command> LogFile::readEntries() 
{
	string line;
	vector<Log::command> log;

	file.open(filename.c_str(), fstream::in);

	while(getline(file, line))
	{	
		Log::command entry;

		entry.cmd = line.substr(0, 1);
		entry.A = line.substr(2, SHA_WIDTH);
		entry.C = line.substr(SHA_WIDTH + 3, SHA_WIDTH);
		
		log.push_back(entry);
	}

	file.close();

	return log;
}

#endif