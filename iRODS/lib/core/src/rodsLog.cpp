/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/

//Creates irods_error_map
#define MAKE_IRODS_ERROR_MAP
#include "rodsErrorTable.h"
const static std::map<const int, const std::string> irods_error_map = irods_error_map_construction::irods_error_map;

#include "rods.h"

#ifdef SYSLOG
#ifndef windows_platform
#include <syslog.h>
#endif
#endif

#include "rodsLog.h"
#include "rcGlobalExtern.h"
#include "rcMisc.h"
#include <time.h>
#include <map>
#include <string>
#include <sys/time.h>

#ifndef windows_platform
#include <unistd.h>
#endif

#ifdef windows_platform
#include "irodsntutil.hpp"
#endif

#define BIG_STRING_LEN MAX_NAME_LEN+300
#include <stdarg.h>



static int verbosityLevel = LOG_ERROR;
static int sqlVerbosityLevel = 0;
pid_t myPid = 0;

#ifdef windows_platform
static void rodsNtElog( char *msg );
#endif

/*
 Log or display a message.  The level argument indicates how severe
 the message is, and depending on the verbosityLevel may or may not be
 recorded.  This is used by both client and server code.
 */
void
rodsLog( int level, const char *formatStr, ... ) {
    char *prefix;
    time_t timeValue;
    FILE *errOrOut;
    va_list ap;

#ifdef SYSLOG
    char *myZone = getenv( "spProxyRodsZone" );
#endif
    int okToLog = 0;

    char extraInfo[100];
#ifdef windows_platform
    char nt_log_msg[2048];
#endif

    if ( level <= verbosityLevel ) {
        okToLog = 1;
    }

    if ( level == LOG_SQL ) {
        okToLog = 1;
    }

    if ( !okToLog ) {
        return;
    }

    va_start( ap, formatStr );
    char * message = ( char * )malloc( sizeof( char ) * BIG_STRING_LEN );
    int len = vsnprintf( message, BIG_STRING_LEN, formatStr, ap );
    if ( len + 1 > BIG_STRING_LEN ) {
        va_end( ap );
        va_start( ap, formatStr );
        free( message );
        message = ( char * )malloc( sizeof( char ) * ( len + 1 ) );
        vsnprintf( message, len + 1, formatStr, ap );
    }
    va_end( ap );

    extraInfo[0] = '\0';
#ifndef windows_platform
    errOrOut = stdout;
#endif
    if ( ProcessType == SERVER_PT || ProcessType == AGENT_PT ||
            ProcessType == RE_SERVER_PT ) {
        char timeBuf[100];
        time( &timeValue );
        rstrcpy( timeBuf, ctime( &timeValue ), 90 );
        timeBuf[19] = '\0';
        myPid = getpid();
        snprintf( extraInfo, 100 - 1, "%s pid:%d ", timeBuf + 4, myPid );
    }
    else {
#ifndef windows_platform
        if ( level <= LOG_ERROR || level == LOG_SQL ) {
            errOrOut = stderr;
        }
#endif
    }

    prefix = "";
    if ( level == LOG_SQL ) {
        prefix = "LOG_SQL";
    }
    if ( level == LOG_SYS_FATAL ) {
        prefix = "SYSTEM FATAL";
    }
    if ( level == LOG_SYS_WARNING ) {
        prefix = "SYSTEM WARNING";
    }
    if ( level == LOG_ERROR ) {
        prefix = "ERROR";
    }
    if ( level == LOG_NOTICE ) {
        prefix = "NOTICE";
    }
#ifdef SYSLOG
    if ( level == LOG_DEBUG ) {
        prefix = "DEBUG";
    }
    if ( level == LOG_DEBUG1 ) {
        prefix = "DEBUG1";
    }
    if ( level == LOG_DEBUG2 ) {
        prefix = "DEBUG2";
    }
    if ( level == LOG_DEBUG3 ) {
        prefix = "DEBUG3";
    }
    if ( ProcessType == SERVER_PT || ProcessType == AGENT_PT ||
            ProcessType == RE_SERVER_PT )
#else
    if ( level == LOG_DEBUG ) {
        prefix = "DEBUG";
    }
    if ( level == LOG_DEBUG1 ) {
        prefix = "DEBUG1";
    }
    if ( level == LOG_DEBUG2 ) {
        prefix = "DEBUG2";
    }
    if ( level == LOG_DEBUG3 ) {
        prefix = "DEBUG3";
    }
    if ( message[strlen( message ) - 1] == '\n' )
#endif
    {
#ifdef SYSLOG
#ifdef SYSLOG_FACILITY_CODE
        syslog( SYSLOG_FACILITY_CODE | LOG_NOTICE, "%s - %s: %s", myZone, prefix, message );
#else
        syslog( LOG_DAEMON | LOG_NOTICE, "%s - %s: %s", myZone, prefix, message );
#endif
#else
#ifndef windows_platform
        fprintf( errOrOut, "%s%s: %s", extraInfo, prefix, message );
#else
        sprintf( nt_log_msg, "%s%s: %s", extraInfo, prefix, message );
        rodsNtElog( nt_log_msg );
#endif
#endif
    }
    else {
#ifndef windows_platform
        fprintf( errOrOut, "%s%s: %s\n", extraInfo, prefix, message );
#else
        sprintf( nt_log_msg, "%s%s: %s\n", extraInfo, prefix, message );
        rodsNtElog( nt_log_msg );
#endif
    }
#ifndef windows_platform
    fflush( errOrOut );
#endif
    free( message );
}

/* same as rodsLog plus putting the msg in myError too. Need to merge with
 * rodsLog
 */

void
rodsLogAndErrorMsg( int level, rError_t *myError, int status,
                    const char *formatStr, ... ) {
    char *prefix;
    time_t timeValue;
    FILE *errOrOut;
    va_list ap;
    char errMsg[ERR_MSG_LEN];

    char extraInfo[100];
#ifdef windows_platform
    char nt_log_msg[2048];
#endif

    if ( level > verbosityLevel ) {
        return;
    }


    va_start( ap, formatStr );
    char * message = ( char * )malloc( sizeof( char ) * BIG_STRING_LEN );
    int len = vsnprintf( message, BIG_STRING_LEN, formatStr, ap );
    if ( len + 1 > BIG_STRING_LEN ) {
        va_end( ap );
        va_start( ap, formatStr );
        free( message );
        message = ( char * )malloc( sizeof( char ) * ( len + 1 ) );
        vsnprintf( message, len + 1, formatStr, ap );
    }
    va_end( ap );

    extraInfo[0] = '\0';
    errOrOut = stdout;
    if ( ProcessType == SERVER_PT || ProcessType == AGENT_PT ||
            ProcessType == RE_SERVER_PT ) {
        char timeBuf[100];
        time( &timeValue );
        rstrcpy( timeBuf, ctime( &timeValue ), 90 );
        timeBuf[19] = '\0';
        myPid = getpid();
        snprintf( extraInfo, 100 - 1, "%s pid:%d ", timeBuf + 4, myPid );
    }
    else {
        if ( level <= LOG_ERROR || level == LOG_SQL ) {
            errOrOut = stderr;
        }
    }

    prefix = "";
    if ( level == LOG_SQL ) {
        prefix = "LOG_SQL";
    }
    if ( level == LOG_SYS_FATAL ) {
        prefix = "SYSTEM FATAL";
    }
    if ( level == LOG_SYS_WARNING ) {
        prefix = "SYSTEM WARNING";
    }
    if ( level == LOG_ERROR ) {
        prefix = "ERROR";
    }
    if ( level == LOG_NOTICE ) {
        prefix = "NOTICE";
    }
    if ( level <= LOG_DEBUG ) {
        prefix = "DEBUG";
    }
    if ( message[strlen( message ) - 1] == '\n' ) {
#ifndef windows_platform
        fprintf( errOrOut, "%s%s: %s", extraInfo, prefix, message );
        if ( myError != NULL ) {
            snprintf( errMsg, ERR_MSG_LEN,
                      "%s: %s", prefix, message );
            addRErrorMsg( myError, status, errMsg );
        }
#else
        sprintf( nt_log_msg, "%s%s: %s", extraInfo, prefix, message );
        rodsNtElog( nt_log_msg );
#endif
    }
    else {
#ifndef windows_platform
        fprintf( errOrOut, "%s%s: %s\n", extraInfo, prefix, message );
        if ( myError != NULL ) {
            snprintf( errMsg, ERR_MSG_LEN,
                      "%s: %s\n", prefix, message );
            addRErrorMsg( myError, status, errMsg );
        }
#else
        sprintf( nt_log_msg, "%s%s: %s\n", extraInfo, prefix, message );
        rodsNtElog( nt_log_msg );
#endif
    }

#ifndef windows_platform
    fflush( errOrOut );
#endif
    free( message );
}

/*
 Change the verbosityLevel of reporting.
 The input value is the new minimum level of message to report.
 */
void
rodsLogLevel( int level ) {
    verbosityLevel = level;
}

int
getRodsLogLevel() {
    return ( verbosityLevel );
}

/*
 Request sql logging.
 */
void
rodsLogSqlReq( int onOrOff ) {
    sqlVerbosityLevel = onOrOff;
}

void
rodsLogSql( const char *sql ) {
    myPid = getpid();
    if ( sqlVerbosityLevel ) rodsLog( LOG_SQL, "pid: %d sql: %s",
                                          myPid, sql );
}
void
rodsLogSqlResult( char *stat ) {
    myPid = getpid();
    if ( sqlVerbosityLevel ) rodsLog( LOG_SQL, "pid: %d result: %s",
                                          myPid, stat );
}

/*
Convert an iRODS error code to the corresponding name.
 */
const char *
rodsErrorName( int errorValue, char **subName ) {
    int testVal = errorValue / 1000;

    if ( subName ) {
        int subCode = errorValue - ( testVal * 1000 );
        *subName = subCode && errorValue < 0 ?
            strdup( strerror( -subCode ) ) :
            strdup( "" );
    }

    const std::map<const int, const std::string>::const_iterator search = irods_error_map.find( testVal * 1000 );
    if ( search == irods_error_map.end() ) {
        return( "Unknown iRODS error" );
    }
    return search->second.c_str();
}

/*
 Convert an error code to a string and log it.
 This was originally called rodsLogError, but was renamed when we
 created the new rodsLogError below.  This is no longer used (
 rodsLogError can be called with the same arguments).
 */
void
rodsLogErrorOld( int level, int rodsErrorCode, char *textStr ) {
    char *errSubName = NULL;

    if ( level < verbosityLevel ) {
        return;
    }

    const char * errName = rodsErrorName( rodsErrorCode, &errSubName );
    if ( textStr && strlen( textStr ) > 0 ) {
        rodsLog( level, "%s Error: %d: %s, %s", textStr, rodsErrorCode,
                 errName, errSubName );
    }
    else {
        rodsLog( level, "Error: %d: %s, %s", rodsErrorCode,
                 errName, errSubName );
    }
    free( errSubName );
}

/* Like rodsLogError but with full rodsLog functionality too.
   Converts the errorcode to a string, and possibly a subcode string,
   and includes that at the end of a regular log message (with
   variable arguments).
 */
void
rodsLogError( int level, int rodsErrorCode, char *formatStr, ... ) {
    char *errSubName = NULL;
    va_list ap;

    if ( level > verbosityLevel ) {
        return;
    }


    va_start( ap, formatStr );
    char * message = ( char * )malloc( sizeof( char ) * BIG_STRING_LEN );
    int len = vsnprintf( message, BIG_STRING_LEN, formatStr, ap );
    if ( len + 1 > BIG_STRING_LEN ) {
        va_end( ap );
        va_start( ap, formatStr );
        free( message );
        message = ( char * )malloc( sizeof( char ) * ( len + 1 ) );
        vsnprintf( message, len + 1, formatStr, ap );
    }
    va_end( ap );

    const char * errName = rodsErrorName( rodsErrorCode, &errSubName );
    if ( strlen( errSubName ) > 0 ) {
        rodsLog( level, "%s status = %d %s, %s", message, rodsErrorCode,
                 errName, errSubName );
    }
    else {
        rodsLog( level, "%s status = %d %s", message, rodsErrorCode,
                 errName );
    }
    free( message );
    free( errSubName );
}

#ifdef windows_platform
static void rodsNtElog( char *msg ) {
    char log_fname[1024];
    int fd;
    int t;

    if ( ProcessType == CLIENT_PT ) {
        fprintf( stderr, "%s", msg );
        return;
    }

    t = strlen( msg );
    if ( msg[t - 1] == '\n' ) {
        msg[t - 1] = '\0';
        t = t - 1;
    }

    if ( iRODSNtServerRunningConsoleMode() ) {
        t = strlen( msg );
        if ( msg[t - 1] == '\n' ) {
            fprintf( stderr, "%s", msg );
        }
        else {
            fprintf( stderr, "%s\n", msg );
        }
        return;
    }

    t = strlen( msg );
    if ( msg[t - 1] != '\n' ) {
        msg[t] = '\n';
        msg[t + 1] = '\0';
        t = t + 1;
    }

    iRODSNtGetLogFilenameWithPath( log_fname );
    fd = iRODSNt_open( log_fname, O_APPEND | O_WRONLY, 1 );
    _write( fd, msg, t );
    _close( fd );
}
#endif

/*
 * This function will generate an ISO 8601 formatted
 * date/time stamp for use in log messages. The format
 * will be 'YYYYMMDDThhmmss.uuuuuuZ' where:
 *
 * YYYY - is the year
 *   MM - is the month (01-12)
 *   DD - is the day   (01-31)
 *   hh - is the hour (00-24)
 *   mm - is the minute (00-59)
 *   ss - is the second (00-59)
 *   u+ - are the number of microseconds.
 *
 * The date/time stamp is in UTC time.
 */
void
generateLogTimestamp( char *ts, int tsLen ) {
    struct timeval tv;
    struct tm utc;
    char timestamp[TIME_LEN];

    if ( ts == NULL ) {
        return;
    }

    gettimeofday( &tv, NULL );
    gmtime_r( &tv.tv_sec, &utc );
    strftime( timestamp, TIME_LEN, "%Y%m%dT%H%M%S", &utc );

    /* 8 characters of '.uuuuuuZ' + nul */
    if ( tsLen < ( int )strlen( timestamp ) + 9 ) {
        return;
    }

    snprintf( ts, strlen( timestamp ) + 9, "%s.%06dZ", timestamp, ( int )tv.tv_usec );
}


