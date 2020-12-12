#include <string>
#include <stdio.h>

#include "LogCall.h"

int LogCall::depth = 0;

LogCall::LogCall(const char *funcname, const char *filepath, int line)
{
	func = std::string(funcname);

	std::string full_path(filepath);
	file = full_path.substr(full_path.rfind('/', std::string::npos) + 1, std::string::npos);
	file = file.substr(0, file.length() - 4);

	std::string indent(depth * 2, ' ');
	printf("\t%s%s [%s : %d]\n", indent.c_str(), funcname, file.c_str(), line);

	depth += 1;
}

LogCall::~LogCall()
{
	depth -= 1;

	std::string indent(depth * 2, ' ');
	printf("\t%s~%s\n", indent.c_str(), func.c_str());
}
