#include "tcpdaemon.h"

_WINDLL_FUNC int tcpmain( struct TcpdaemonServerEnvirment *p_env , int sock , void *p_addr )
{
	char	http_buffer[ 4096 + 1 ] ;
	long	http_len ;
	long	len ;
	
	/* 接收HTTP请求 */
	memset( http_buffer , 0x00 , sizeof(http_buffer) );
	http_len = 0 ;
	while( sizeof(http_buffer)-1 - http_len > 0 )
	{
		len = RECV( sock , http_buffer + http_len , sizeof(http_buffer)-1 - http_len , 0 ) ;
		if( len == 0 )
			return TCPMAIN_RETURN_CLOSE;
		if( len == -1 )
			return TCPMAIN_RETURN_ERROR;
		if( strstr( http_buffer , "\r\n\r\n" ) )
			break;
		http_len += len ;
	}
	if( sizeof(http_buffer)-1 - http_len <= 0 )
	{
		return TCPMAIN_RETURN_ERROR;
	}
	
	/* 发送HTTP响应 */
	memset( http_buffer , 0x00 , sizeof(http_buffer) );
	http_len = 0 ;
	http_len = sprintf( http_buffer , "HTTP/1.0 200 OK\r\nContent-length: 17\r\n\r\nHello Tcpdaemon\r\n" ) ;
	SEND( sock , http_buffer , http_len , 0 );
	
	return TCPMAIN_RETURN_CLOSE;
}

