#include <fstream>
#include <string>
#include <sstream>
#include <dirent.h>

#ifndef UTILS_H
#define UTILS_H

using namespace std;

void dir_delete(string dir);

void file_copy(const char *filename1, const char *filename2);

void convertHexToByteArray(unsigned char *byteArray, string s);

unsigned int convertHexToInt(string s);
 
string convertIntToHex(int n, unsigned int width = 0);

string convertIntToString(int n, unsigned int width = 0);

#endif