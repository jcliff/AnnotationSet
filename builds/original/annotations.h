#include <iostream>
#include <tr1/unordered_map>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <set>
#include "hashfile.h"
#include "logfile.h"
     
using namespace std;
using namespace tr1;

class AnnotationSet
{
	public:
		AnnotationSet(string directory_path);
		void initialize();
		void annotate_entry(string A, string C);
		void unannotate_entry(string A, string C);
		set<string> list_annotations(string C);
		set<string> list_entries(string A);
		void commit_to_disk();

	private:
		set<string>& hash_lookup(string, unordered_map<string, set<string> > &map, HashFile &h);
		void modify_entry(string cmd, string A, string C, bool writeLog = true);
		void modify_entry_in_table( unordered_map<string, set<string> > &table, 
									HashFile &hashfile, string cmd, string key, string value );
	
		void compact_log();
		void atomic_write(char value);
		char atomic_read();

		string directory_path, atomic_log_filename;
		HashFile A2C_File, C2A_File;	
		LogFile Log;
		unordered_map<string, set<string> > A2C_Table, C2A_Table;
		unordered_map<string, string> Dirty_Cache_Table;
};

void file_copy(const char *filename1, const char *filename2)
{
	char buf;

	fstream f1(filename1, fstream::in);
    fstream f2(filename2, fstream::out | fstream::trunc);

    while(f1 && f1.get(buf)) 
    	f2.put(buf);

    f1.close();
    f2.close();
}

AnnotationSet::AnnotationSet(string dir_path)
{
	directory_path = dir_path;
	mkdir(directory_path.c_str(),0777);

	atomic_log_filename = directory_path + "/" + "atomic_log.txt";
	A2C_File.setFilename(directory_path + "/" + "A2C.txt");
	C2A_File.setFilename(directory_path + "/" + "C2A.txt");
	Log.setFilename(directory_path + "/" + "log.txt");
}

void AnnotationSet::initialize()
{	
	//A WAL state of 1 means we need to roll-back a failed commit

	if(atomic_read() == '1')
	{
		rename((A2C_File.getFilename() + ".bak").c_str(), A2C_File.getFilename().c_str());
		rename((C2A_File.getFilename() + ".bak").c_str(), C2A_File.getFilename().c_str());
		rename((Log.getFilename() + ".bak").c_str(), Log.getFilename().c_str());

		atomic_write('0');
	}

	vector<Log::command> logEntries = Log.readEntries();

	// now load all commands from the log file, lazily populating the in-memory hashtable
	// when necessary

	for(int i=0; i<logEntries.size(); i++)
		modify_entry(logEntries[i].cmd, logEntries[i].A, logEntries[i].C, /*writeLog*/ false);
}

void AnnotationSet::annotate_entry(string A, string C)
{
	modify_entry("A", A, C, /*writeLog*/ true);
}

void AnnotationSet::unannotate_entry(string A, string C)
{
	modify_entry("U", A, C, /*writeLog*/ true);
}

set<string> AnnotationSet::list_annotations(string C)
{
	return hash_lookup(C, C2A_Table, C2A_File);
}

set<string> AnnotationSet::list_entries(string A)
{
	return hash_lookup(A, A2C_Table, A2C_File);
}

// Perform either an Annotate or Unannotate action
// Both A2C and C2A in-memory hashtables need to be updated, as well as read from disk if currently empty
// Finally, write this action to the log file

void AnnotationSet::modify_entry(string cmd, string A, string C, bool writeLog)
{
	modify_entry_in_table(A2C_Table, A2C_File, cmd, A, C);
	modify_entry_in_table(C2A_Table, C2A_File, cmd, C, A);

	string key = A + C;
	Dirty_Cache_Table[key] = cmd;

	// record action to log, except if we are initializing
	if(writeLog)
		Log.addEntry(cmd, A, C);
}

void AnnotationSet::modify_entry_in_table(
	unordered_map<string, set<string> > &table, 
	HashFile &hashfile,
	string cmd, 
	string key, 
	string value
	)
{
	set<string> *list = &hash_lookup(key, table, hashfile);

	if(cmd == "A")
		list->insert(value);
	
	if(cmd == "U")
		list->erase(value);
}

// return by reference, of in-memory hashtable (populates from disk if necessary)
set<string>& AnnotationSet::hash_lookup(
	string key,
	unordered_map<string, set<string> > &hash_map,
	HashFile &hash_file
	)
{
	// read annotation from disk via binary search, and update in-memory table
	if(hash_map.count(key) == 0)
		hash_map[key] = hash_file.get(key);

	return hash_map[key];
}

char AnnotationSet::atomic_read()
{
	fstream file(atomic_log_filename.c_str(), fstream::in);
	char state;
	file.get(state);
	file.close();
	return state;
}

void AnnotationSet::atomic_write(char state)
{
	fstream file(atomic_log_filename.c_str(), fstream::out | fstream:: trunc);
	file.write(&state, 1);
	file.flush();
	file.close();		
}

void AnnotationSet::commit_to_disk()
{
	string A2C_temp_filename = A2C_File.getFilename() + ".tmp";
	string C2A_temp_filename = C2A_File.getFilename() + ".tmp";
	
	string A2C_backup_filename = A2C_File.getFilename() + ".bak";
	string C2A_backup_filename = C2A_File.getFilename() + ".bak";
	string log_backup_filename = Log.getFilename() + ".bak";

	//in this implementation, logfile MUST be compacted for commit to properly work
	compact_log();

	//commit our on-disk hashtables to a temp file
	A2C_File.commit(A2C_temp_filename, Log, false);
	C2A_File.commit(C2A_temp_filename, Log, true);

	//copy originals to backup files (for rollback purposes)
	file_copy(A2C_File.getFilename().c_str(), A2C_backup_filename.c_str());
	file_copy(C2A_File.getFilename().c_str(), C2A_backup_filename.c_str());
	file_copy(Log.getFilename().c_str(), log_backup_filename.c_str());

	atomic_write('1');

	/////////////////// UNCOMMENT BELOW 3 LINES //////////////////
	//
	rename(A2C_temp_filename.c_str(), A2C_File.getFilename().c_str());
	rename(C2A_temp_filename.c_str(), C2A_File.getFilename().c_str());
	Log.clear();
	// 
	//////////////////////////////////////////////////////////////
	
	atomic_write('0');

}

void AnnotationSet::compact_log()
{
	LogFile log_temp(Log.getFilename() + ".tmp");
	unordered_map<string, string>::iterator it;

	for(it = Dirty_Cache_Table.begin(); it != Dirty_Cache_Table.end(); it++)
	{
		string A = it->first.substr(0, SHA_WIDTH);
		string C = it->first.substr(SHA_WIDTH, SHA_WIDTH);
		string cmd = it->second;

		log_temp.addEntry(cmd, A, C);
	}

	rename(log_temp.getFilename().c_str(), Log.getFilename().c_str());

}

