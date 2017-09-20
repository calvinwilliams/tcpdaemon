#include "tcpdaemon.h"

#include "IDL_hello_world.dsc.h"

#define DEBUG			0

#define COMM_HEAD_LEN		4

_WINDLL_FUNC int tcpmain( struct TcpdaemonServerEnvironment *p_env , int sock , void *p_addr )
{
	char		comm_buffer[ 9999 + 1 ] ;
	hello_world	st ;
	int		unpack_len ;
	int		recv_len ;
	int		send_len ;
	struct timeval	timeout ;
	
	int		nret = 0 ;
	
	/* 接收通讯请求 */
	memset( comm_buffer , 0x00 , sizeof(comm_buffer) );
	recv_len = sizeof(comm_buffer) ;
	timeout.tv_sec = 60 ;
	timeout.tv_usec = 0 ;
	nret = TDHBReceiveData( sock , COMM_HEAD_LEN , comm_buffer , & recv_len , & timeout ) ;
	if( nret )
		return TCPMAIN_RETURN_ERROR;
	
	/* 反序列化报文 */
	unpack_len = 0 ;
	memset( & st , 0x00 , sizeof(hello_world) );
	nret = DSCDESERIALIZE_COMPRESS_hello_world( comm_buffer , & unpack_len , & st ) ;
	if( nret )
		return TCPMAIN_RETURN_ERROR;
	
	/* 序列化报文 */
	memset( comm_buffer , 0x00 , sizeof(comm_buffer) );
	send_len = sizeof(comm_buffer) ;
	nret = DSCSERIALIZE_COMPRESS_hello_world( & st , comm_buffer , & send_len ) ;
	if( nret )
		return TCPMAIN_RETURN_ERROR;
	
	/* 发送通讯响应 */
	nret = TDHBSendData( sock , COMM_HEAD_LEN , comm_buffer , & send_len , & timeout ) ;
	if( nret )
		return TCPMAIN_RETURN_ERROR;
	
	return TCPMAIN_RETURN_CLOSE;
}

