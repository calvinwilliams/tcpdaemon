#include "inc.h"

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams.c@gmail.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

/* 日志文件名 */
__thread char	g_log_pathfilename[ MAXLEN_FILENAME + 1 ] = "" ;
__thread int	g_log_level = LOGLEVEL_INFO ;

const char log_level_itoa[][6] = { "DEBUG" , "INFO" , "WARN" , "ERROR" , "FATAL" } ;

/* 设置日志文件名 */
void SetLogFile( char *format , ... )
{
	va_list		valist ;
	
	va_start( valist , format );
	vsnprintf( g_log_pathfilename , sizeof(g_log_pathfilename)-1 , format , valist );
	va_end( valist );
	
	return;
}

/* 设置日志等级 */
void SetLogLevel( int log_level )
{
	g_log_level = log_level ;
	
	return;
}

/* 输出日志 */
int OutputLog( int log_level , char *c_filename , long c_fileline , char *format , va_list valist )
{
	char		log_buffer[ 4096 + 1 ] ;
	
	if( g_log_pathfilename[0] == '\0' )
	{
		memset( log_buffer , 0x00 , sizeof(log_buffer) );
		vsnprintf( log_buffer , sizeof(log_buffer)-1 , format , valist );
		
		printf( "%s" , log_buffer );
	}
	else
	{
		struct timeval	tv ;
		size_t		len ;
		int		fd ;
		
		memset( log_buffer , 0x00 , sizeof(log_buffer) );
		gettimeofday( & tv , NULL );
		len = strftime( log_buffer , sizeof(log_buffer) , "%Y-%m-%d %H:%M:%S" , localtime(&(tv.tv_sec)) ) ;
		len += snprintf( log_buffer+len , sizeof(log_buffer)-1-len , ".%06ld" , (long)(tv.tv_usec) ) ;
		len += snprintf( log_buffer+len , sizeof(log_buffer)-1-len , " | %s" , log_level_itoa[log_level] ) ;
		len += snprintf( log_buffer+len , sizeof(log_buffer)-1-len , " | %ld:%s:%ld | ", (long)getpid() , c_filename , c_fileline ) ;
		len += vsnprintf( log_buffer+len , sizeof(log_buffer)-1-len , format , valist );
		
		fd = open( g_log_pathfilename , O_CREAT | O_WRONLY | O_APPEND , S_IRWXU | S_IRWXG | S_IRWXO ) ;
		if( fd == -1 )
			return -1;
		
		write( fd , log_buffer , len );
		
		close( fd );
	}
	
	return 0;
}

int FatalLog( char *c_filename , long c_fileline , char *format , ... )
{
	va_list		valist ;
	
	if( LOGLEVEL_FATAL < g_log_level )
		return 0;
	
	va_start( valist , format );
	OutputLog( LOGLEVEL_FATAL , c_filename , c_fileline , format , valist );
	va_end( valist );
	
	return 0;
}

int ErrorLog( char *c_filename , long c_fileline , char *format , ... )
{
	va_list		valist ;
	
	if( LOGLEVEL_ERROR < g_log_level )
		return 0;
	
	va_start( valist , format );
	OutputLog( LOGLEVEL_ERROR , c_filename , c_fileline , format , valist );
	va_end( valist );
	
	return 0;
}

int WarnLog( char *c_filename , long c_fileline , char *format , ... )
{
	va_list		valist ;
	
	if( LOGLEVEL_WARN < g_log_level )
		return 0;
	
	va_start( valist , format );
	OutputLog( LOGLEVEL_WARN , c_filename , c_fileline , format , valist );
	va_end( valist );
	
	return 0;
}

int InfoLog( char *c_filename , long c_fileline , char *format , ... )
{
	va_list		valist ;
	
	if( LOGLEVEL_INFO < g_log_level )
		return 0;
	
	va_start( valist , format );
	OutputLog( LOGLEVEL_INFO , c_filename , c_fileline , format , valist );
	va_end( valist );
	
	return 0;
}

int DebugLog( char *c_filename , long c_fileline , char *format , ... )
{
	va_list		valist ;
	
	if( LOGLEVEL_DEBUG < g_log_level )
		return 0;
	
	va_start( valist , format );
	OutputLog( LOGLEVEL_DEBUG , c_filename , c_fileline , format , valist );
	va_end( valist );
	
	return 0;
}
