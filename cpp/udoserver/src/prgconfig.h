// prgconfig_base.h

#ifndef DLCORE_PRGCONFIG_BASE_H_
#define DLCORE_PRGCONFIG_BASE_H_

#include <string>
#include <vector>
#include <map>
#include "stdint.h"

#include "strparse.h"

using namespace std;

class TPrgConfig
{
public:
  string    filename;

  char *    filedata = NULL;
  unsigned  filedatasize;

  bool      error = false;
  string    errormsg = "";

public:
  string    udosl_devaddr = "/dev/ttyACM0";

public:
  virtual   ~TPrgConfig() { }
  void      Reset();

  bool      ReadConfigFile(string fname);
  bool      ParseConfig();

  void      SkipSemiColon();

  double    ParseNumAssignment();
  double    ParseNumValue();  // integer expression parser
  bool      ParseBoolAssignment();
  bool      ParseBoolValue();

  string    ParseStringAssignment();
  string    ParseStringValue();
  string    ParseIdentifier();

  bool      ParseSetScriptVar(int ascopelevel);

  int       GetBitLengthFromType(string vtype);

  virtual bool ParseConfigLine(string idstr);

public:
  TStrParseObj strparser;
  PStrParseObj sp;

  int          slotcount;

};

extern TPrgConfig prgconfig;

#endif /* DLCORE_PRGCONFIG_BASE_H_ */
