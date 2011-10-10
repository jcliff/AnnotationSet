#include "utils.h"

void dir_delete(string dir)
{
	struct dirent *de = NULL;
	DIR *d = NULL;

	if( (d = opendir(dir.c_str())) == NULL)
		return;

	while(de = readdir(d))
	{
		if(strcmp(de->d_name,".") == 0 || strcmp(de->d_name,"..") == 0)
			continue;

		if(de->d_type == DT_DIR)
		{
			dir_delete(dir + "/" + de->d_name);
			rmdir(dir.c_str());
		}
		else
			unlink((dir + "/" + de->d_name).c_str());	
	}

	rmdir(dir.c_str());
}

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

void convertHexToByteArray(unsigned char *byteArray, string s)
{
	unsigned int n;

	for(int i=0; i<s.size()/2; i++)
	{
		stringstream ss;
		ss << hex << s.substr(i*2,2);
		ss >> n;
		byteArray[s.size()/2-1-i] = n;
	}

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
string convertIntToHex(int n, int width)
{
	stringstream ss;
	string str;
	ss << hex << n;
	str = ss.str();
	while(str.size() < width)
		str = "0" + str;

	return str;
}

string convertIntToString(int n, int width)
{
	stringstream ss;
	string str;
	ss << n;
	str = ss.str();
	while(str.size() < width)
		str = " " + str;

	return str;
}
