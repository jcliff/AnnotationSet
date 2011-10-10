#include <iostream>
#include <tr1/unordered_map>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <set>
#include "hashfile.h"
#include "logfile.h"
#include "utils.h"
#include "btreefile.h"
    
using namespace std;
using namespace tr1;


// a cacheLine is associated to a particular A,C pair
typedef struct
{
	// state defined as
	// 0: not present
	// 1: present

	// records whether this was in the hash-file
	int file_state;

	// records whether this is in memory
	int memory_state;

}	CacheLine;

class AnnotationSet
{
	public:
		AnnotationSet(string directory_path, string hashTableType = "");
		~AnnotationSet();
		void initialize();
		void annotate_entry(string A, string C);
		void unannotate_entry(string A, string C);
		set<string> list_annotations(string C);
		set<string> list_entries(string A);
		void commit_to_disk();

	private:
		set<string>& hash_lookup(string, unordered_map<string, set<string> > &map, HashFile *h);
		void modify_entry(string cmd, string A, string C, bool writeLog = true);
		void modify_entry_in_table( unordered_map<string, set<string> > &table, string cache_key, 
									HashFile *hashfile, string cmd, string key, string value );
	
		void compact_log();
		void atomic_write(char value);
		char atomic_read();

		string directory_path, atomic_log_filename;
		HashFile *A2C_File, *C2A_File;	
		LogFile Log;
		unordered_map<string, set<string> > A2C_Memory_Map, C2A_Memory_Map;
		unordered_map<string, CacheLine> Cache_Table;
};


