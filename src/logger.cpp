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

#include "logger.h"

namespace magoblanco {

logger::logger()
{
	log_file_.exceptions(std::ios_base::badbit | std::ios_base::failbit);
}

logger::~logger()
{

}

void logger::open(const std::string& file_name, std::size_t rotation)
{
	boost::posix_time::ptime tact(boost::posix_time::second_clock::local_time());
	std::string fn2 = file_name;

	if (rotation)
		fn2 += "-" + boost::posix_time::to_iso_string(tact);

	file_name_ = file_name;
	rotation_ = rotation;
	last_rotation_ = tact;
	log_file_.open(fn2.c_str(), std::ios_base::app | std::ios_base::ate);
}

void logger::check_rotation(const boost::posix_time::ptime& tact)
{
	if (rotation_ && (tact - last_rotation_) > boost::posix_time::minutes(rotation_)) {
		log_file_.close();
		open(file_name_, rotation_);
	}
}

void logger::close()
{
	log_file_.close();
}

}
