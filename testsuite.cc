#include <iostream>
#include <string>
#include <assert.h>
#include <fstream>

#include "annotations.h"
#include "utils.h"

using namespace std;

typedef struct 
{
	string annotation;
	string message;	
} AnnotationPair;

void randomizePairs(vector<AnnotationPair> &pairs)
{
	for(int i = pairs.size()-1; i>=1 ; i--)
	{
		int j = rand()%(i+1);
		AnnotationPair tmp = pairs[i];
		pairs[i] = pairs[j];
		pairs[j] = tmp;
	}
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

//verify that all pairs are either bound or unbound
void verifyAllEntries(AnnotationSet *AS, vector<AnnotationPair> pairs, int state)
{
	for(int i=0; i<pairs.size(); i++)
	{
		set<string>::iterator it;
		string message = pairs[i].message;
		string annotation = pairs[i].annotation;

		set<string> messages = AS->list_entries(annotation);
		set<string> annotations = AS->list_annotations(message);

		assert(messages.count(message) == state);
		assert(annotations.count(annotation) == state);

	}	
}

//set all pairs to be bound or unbound
//where 1: annotate, 0: unannotate
void setAllEntries(AnnotationSet *AS, vector<AnnotationPair> pairs, int state)
{
	//randomizePairs(pairs);

	for(int i=0; i<pairs.size(); i++)
	{
		if(state == 1)
			AS->annotate_entry(pairs[i].annotation, pairs[i].message);
		else	
			AS->unannotate_entry(pairs[i].annotation, pairs[i].message);			

	}
}

//Test if annotations are correctly bound in a live environment.  
//Test ends with all annotation pairs being bound.
//  1. Annotate (A)
//  2. Annotate / unannotate (U)
//  3. A / U / U
//  4. A / U / U / A
//  5. A / U / U / A / A

void runLiveVerification(AnnotationSet *AS, vector<AnnotationPair> pairs)
{
	setAllEntries(AS, pairs, 1);
	verifyAllEntries(AS, pairs, 1);

	setAllEntries(AS, pairs, 0);
	verifyAllEntries(AS, pairs, 0);

	setAllEntries(AS, pairs, 0);
	verifyAllEntries(AS, pairs, 0);

	setAllEntries(AS, pairs, 1);
	verifyAllEntries(AS, pairs, 1);

	setAllEntries(AS, pairs, 1);
	verifyAllEntries(AS, pairs, 1);
}


int main(int argc, char *argv[]) 
{
	string test_bed_directory("testbed");
	string hashTableType("");

	if(argc < 2)
	{
		cout << "USAGE: [A2C SNAPSHOT FILE]" << endl;
		return 0;
	}
	if(argc == 3)
		hashTableType = string(argv[2]);
	
	vector<AnnotationPair> pairs = read_initial_annotations(string(argv[1]));

	//initialize system to blank state
	dir_delete(test_bed_directory);

	AnnotationSet *AS = new AnnotationSet(test_bed_directory, hashTableType);

	AS->initialize();

	cout<<"testing brand-new system..." << endl;
	runLiveVerification(AS, pairs);
	cout<<"done."<<endl<<endl;

	delete(AS);

	AS = new AnnotationSet(test_bed_directory, hashTableType);
	AS->initialize();

	cout<<"verifying initial bootup from log..." << endl;
	verifyAllEntries(AS, pairs, 1);
	cout<<"done."<<endl<<endl;

	cout<<"testing system booted from log..." << endl;
	runLiveVerification(AS, pairs);
	cout<<"done."<<endl<<endl;

	setAllEntries(AS, pairs, 1);
	AS->commit_to_disk();
	delete(AS);

	AS = new AnnotationSet(test_bed_directory, hashTableType);
	AS->initialize();

	cout<<"verifying initial bootup from commited hashfile..." << endl;
	verifyAllEntries(AS, pairs, 1);
	cout<<"done."<<endl<<endl;

	cout<<"testing system booted from commited hashfile..." << endl;
	runLiveVerification(AS, pairs);
	cout<<"done."<<endl<<endl;

	// ************ Below tests are on a FULLY-DELETED system *********************** //

	//unannotate all entries
	setAllEntries(AS, pairs, 0);
	delete(AS);

	AS = new AnnotationSet(test_bed_directory, hashTableType);
	AS->initialize();


	cout<<"verifying fully-deleted system booted from log..." << endl;
	verifyAllEntries(AS, pairs, 0);
	cout<<"done."<<endl<<endl;

	AS->commit_to_disk();
	delete(AS);

	AS = new AnnotationSet(test_bed_directory, hashTableType);
	AS->initialize();
	cout<<"verifying fully-deleted system booted from commited hashfile..."<<endl;
	verifyAllEntries(AS, pairs, 0);
	cout<<"done."<<endl<<endl;
	cout<<"All tests passed."<<endl;

	return 0;	
}
