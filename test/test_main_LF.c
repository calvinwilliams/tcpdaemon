#include "tcpdaemon.h"

extern int tcpmain( struct TcpdaemonServerEnvirment *p_env , int sock , void *p_addr );

int main()
{
	struct TcpdaemonEntryParameter	ep ;
	
	memset( & ep , 0x00 , sizeof(struct TcpdaemonEntryParameter) );
	
	ep.daemon_level = 1 ;
	
	snprintf( ep.log_pathfilename , sizeof(ep.log_pathfilename) , "%s/log/test_main_LF.log" , getenv("HOME") );
	ep.log_level = LOGLEVEL_DEBUG ;
	
	strcpy( ep.server_model , "LF" );
	strcpy( ep.ip , "0" );
	ep.port = 9527 ;
	
	ep.process_count = 10 ;
	
	strcpy( ep.so_pathfilename , "test_callback_http_echo.so" );
	ep.param_tcpmain = NULL ;
	
	return -tcpdaemon( & ep );
}

