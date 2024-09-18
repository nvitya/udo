
#include <prgconfig.h>
#include "string.h"
#include "general.h"

TPrgConfig prgconfig;

void TPrgConfig::Reset()
{
}

bool TPrgConfig::ReadConfigFile(string fname)
{
  sp = &strparser;

  error = false;

  filename = fname;

  Reset();

  if (filedata)
  {
    free(filedata);
    filedata = NULL;
  }

  filedata = read_file_contents(&filename[0], &filedatasize);
  if (!filedata)
  {
    error = true;
    errormsg = string("File \"") + fname + string("\" can not be read.");
  }

  if (!error)
  {
    error = !ParseConfig();
  }

  return !error;
}

bool TPrgConfig::ParseConfig()
{
  strparser.Init(filedata, filedatasize);
  sp = &strparser;
  errormsg = "";

    // add global variables
  //scriptvars.SetStrVar(0, "GLOBAL_ECADAPTER", global_ecadapter);

  sp->SkipWhite();

  if (sp->CheckSymbol("\xEF\xBB\xBF"))
  {
  	// skip UTF-8 byte order mark at the beginning
  }

  sp->SkipWhite();

  while (sp->readptr < sp->bufend)
  {
    if (!sp->ReadAlphaNum())
    {
    	errormsg = string("identifier missing, current char: ") + *sp->readptr;
    	return false;
    }

    string idstr(sp->prevptr, sp->prevlen);
    StringToUpper(idstr);

    sp->SkipWhite();

    if (!ParseConfigLine(idstr))
    {
      errormsg = string("Unknown identifier: ") + idstr;
      return false;
    }

    sp->SkipWhite();

  } // while reading section

  return true;
}

bool TPrgConfig::ParseConfigLine(string idstr)
{
  if ("UDOSL_DEVADDR" == idstr)
  {
  	udosl_devaddr = ParseStringAssignment();
  	if (error)  return false;
  }
  else
  {
  	return false;
  }

  return true;
}

double TPrgConfig::ParseNumAssignment()
{
  double result = 0;
  sp->SkipWhite();
  if (!sp->CheckSymbol("="))
  {
    error = true;
    errormsg = "= is missing";
    return 0;
  }

  result = ParseNumValue();
  if (!error)
  {
    SkipSemiColon();
  }

  return result;
}

string TPrgConfig::ParseStringAssignment()
{
  string result;
  sp->SkipWhite();
  if (!sp->CheckSymbol("="))
  {
    error = true;
    errormsg = "= is missing";
    return 0;
  }

  result = ParseStringValue();
  if (!error)
  {
    SkipSemiColon();
  }

  return result;
}

double TPrgConfig::ParseNumValue()
{
	sp->SkipWhite();
	if (!sp->ReadFloatNum())
	{
		return 0;
	}
	else
	{
		return sp->PrevToFloat();
	}
}


bool TPrgConfig::ParseBoolAssignment()
{
  bool result = false;
  sp->SkipWhite();
  if (!sp->CheckSymbol("="))
  {
    error = true;
    errormsg = "= is missing";
    return 0;
  }

  sp->SkipWhite();

  result = ParseBoolValue();
  if (!error)
  {
    SkipSemiColon();
  }

  return result;
}

bool TPrgConfig::ParseBoolValue()
{
	string vstr;
  bool result = false;

	if (!sp->ReadAlphaNum())
	{
		error = true;
		errormsg = "Error reading bool value";
		return result;
	}
	vstr.assign(sp->prevptr, sp->prevlen);
	StringToUpper(vstr);

	if (("1" == vstr) || ("TRUE" == vstr) || ("T" == vstr) || ("Y" == vstr) || ("YES" == vstr))
	{
		result = true;
	}

  return result;
}

string TPrgConfig::ParseStringValue()
{
  string result = "";

  while (true)
  {
  	sp->SkipWhite();

		if (sp->CheckSymbol("\""))
		{
			// string constant
			if (!sp->ReadTo("\""))
			{
				error = true;
				errormsg = "End of string not found.";
				return result;
			}
			result = result + sp->PrevStr();
			sp->CheckSymbol("\""); // skip closing "
		}
		else
		{
			error = true;
			errormsg = string("String constant expected.");
			return "";
		}

		sp->SkipSpaces();
		if (!sp->CheckSymbol("+"))
		{
			break;
		}
  }

	return result;
}

string TPrgConfig::ParseIdentifier()
{
  if (sp->CheckSymbol("@"))
  {
  	return ParseStringValue();
  }

	string vstr;
  string result = "";

  bool isquoted = sp->CheckSymbol("\"");

  if (isquoted)
  {
		if (!sp->ReadTo("\""))
		{
			error = true;
			errormsg = "End of string not found.";
			return result;
		}
		result = sp->PrevStr();

		sp->CheckSymbol("\""); // skip closing "
  }
  else
  {
  	if (!sp->ReadAlphaNum())
  	{
			error = true;
			errormsg = "Error reading identifier.";
			return result;
  	}
		result = sp->PrevStr();
  }

	return result;
}

void TPrgConfig::SkipSemiColon()
{
  sp->SkipWhite();
  if (sp->CheckSymbol(";"))
  {
    sp->SkipWhite();
  }
}

