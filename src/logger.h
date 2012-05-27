/*
   Mago Blanco - "You Shall Not Pass"
   Alejandro Santos - alejolp@alejolp.com.ar

Copyright (c) 2012 Alejandro Santos

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
 */

#include <sstream>
#include <string>
#include <fstream>

#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#ifndef LOGGER_H_
#define LOGGER_H_

namespace magoblanco {

class logger {
public:
	logger();
	virtual ~logger();

	void open(const std::string& file_name);
	void close();

	inline void log(const std::string& s) {
		boost::mutex::scoped_lock l(m_);
		boost::posix_time::ptime tact(boost::posix_time::second_clock::local_time());

		log_file_ << tact << " - " << s << std::endl;
	}

	inline void log(const std::stringstream& s) {
		boost::mutex::scoped_lock l(m_);
		boost::posix_time::ptime tact(boost::posix_time::second_clock::local_time());

		log_file_ << tact << " - "<< s.str() << std::endl;
	}
private:
	std::string file_name_;
	std::ofstream log_file_;
	boost::mutex m_;
};

}

#endif /* LOGGER_H_ */
