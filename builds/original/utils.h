#include <fstream>
#include <string>
#include <sstream>

#ifndef UTILS_H
#define UTILS_H

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


unsigned int convertHexToInt(string s)
{
	unsigned int n;   
	stringstream ss;
	ss << hex << s;
	ss >> n;

	return n;
}

//perform the conversion 
string convertIntToHex(int n, int width = 0)
{
	stringstream ss;
	string str;
	ss << hex << n;
	str = ss.str();
	while(str.size() < width)
		str = "0" + str;

	return str;
}

string convertIntToString(int n, int width = 0)
{
	stringstream ss;
	string str;
	ss << n;
	str = ss.str();
	while(str.size() < width)
		str = " " + str;

	return str;
}

#endif