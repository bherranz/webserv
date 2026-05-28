#pragma once

#include <string>

class Client
{
	public:
		explicit Client(int fd = -1);

		int fd() const;
		void closeFd();

		std::string &inBuffer();
		std::string &outBuffer();

	private:
		int _fd;
		std::string _in;
		std::string _out;
};
