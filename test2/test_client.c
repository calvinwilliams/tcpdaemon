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
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "IDL_hello_world.dsc.h"

#include "tcpdaemon.h"

#define COMM_HEAD_LEN		4

static int requester( char *ip , int port , int processing_count )
{
	int			sock ;
	int			i ;
	
	hello_world		st ;
	int			send_len ;
	int			recv_len ;
	int			unpack_len ;
	char			comm_buffer[ 9999 + 1 ] ;
	struct timeval		timeout ;
	
	int			nret = 0 ;
	
	/* 连接服务端 */
	sock = TDHBConnect( ip , port ) ;
	if( sock < 0 )
	{
		printf( "TDHBConnect failed[%d]\n" , sock );
		return -1;
	}
	
	for( i = 0 ; i < processing_count ; i++ )
	{
		/* 组织报文 */
		memset( & st , 0x00 , sizeof(hello_world) );
		strcpy( st.message , "hello world" );
		
		if( i == 0 )
			printf( "[%d] REQUEST : [%s]\n" , i , st.message );
		
		/* 序列化报文 */
		memset( comm_buffer , 0x00 , sizeof(comm_buffer) );
		send_len = sizeof(comm_buffer) ;
		nret = DSCSERIALIZE_COMPRESS_hello_world( & st , comm_buffer , & send_len ) ;
		if( nret )
		{
			printf( "DSCSERIALIZE_COMPRESS_hello_world failed[%d]\n" , nret );
			return -1;
		}
		
		/* 发送接收数据 */
		recv_len = sizeof(comm_buffer) ;
		timeout.tv_sec = 60 ;
		timeout.tv_usec = 0 ;
		nret = TDHBSendAndReceiveData( sock , COMM_HEAD_LEN , comm_buffer , & send_len , & recv_len , & timeout ) ;
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
		
		if( i == 0 )
			printf( "[%d]RESPONSE : [%s]\n" , i , st.message );
	}
	
	/* 断开服务端 */
	TDHBDisconnect( sock );
	
	return 0;
}

static int test_client( char *ip , int port , int processor_count , int processing_count )
{
	int		i ;
	pid_t		pid ;
	int		status ;
	
	signal( SIGCLD , SIG_DFL );
	signal( SIGCHLD , SIG_DFL );
	
	for( i = 0 ; i < processor_count ; i++ )
	{
		pid = fork() ;
		if( pid == -1 )
		{
			printf( "fork failed , errno[%d]\n" , errno );
			return -1;
		}
		else if( pid == 0 )
		{
			printf( "fork ok , pid[%d]\n" , getpid() );
			exit(-requester(ip,port,processing_count));
		}
	}
	
	for( i = 0 ; i < processor_count ; i++ )
	{
		pid = waitpid( -1 , & status , 0 ) ;
		if( pid == -1 )
		{
			printf( "waitpid failed , errno[%d]\n" , errno );
			return -1;
		}
	}
	
	return 0;
}

static void usage()
{
	printf( "USAGE : test_client ip port processor_count processing_count\n" );
	return;
}

int main( int argc , char *argv[] )
{
	if( argc != 1 + 4 )
	{
		usage();
		exit(7);
	}
	
	setbuf( stdout , NULL );
	
	return -test_client( argv[1] , atoi(argv[2]) , atoi(argv[3]) , atoi(argv[4]) );
}

