#include "logfile.h"

namespace Log 
{

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
};

LogFile::LogFile(std::string path)
{
	setPath(path);
}

void LogFile::setPath(string path)
{
	filename = path + "log.txt";
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