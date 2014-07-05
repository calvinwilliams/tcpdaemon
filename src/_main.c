#include "inc.h"

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams.c@gmail.com
 * LastVersion : 2014-06-29	v1.0.0		创建
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

static void usage( char *program )
{
	printf( "USAGE : %s	-m LF -n process_count [ -r max_requests_per_process ] -l ip:port -s so_pathfilename\n" , program );
	printf( "        	-m IF [ -n max_process_count ] -l ip:port -s so_pathfilename\n" );
	printf( "        other options :\n" );
	printf( "        	[ --loglevel-(debug|info|warn|error|fatal) ] [ --tcp-nodelay ] [ --tcp-linger linger ]\n" );
}

static void version( char *program )
{
	printf( "%s v%s\n" , program , __version_tcpdaemon );
	printf( "Copyright by calvin<calvinwilliams.c@gmail.com> 2014\n" );
}

/* 主守护模式main函数 */

int _main( int argc , char *argv[] , struct TcpdaemonEntryParam *pep , func_tcpmain *pfunc_tcpmain , void *param_tcpmain )
{
	int				c ;
	
	int				nret = 0 ;
	
	if( argc == 1 )
	{
		usage( argv[0] );
		exit(0);
	}
	
	/* 填充入口参数结构 */
	pep->max_process_count = 0 ;
	pep->max_requests_per_process = 0 ;
	pep->log_level = LOGLEVEL_INFO ;
	pep->pfunc_tcpmain = pfunc_tcpmain ;
	pep->param_tcpmain = param_tcpmain ;
	
	/* 解析命令行参数 */
	for( c = 1 ; c < argc ; c++ )
	{
		if( STRCMP( argv[c] , == , "-v" ) )
		{
			version( argv[0] );
			usage( argv[0] );
			exit(0);
		}
		else if( STRCMP( argv[c] , == , "-m" ) && c + 1 < argc )
		{
			strncpy( pep->server_model , argv[c+1] , 2 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "-n" ) && c + 1 < argc )
		{
			pep->max_process_count = atol(argv[c+1]) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "-r" ) && c + 1 < argc )
		{
			pep->max_requests_per_process = atol(argv[c+1]) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "-l" ) && c + 1 < argc )
		{
			char	*p = NULL ;
			p = strchr( argv[c+1] , ':' ) ;
			if( p == NULL )
			{
				InfoLog( __FILE__ , __LINE__ , "parameter --listen[%s]无效\n" , argv[c+1] );
				return 1;
			}
			strncpy( pep->ip , argv[c+1] , p - argv[c+1] );
			pep->port = atol(p+1) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "-s" ) && c + 1 < argc )
		{
			strncpy( pep->so_pathfilename , argv[c+1] , sizeof(pep->so_pathfilename)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-fatal" ) )
		{
			pep->log_level = LOGLEVEL_FATAL ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-error" ) )
		{
			pep->log_level = LOGLEVEL_ERROR ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-warn" ) )
		{
			pep->log_level = LOGLEVEL_WARN ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-info" ) )
		{
			pep->log_level = LOGLEVEL_INFO ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-debug" ) )
		{
			pep->log_level = LOGLEVEL_DEBUG ;
		}
		else if( STRCMP( argv[c] , == , "--tcp-nodelay" ) )
		{
			pep->tcp_nodelay = 1 ;
		}
		else if( STRCMP( argv[c] , == , "--tcp-linger" ) && c + 1 < argc )
		{
			pep->tcp_linger = atoi(argv[c+1]) ;
			c++;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "parameter[%s] invalid\n" , argv[c] );
			usage( argv[0] );
			exit(1);
		}
	}
	
	/* 调用主入口函数 */
	nret = -tcpdaemon( pep ) ;
	
	return nret;
}

