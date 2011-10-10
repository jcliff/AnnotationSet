#include "annotations.h"

// type: refers to HashFile implementation. 'btree' or blank

AnnotationSet::AnnotationSet(string dir_path, string hashTableType)
{
	directory_path = dir_path;

	mkdir(directory_path.c_str(),0777);
	mkdir((directory_path + "/A2C/").c_str(),0777);
	mkdir((directory_path + "/C2A/").c_str(),0777);
	mkdir((directory_path + "/LOG/").c_str(),0777);
	mkdir((directory_path + "/A2C-bak/").c_str(),0777);
	mkdir((directory_path + "/C2A-bak/").c_str(),0777);
	mkdir((directory_path + "/A2C-tmp/").c_str(),0777);
	mkdir((directory_path + "/C2A-tmp/").c_str(),0777);

	atomic_log_filename = directory_path + "/" + "atomic_log.txt";

	Log.setPath(directory_path + "/LOG/");

	if(hashTableType == string("BTreeFile"))
	{
		A2C_File = new BTreeFile(directory_path + "/A2C/");
		C2A_File = new BTreeFile(directory_path + "/C2A/");
	}
	else
	{

		A2C_File = new HashFile(directory_path + "/A2C/" );
		C2A_File = new HashFile(directory_path + "/C2A/" );
	}
}

AnnotationSet::~AnnotationSet()
{
	delete(A2C_File);
	delete(C2A_File);
}

void AnnotationSet::initialize()
{	
	//A WAL state of 1 means we need to roll-back a failed commit

	if(atomic_read() == '1')
	{
		A2C_File->moveState(directory_path + "/A2C-bak/", directory_path + "/A2C/");
		C2A_File->moveState(directory_path + "/C2A-bak/", directory_path + "/C2A/");
		
		rename((Log.getFilename() + ".bak").c_str(), Log.getFilename().c_str());

		atomic_write('0');
	}

	vector<Log::command> logEntries = Log.readEntries();

	// now load all commands from the log file, lazily populating the in-memory hashtable
	// when necessary

	for(unsigned long i=0; i<logEntries.size(); i++)
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
	return hash_lookup(C, C2A_Memory_Map, C2A_File);
}

set<string> AnnotationSet::list_entries(string A)
{
	return hash_lookup(A, A2C_Memory_Map, A2C_File);
}

// Perform either an Annotate or Unannotate action
// Both A2C and C2A in-memory hashtables need to be updated, as well as read from disk if currently empty
// Finally, write this action to the log file

void AnnotationSet::modify_entry(string cmd, string A, string C, bool writeLog)
{
	modify_entry_in_table(A2C_Memory_Map, A+C, A2C_File, cmd, A, C);
	modify_entry_in_table(C2A_Memory_Map, A+C, C2A_File, cmd, C, A);

	// record action to log, except if we are initializing
	if(writeLog)
		Log.addEntry(cmd, A, C);
}

// returns whether or not the entry was found
void AnnotationSet::modify_entry_in_table(
	unordered_map<string, set<string> > &table, 
	string cache_key,
	HashFile *hashfile,
	string cmd, 
	string key, 
	string value
	)
{
	set<string> *list = &hash_lookup(key, table, hashfile);

	if(Cache_Table.count(cache_key) == 0)
		Cache_Table[cache_key].file_state = list->count(value);

	if(cmd == "A")
		list->insert(value).second;

	if(cmd == "U")
		list->erase(value);

	Cache_Table[cache_key].memory_state = (cmd == "A" ? 1 : 0);	
}

// return by reference, of in-memory hashtable (populates from disk if necessary)
set<string>& AnnotationSet::hash_lookup(
	string key,
	unordered_map<string, set<string> > &hash_map,
	HashFile *hash_file
	)
{
	// read annotation from disk via binary search, and update in-memory table
	if(hash_map.count(key) == 0)
		hash_map[key] = hash_file->get(key);

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
	
	string log_backup_filename = Log.getFilename() + ".bak";

	//in this implementation, logfile MUST be compacted for commit to properly work
	compact_log();

	//commit our on-disk hashtables to a temp file
	A2C_File->commit(directory_path + "/A2C-tmp/", Log, false);
	C2A_File->commit(directory_path + "/C2A-tmp/", Log, true);

	//copy originals to backup files (for rollback purposes)
	A2C_File->copyState(directory_path + "/A2C-bak/");
	C2A_File->copyState(directory_path + "/C2A-bak/");

	file_copy(Log.getFilename().c_str(), log_backup_filename.c_str());

	///////////////////////// ATOMIC ACTION ////////////////////////////////////
	atomic_write('1');
	A2C_File->moveState(directory_path + "/A2C-tmp/", directory_path + "/A2C/");
	C2A_File->moveState(directory_path + "/C2A-tmp/", directory_path + "/C2A/");
	Log.clear();	
	atomic_write('0');
	////////////////////////////////////////////////////////////////////////////

}

void AnnotationSet::compact_log()
{
	LogFile log_temp(Log.getFilename() + ".tmp");
	unordered_map<string, CacheLine>::iterator it;

	for(it = Cache_Table.begin(); it != Cache_Table.end(); it++)
	{
		string A = it->first.substr(0, SHA_WIDTH);
		string C = it->first.substr(SHA_WIDTH, SHA_WIDTH);
		CacheLine cache_line = it->second;

		if(cache_line.file_state == cache_line.memory_state)
			continue;
			
		log_temp.addEntry(cache_line.file_state == 0 ? "A" : "U", A, C);
	}

	rename(log_temp.getFilename().c_str(), Log.getFilename().c_str());

}
