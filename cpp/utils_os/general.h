/*
 *  file:     general.h
 *  brief:    Some generic C++ utilities for Linux and Windows
 *  created:  2023-05-13
 *  authors:  nvitya
 *  license:  public domain
*/

#ifndef __GENERAL_H_
#define __GENERAL_H_

#include <string>

using namespace std;

#include <stdio.h>
#include <strings.h>
#include <stdlib.h>

#define MILLION  1000000

char * read_file_contents(const char * fname, unsigned * len);
int write_to_file(const char * fname, char * srcbuf, unsigned len);

void StringToUpper(string & strToConvert);
void StringToLower(string & strToConvert);
bool StringReplace(string & str, string tosearch, string toreplace);

bool StringToBool(string & str);
string StringFormat(const char * fmt, ...);

string to_string2(int value);

void sleep_ms(int amillisecs);

#endif // __GENERAL_H_
