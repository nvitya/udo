/*
 *  file:     general.cpp
 *  brief:    Some generic C++ utilities for Linux and Windows
 *  created:  2023-05-13
 *  authors:  nvitya
 *  license:  public domain
*/

#include <stdexcept>
#include <sstream>
#ifdef WIN32
#include "windows.h"
#endif

#include <stdarg.h>
#include "general.h"
#include "unistd.h"

void sleep_ms(int amillisecs)
{
	#ifdef WIN32
		Sleep(amillisecs);
  #else
		usleep(amillisecs * 1000);
  #endif
}

char * read_file_contents(const char * fname, unsigned * len)
{
  char * result;

  FILE *file = fopen(fname, "rb");
  if (!file)
  {
    return NULL;
  }

  fseek(file, 0, SEEK_END);      // seek to end of file
  *len = ftell(file);       // get current file pointer
  fseek(file, 0, SEEK_SET);      // seek back to beginning of file

  result = (char *)malloc(*len);

  fread(result, 1, *len, file);
  fclose(file);

  return result;
}

int write_to_file(const char * fname, char * srcbuf, unsigned len)
{
  FILE * file = fopen(fname, "wb");
  if (!file)
  {
    return -1;
  }

  int result = fwrite(srcbuf, 1, len, file);
  fclose(file);

  return result;
}

void StringToUpper(string & strToConvert)
{
  for (unsigned i = 0; i < strToConvert.length(); ++i)
  {
    strToConvert[i] = char(toupper(strToConvert[i]));
  }
}

void StringToLower(string & strToConvert)
{
  for(unsigned i = 0; i < strToConvert.length(); ++i)
  {
    strToConvert[i] = char(tolower(strToConvert[i]));
  }
}

bool StringReplace(string & str, string tosearch, string toreplace)
{
	int fpos = str.find(tosearch);
	if (fpos >= 0)
	{
		str.replace(fpos, tosearch.length(), toreplace);
		return true;
	}
	else
	{
		return false;
	}
}

string StringFormat(const char * fmt, ...)
{
  va_list arglist;
  va_start(arglist, fmt);

  char fmtbuf[256];

  vsnprintf(&fmtbuf[0], 256, fmt, arglist);

  va_end(arglist);

  return string(&fmtbuf[0]);
}


string to_string2(int value)
{
  //create an output string stream
  std::ostringstream os ;

  //throw the value into the string stream
  os << value ;

  //convert the string stream into a string and return
  return os.str() ;
}

bool StringToBool(string & str)
{
  string s(str);
  StringToUpper(s);

  if ("1" == str)     return true;
  if ("TRUE" == str)  return true;
  if ("YES" == str)   return true;
  if ("Y" == str)     return true;

  return false;
}

// EOF

