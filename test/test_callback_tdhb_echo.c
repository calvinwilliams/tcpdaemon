#include "tcpdaemon.h"

#define PREHEAD_LEN		4

_WINDLL_FUNC int tcpmain( struct TcpdaemonServerEnvironment *p_env , int sock , void *p_addr )
{
	char		tdhb_buffer[ 9999 + 1 ] ;
	int		tdhb_len ;
	struct timeval	timeout ;
	int		nret = 0 ;
	
	/* 接收通讯请求 */
	memset( tdhb_buffer , 0x00 , sizeof(tdhb_buffer) );
	tdhb_len = sizeof(tdhb_buffer) ;
	timeout.tv_sec = 60 ;
	timeout.tv_usec = 0 ;
	nret = TDHBReceiveData( sock , PREHEAD_LEN , tdhb_buffer , & tdhb_len , & timeout ) ;
	if( nret == TDHB_ERROR_SOCKET_CLOSED )
		return TCPMAIN_RETURN_CLOSE;
	else if( nret )
		return TCPMAIN_RETURN_ERROR;
	
	/* 发送通讯响应 */
	nret = TDHBSendData( sock , PREHEAD_LEN , tdhb_buffer , & tdhb_len , & timeout ) ;
	if( nret )
		return TCPMAIN_RETURN_ERROR;
	
	return TCPMAIN_RETURN_CLOSE;
}

