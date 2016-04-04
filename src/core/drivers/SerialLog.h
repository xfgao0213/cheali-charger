/*
    cheali-charger - open source firmware for a variety of LiPo chargers
    Copyright (C) 2013  Paweł Stawicki. All right reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef SERIALLOG_H_
#define SERIALLOG_H_

#include <stdint.h>
#include "memory.h"

namespace SerialLog {

#ifdef ENABLE_SERIAL_LOG
    void powerOn();
    void doIdle();
    void powerOff();
    void flush();

    void printString(const char *s);
    void printString_P(const_char_ptr s);
    void printLong(int32_t x);
    void printNL();
    void printChar(char c);
#else
    inline void powerOn() {}
    inline void doIdle() {}
    inline void powerOff() {}
    inline void flush() {}

    inline void printString(const char *s) {}
    inline void printString_P(const_char_ptr s) {}
    inline void printLong(int32_t x) {}
    inline void printNL() {}
    inline void printChar(char c) {}
#endif


    inline void printInt(int16_t x)     { return printLong(x);  }
    inline void printUInt(uint16_t x)   { return printLong(x);  }


} //namespace SerialLog


#endif /* SERIALLOG_H_ */
