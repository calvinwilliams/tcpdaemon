#include "tcpdaemon.h"

#define COMM_HEAD_LEN		4

_WINDLL_FUNC int tcpmain( struct TcpdaemonServerEnvironment *p_env , int sock , void *p_addr )
{
	char	comm_buffer[ 9999 + 1 ] ;
	int	received_len ;
	int	receiving_body_len ;
	int	sent_len ;
	int	sending_len ;
	int	len ;
	
	/* 接收通讯请求 */
	memset( comm_buffer , 0x00 , sizeof(comm_buffer) );
	received_len = 0 ;
	receiving_body_len = 0 ;
	while( receiving_body_len == 0 || ( receiving_body_len > 0 && received_len - COMM_HEAD_LEN < receiving_body_len ) )
	{
		if( receiving_body_len == 0 )
			len = recv( sock , comm_buffer + received_len , COMM_HEAD_LEN - received_len , 0 ) ;
		else
			len = recv( sock , comm_buffer + received_len , COMM_HEAD_LEN + receiving_body_len - received_len , 0 ) ;
		if( len == 0 )
			return TCPMAIN_RETURN_CLOSE;
		else if( len == -1 )
			return TCPMAIN_RETURN_ERROR;
		
		received_len += len ;
		
		if( receiving_body_len == 0 && received_len >= COMM_HEAD_LEN )
			receiving_body_len = (comm_buffer[0]-'0')*1000 + (comm_buffer[1]-'0')*100 + (comm_buffer[2]-'0')*10 + (comm_buffer[3]-'0') ;
	}
	
	/* printf( "RECV[%.*s]\n" , received_len , comm_buffer ); */
	
	/* 发送通讯响应 */
	sending_len = received_len ;
	sent_len = 0 ;
	while( sent_len < sending_len )
	{
		len = send( sock , comm_buffer + sent_len , sending_len - sent_len , 0 ) ;
		if( len == -1 )
			return TCPMAIN_RETURN_ERROR;
		
		sent_len += len ;
	}
	
	return TCPMAIN_RETURN_CLOSE;
}

