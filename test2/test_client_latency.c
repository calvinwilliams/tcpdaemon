#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "IDL_hello_world.dsc.h"

#include "tcpdaemon.h"

#define COMM_HEAD_LEN		4

static int test_client_latency( char *ip , int port )
{
	hello_world		st ;
	int			send_len ;
	int			recv_len ;
	int			unpack_len ;
	char			comm_buffer[ 9999 + 1 ] ;
	struct timeval		timeout ;
	
	struct timeval		begin_timestamp ;
	struct timeval		end_timestamp ;
	long			diff_us ;
	
	int			nret = 0 ;
	
	while(1)
	{
		/* 得到开始时间戳 */
		gettimeofday( & begin_timestamp , NULL );
		
		/* 组织报文 */
		memset( & st , 0x00 , sizeof(hello_world) );
		strcpy( st.message , "hello world" );
		
		/* 序列化报文 */
		memset( comm_buffer , 0x00 , sizeof(comm_buffer) );
		send_len = sizeof(comm_buffer) ;
		nret = DSCSERIALIZE_COMPRESS_hello_world( & st , comm_buffer , & send_len ) ;
		if( nret )
		{
			printf( "DSCSERIALIZE_COMPRESS_hello_world failed[%d]\n" , nret );
			return -1;
		}
		
		/* 连接服务端、发送接收数据、断开服务端 */
		recv_len = sizeof(comm_buffer) ;
		timeout.tv_sec = 60 ;
		timeout.tv_usec = 0 ;
		nret = TDHBCall( ip , port , COMM_HEAD_LEN , comm_buffer , & send_len , & recv_len , & timeout ) ;
		if( nret )
		{
			printf( "TDHBCall failed[%d]\n" , nret );
			return -1;
		}
		
		/* 反序列化报文 */
		comm_buffer[recv_len] = '\0' ;
		unpack_len = 0 ;
		memset( & st , 0x00 , sizeof(hello_world) );
		nret = DSCDESERIALIZE_COMPRESS_hello_world( comm_buffer , & unpack_len , & st ) ;
		if( nret )
		{
			printf( "DSCDESERIALIZE_COMPRESS_hello_world failed[%d]\n" , nret );
			return -1;
		}
		
		/* 得到开始时间戳 */
		gettimeofday( & end_timestamp , NULL );
		
		/* 计算时间差 */
		diff_us = (end_timestamp.tv_sec-begin_timestamp.tv_sec)*1000000 + (end_timestamp.tv_usec-begin_timestamp.tv_usec) ;
		
		/* 输出统计信息 */
		printf( "message[%s] latency[%ld]us\n" , st.message , diff_us );
		
		sleep(1);
	}
	
	return 0;
}

static void usage()
{
	printf( "USAGE : test_client_latency ip port\n" );
	return;
}

int main( int argc , char *argv[] )
{
	setbuf( stdout , NULL );
	
	if( argc != 1 + 2 )
	{
		usage();
		exit(7);
	}
	
	return -test_client_latency( argv[1] , atoi(argv[2]) ) ;
}

