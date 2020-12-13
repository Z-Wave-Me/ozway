//-----------------------------------------------------------------------------
//
//      ZWayHelper.h
//
//      Z-Way logging helpers
//
//      Copyright (c) 2020 Z-Wave.Me <z-way@z-wave.me>
//
//      SOFTWARE NOTICE AND LICENSE
//
//      This file is part of OpenZWave.
//
//      OpenZWave is free software: you can redistribute it and/or modify
//      it under the terms of the GNU Lesser General Public License as published
//      by the Free Software Foundation, either version 3 of the License,
//      or (at your option) any later version.
//
//      OpenZWave is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU Lesser General Public License for more details.
//
//      You should have received a copy of the GNU Lesser General Public License
//      along with OpenZWave.  If not, see <http://www.gnu.org/licenses/>.
//
//-----------------------------------------------------------------------------

#ifndef _ZWayHelper_H
#define _ZWayHelper_H

#define _NOT_YET_IMPLEMENTED_ printf("The method \"%s\" is not yet implemented, in %s, line %d\n", __func__,  __FILE__, __LINE__);
//#define _NOT_YET_IMPLEMENTED_(zway) zway_log((zway), Error, "The method \"%s\" is not yet implemented, in %s, line %d\n", __func__,  __FILE__, __LINE__)
#define _NOT_SUPPORTED_(zway) zway_log((zway), Error, "The method \"%s\" is not supported, in %s, line %d\n", __func__,  __FILE__, __LINE__)

#define FINISH_ME TODO(OZWay: Finish this part of code)

#ifdef DEBUG

void zway_debug_log_error(const ZWay zway, ZWError err, ZWError opt, const char *func, const char *file, int line);
#define LOG_ERR(r)      zway_debug_log_error(zway, r, NoError, #r, __FILE__, __LINE__)
#define LOG_ERR_OPT(r)  zway_debug_log_error(zway, r, NotSupported, #r, __FILE__, __LINE__)

#else

void zway_debug_log_error(const ZWay zway, ZWError err, ZWError opt, const char *func);
#define LOG_ERR(r)      zway_debug_log_error(zway, r, NoError, #r)
#define LOG_ERR_OPT(r)  zway_debug_log_error(zway, r, NotSupported, #r)

#endif // DEBUG

#endif // _ZWayHelper_H