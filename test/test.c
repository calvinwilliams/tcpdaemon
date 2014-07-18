#include "tcpdaemon.h"

/* 函数调用模式测试程序 */

func_tcpmain tcpmain ;
int tcpmain( void *param_tcpmain , int sock , struct sockaddr *addr )
{
	char	http_buffer[ 4096 + 1 ] ;
	long	http_len ;
	long	len ;
	
	memset( http_buffer , 0x00 , sizeof(http_buffer) );
	http_len = 0 ;
	
	while( sizeof(http_buffer)-1 - http_len > 0 )
	{
		len = RECV( sock , http_buffer + http_len , sizeof(http_buffer)-1 - http_len , 0 ) ;
		if( len == -1 || len == 0 )
			return 0;
		if( strstr( http_buffer , "\r\n\r\n" ) )
			break;
		http_len += len ;
	}
	if( sizeof(http_buffer)-1 - http_len <= 0 )
	{
		return -1;
	}
	
	memset( http_buffer , 0x00 , sizeof(http_buffer) );
	http_len = 0 ;
	
	http_len = sprintf( http_buffer , "HTTP/1.0 200 OK\r\n\r\n" ) ;
	SEND( sock , http_buffer , http_len , 0 );
	
	return 0;
}

int main()
{
	struct TcpdaemonEntryParam	ep ;
	
	memset( & ep , 0x00 , sizeof(struct TcpdaemonEntryParam) );
	ep.call_mode = TCPDAEMON_CALLMODE_MAIN ;
	/* strcpy( ep.server_model , "LF" ); */
	strcpy( ep.server_model , "WIN-TLF" );
	ep.max_process_count = 10 ;
	strcpy( ep.ip , "0" );
	ep.port = 7879 ;
	/* strcpy( ep.so_pathfilename , "testso.so" ); */
	strcpy( ep.so_pathfilename , "test\\Debug\\testso.dll" );
	ep.pfunc_tcpmain = & tcpmain ;
	ep.param_tcpmain = NULL ;
	ep.log_level = LOGLEVEL_INFO ;
	
#if ( defined _WIN32 )
	{
	WSADATA		wsd;
	if( WSAStartup( MAKEWORD(2,2) , & wsd ) != 0 )
		return 1;
	}
#endif
	
	return -tcpdaemon( & ep );
}

