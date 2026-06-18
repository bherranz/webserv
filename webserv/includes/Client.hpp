#pragma once

#include <string>
#include <ctime>

class Client
{
	public:
		explicit Client(int fd = -1);

		int fd() const;
		void closeFd();

		std::string &inBuffer();
		std::string &outBuffer();

		void updateActivity();
		time_t lastActivity() const;
		bool isTimedOut(time_t timeoutSec) const;

		void setKeepAlive(bool val);
		bool keepAlive() const;
		void clearBuffers();

	private:
		int _fd;
		std::string _in;
		std::string _out;
		time_t _lastActivity;
		bool _keepAlive;
};
