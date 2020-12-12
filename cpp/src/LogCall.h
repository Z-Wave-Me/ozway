#ifndef OZWAY_LOG_CALL_H
#define OZWAY_LOG_CALL_H

#include <string>

class LogCall
{
public:
	LogCall(const char *func, const char *file, int line);
	~LogCall();
private:
	static int depth;
	std::string func;
	std::string file;
};

#define LOG_CALL LogCall lc(__FUNCTION__, __FILE__, __LINE__);

#endif
