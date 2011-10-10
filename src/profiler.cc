#include <iostream>
#include <string>
#include <assert.h>
#include <dirent.h>
#include <time.h>
#include <set>

#include "utils.h"
#include "annotations.h"

using namespace std;

typedef struct 
{
	string annotation;
	string message;	
} AnnotationPair;


void randomize_vector(vector<string> &vec)
{
	for(int i = vec.size()-1; i>=1 ; i--)
	{
		int j = rand()%(i+1);
		string tmp = vec[i];
		vec[i] = vec[j];
		vec[j] = tmp;
	}
}

vector<string> generate_rand_vector_from_set(set<string> queries)
{
	set<string>::iterator it;
	vector<string> randVec;

	for(it = queries.begin(); it != queries.end(); it++)
		randVec.push_back(*it);	
	
	randomize_vector(randVec);

	return randVec;
}

vector<AnnotationPair> read_initial_annotations(string filename)
{
 	vector<AnnotationPair> pairs;
	AnnotationPair pair;

	fstream file(filename.c_str(), fstream::in);
	string line; 

	while(getline(file, line))
	{
		pair.annotation = line.substr(0, SHA_WIDTH);
		pair.message = line.substr(SHA_WIDTH + 1, SHA_WIDTH);
		pairs.push_back(pair);
	}	

	file.close();
	return pairs;
}

// pure read performance 
// mode = 1 : list_entries
// mode = 2 : list_annotations

unsigned long readSystem(AnnotationSet *AS, vector<string> queries, int mode)
{
	unsigned long total_cycles = 0, timer = 0;

	for(unsigned long i=0; i<queries.size(); i++)
	{
		string query = queries[i];;
		
		timer = clock();

		if(mode == 1)
			AS->list_entries(query);
		else
			AS->list_annotations(query);
		
		total_cycles += clock() - timer;
	}
	return total_cycles;

}


int main(int argc, char *argv[]) 
{
	string test_bed_directory("testbed");
	set<string> annotations, messages;
	string hashTableType("");

	if(argc < 2)
	{
		cout << "USAGE: [A2C SNAPSHOT FILE]" << endl;
		return 0;
	}	

	if(argc > 2)
		hashTableType = string("BTreeFile");

	//initialize system to blank state
	dir_delete(test_bed_directory);

	vector<AnnotationPair> pairs = read_initial_annotations(string(argv[1]));
	AnnotationSet *AS = new AnnotationSet(test_bed_directory, hashTableType);

	cout << "initializing... "; cout.flush();
	for(unsigned long i=0; i<pairs.size(); i++)
	{
		AS->annotate_entry(pairs[i].annotation, pairs[i].message);
		annotations.insert(pairs[i].annotation);
		messages.insert(pairs[i].message);
	}

	vector<string> randAnnotations = generate_rand_vector_from_set(annotations);
	vector<string> randMessages = generate_rand_vector_from_set(messages);


	AS->commit_to_disk();
	delete(AS);
	cout <<"done." << endl;
	cout <<"running profiler... " << endl; cout.flush();

	// run both the default implementation (vanilla HashFile)
	// as well as the BTreeFile implementation

	AS = new AnnotationSet(test_bed_directory);
	AS->initialize();

	unsigned long entries_time = readSystem(AS, randAnnotations, 1);
	unsigned long annotations_time = readSystem(AS, randMessages, 2);

	cout<<"list_entries (cycles): " << entries_time << endl;
	cout<<"list_annotations (cycles): " << annotations_time << endl;
	delete(AS);

	// run test using BTreeFile
	if(hashTableType == "BTreeFile")
	{
		AS = new AnnotationSet(test_bed_directory, hashTableType);
		AS->initialize();

		entries_time = readSystem(AS, randAnnotations, 1);
		annotations_time = readSystem(AS, randMessages, 2);

		cout<<"BTreeFile - list_entries (cycles): " << entries_time << endl;
		cout<<"BTreeFile - list_annotations (cycles): " << annotations_time << endl;
	}
	
	delete(AS);

	return 0;
}