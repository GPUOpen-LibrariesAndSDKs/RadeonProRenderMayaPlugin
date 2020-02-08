#include <iostream>
#include <fstream>
#include <string>

using namespace std;

ofstream logfile;
ostream* outmsg;

int main(int argc, const char *argv[])
{
	outmsg = &cout;
	logfile.open("log.txt");
}
