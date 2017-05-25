#include "tcpdaemon_in.h"

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

void usage()
{
	printf( "USAGE : tcpdaemon -m LF -c process_count [ -n max_requests_per_process ] -l ip:port -s so_pathfilename\n" );
	printf( "                  -m IF [ -c max_process_count ] -l ip:port -s so_pathfilename\n" );
	printf( "        other options :\n" );
	printf( "                  -v\n" );
	printf( "                  [ --daemon-level ]\n" );
#if ( defined __linux__ ) || ( defined __unix )
	printf( "                  [ --work-user work_user ]\n" );
#endif
	printf( "                  [ --work-path work_path ]\n" );
	printf( "                  [ --logfile log_pathfilename --loglevel-(debug|info|warn|error|fatal) ]\n" );
	printf( "                  [ --tcp-nodelay ] [ --tcp-linger linger ]\n" );
#if ( defined _WIN32 )
	printf( "                  [ --install-winservice ] [ --uninstall-winservice ]\n" );
#endif
}

void version()
{
	printf( "tcpdaemon v%s\n" , __TCPDAEMON_VERSION );
	printf( "Copyright by calvin<calvinwilliams@163.com> 2014,2015,2016,2017\n" );
}

/* 解析命令行参数 */
int ParseCommandParameter( int argc , char *argv[] , struct TcpdaemonEntryParameter *p_para )
{
	int		c ;
	
	/* 解析命令行参数 */
	for( c = 1 ; c < argc ; c++ )
	{
		if( STRCMP( argv[c] , == , "-v" ) )
		{
			version();
			exit(0);
		}
		else if( STRCMP( argv[c] , == , "-m" ) && c + 1 < argc )
		{
			strncpy( p_para->server_model , argv[c+1] , sizeof(p_para->server_model)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "-c" ) && c + 1 < argc )
		{
			p_para->max_process_count = atol(argv[c+1]) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "-n" ) && c + 1 < argc )
		{
			p_para->max_requests_per_process = atol(argv[c+1]) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "-l" ) && c + 1 < argc )
		{
			char	*p = NULL ;
			p = strchr( argv[c+1] , ':' ) ;
			if( p == NULL )
			{
				InfoLog( __FILE__ , __LINE__ , "parameter --listen[%s]无效" , argv[c+1] );
				return 1;
			}
			if( p - argv[c+1] > sizeof(p_para->ip) - 1 )
			{
				InfoLog( __FILE__ , __LINE__ , "parameter -l[%s]中的ip过长" , argv[c+1] );
				return 1;
			}
			strncpy( p_para->ip , argv[c+1] , p - argv[c+1] );
			p_para->port = atoi(p+1) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "-s" ) && c + 1 < argc )
		{
			strncpy( p_para->so_pathfilename , argv[c+1] , sizeof(p_para->so_pathfilename)-1 );
			c++;
		}
#if ( defined __linux__ ) || ( defined __unix )
		else if( STRCMP( argv[c] , == , "--work-user" ) && c + 1 < argc )
		{
			strncpy( p_para->work_user , argv[c+1] , sizeof(p_para->work_user)-1 );
			c++;
		}
#endif
		else if( STRCMP( argv[c] , == , "--work-path" ) && c + 1 < argc )
		{
			strncpy( p_para->work_path , argv[c+1] , sizeof(p_para->work_path)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "--logfile" ) && c + 1 < argc )
		{
			strncpy( p_para->log_pathfilename , argv[c+1] , sizeof(p_para->log_pathfilename)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-fatal" ) )
		{
			p_para->log_level = LOGLEVEL_FATAL ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-error" ) )
		{
			p_para->log_level = LOGLEVEL_ERROR ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-warn" ) )
		{
			p_para->log_level = LOGLEVEL_WARN ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-info" ) )
		{
			p_para->log_level = LOGLEVEL_INFO ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-debug" ) )
		{
			p_para->log_level = LOGLEVEL_DEBUG ;
		}
		else if( STRCMP( argv[c] , == , "--tcp-nodelay" ) )
		{
			p_para->tcp_nodelay = 1 ;
		}
		else if( STRCMP( argv[c] , == , "--tcp-linger" ) && c + 1 < argc )
		{
			p_para->tcp_linger = atoi(argv[c+1]) ;
			c++;
		}
#if ( defined _WIN32 )
		else if( STRCMP( argv[c] , == , "--install-winservice" ) )
		{
			p_para->install_winservice = 1 ;
		}
		else if( STRCMP( argv[c] , == , "--uninstall-winservice" ) )
		{
			p_para->uninstall_winservice = 1 ;
		}
#endif
		else if( STRCMP( argv[c] , == , "--daemon-level" ) )
		{
			p_para->daemon_level = 1 ;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "parameter[%s] invalid" , argv[c] );
			usage();
			exit(1);
		}
	}
	
	return 0;
}

#if ( defined __linux__ ) || ( defined __unix )

/* 转换为守护进程 */
static int BindDaemonServer()
{
	pid_t		pid ;
	int		sig ;
	
	pid = fork() ;
	if( pid == -1 )
	{
		return -1;
	}
	else if( pid > 0 )
	{
		exit(0);
	}
	
	setsid();
	signal( SIGHUP,SIG_IGN );
	
	pid = fork() ;
	if( pid == -1 )
	{
		return -2;
	}
	else if( pid > 0 )
	{
		exit(0);
	}
	
	setuid( getpid() ) ;
	chdir("/tmp");
	umask( 0 ) ;
	
	for( sig = 0 ; sig < 30 ; sig++ )
	{
		signal( sig , SIG_IGN );
	}
	
	return 0;
}

int main( int argc , char *argv[] )
{
	struct TcpdaemonEntryParameter	para ;
	
	int				nret = 0 ;
	
	if( argc == 1 )
	{
		usage();
		exit(0);
	}
	
	/* 填充入口参数结构 */
	memset( & para , 0x00 , sizeof(struct TcpdaemonEntryParameter) );
	para.call_mode = TCPDAEMON_CALLMODE_CALLBACK ;
	para.max_process_count = 0 ;
	para.max_requests_per_process = 0 ;
	para.log_level = LOGLEVEL_INFO ;
	
	/* 解析命令行参数 */
	nret = ParseCommandParameter( argc , argv , & para ) ;
	if( nret )
		return nret;
	
	/* 检查命令行参数 */
	nret = CheckCommandParameter( & para ) ;
	if( nret )
		return nret;
	
	if( para.daemon_level == 1 )
	{
		/* 转换为守护进程 */
		nret = BindDaemonServer() ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "convert to daemon failed[%d]" , nret );
			return nret;
		}
	}
	
	/* 调用tcpdaemon函数 */
	return -tcpdaemon( & para );
}

#elif ( defined _WIN32 )

STARTUPINFO		g_si ;
PROCESS_INFORMATION	g_li ;
	
static int (* _iRTF_g_pfControlMain)(long lControlStatus) ;
SERVICE_STATUS		_gssServiceStatus ;
SERVICE_STATUS_HANDLE	_gsshServiceStatusHandle ;
HANDLE			_ghServerHandle ;
void			*_gfServerParameter ;
char			_gacServerName[257] ;
int			(* _gfServerMain)( void *pv ) ;

static int InstallDaemonServer( char *pcServerName , char *pcRunParameter )
{
	char		acPathFileName[256];
	char		acStartCommand[256];
	SC_HANDLE	schSCManager;
	SC_HANDLE	schService;
	
	memset( acPathFileName , 0x00 , sizeof( acPathFileName ) );
	GetModuleFileName( NULL, acPathFileName, 255 );
	_snprintf( acStartCommand , sizeof(acStartCommand)-1 , "\"%s\" %s" , acPathFileName , pcRunParameter );
	
	schSCManager = OpenSCManager( NULL , NULL , SC_MANAGER_CREATE_SERVICE ) ;
	if( schSCManager == NULL )
	{
		return -1;
	}
	
	schService = CreateService( schSCManager ,
				pcServerName ,
				pcServerName ,
				SERVICE_ALL_ACCESS ,
				SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS ,
				SERVICE_AUTO_START ,
				SERVICE_ERROR_NORMAL ,
				acStartCommand ,
				NULL ,
				NULL ,
				NULL ,
				NULL ,
				NULL );
	if( schService == NULL )
	{
		CloseServiceHandle( schSCManager );
		return -2;
	}
	
	CloseServiceHandle( schService );
	CloseServiceHandle( schSCManager );
	
	return 0;
}

static int UninstallDaemonServer( char *pcServerName )
{
	SC_HANDLE	schSCManager;
	SC_HANDLE	schService;
	
	BOOL		bret;
	
	schSCManager = OpenSCManager( NULL , NULL , SC_MANAGER_CREATE_SERVICE ) ;
	if( schSCManager == NULL )
	{
		return -1;
	}
	
	schService = OpenService( schSCManager, pcServerName, SERVICE_ALL_ACCESS) ;
	if( schService == NULL )
	{
		CloseServiceHandle( schSCManager );
		return -2;
	}
	
	bret = DeleteService( schService ) ;
	if( bret == FALSE )
	{
		CloseServiceHandle( schService );
		CloseServiceHandle( schSCManager );
		return -3;
	}
	
	CloseServiceHandle( schService );
	CloseServiceHandle( schSCManager );
	
	return 0;
}

static void WINAPI ServiceCtrlHandler( DWORD dwControl )
{
	switch ( dwControl )
	{
		case SERVICE_CONTROL_STOP :
		case SERVICE_CONTROL_SHUTDOWN :
			
			_gssServiceStatus.dwCurrentState = SERVICE_STOP_PENDING ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus ) ;
			_gssServiceStatus.dwCurrentState = SERVICE_STOPPED ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus) ;
			break;
			
		case SERVICE_CONTROL_PAUSE :
			
			_gssServiceStatus.dwCurrentState = SERVICE_PAUSE_PENDING ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus ) ;
			_gssServiceStatus.dwCurrentState = SERVICE_PAUSED ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus) ;
			break;
		
		case SERVICE_CONTROL_CONTINUE :
			
			_gssServiceStatus.dwCurrentState = SERVICE_CONTINUE_PENDING ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus ) ;
			_gssServiceStatus.dwCurrentState = SERVICE_RUNNING ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus) ;
			break;
			
		case SERVICE_CONTROL_INTERROGATE :
			
			_gssServiceStatus.dwCurrentState = SERVICE_RUNNING ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus) ;
			break;
			
		default:
			
			break;
			
	}
	
	if( _iRTF_g_pfControlMain )
		(* _iRTF_g_pfControlMain)( dwControl );
	
	return;
}

static void WINAPI ServiceMainProc( DWORD argc , LPTSTR *argv )
{
	_gssServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS ;
	_gssServiceStatus.dwCurrentState = SERVICE_START_PENDING ;
	_gssServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP ;
	_gssServiceStatus.dwWin32ExitCode = 0 ;
	_gssServiceStatus.dwServiceSpecificExitCode = 0 ;
	_gssServiceStatus.dwCheckPoint = 0 ;
	_gssServiceStatus.dwWaitHint = 0 ;
	
	_gsshServiceStatusHandle = RegisterServiceCtrlHandler( _gacServerName , ServiceCtrlHandler ) ;
	if( _gsshServiceStatusHandle == (SERVICE_STATUS_HANDLE)0 )
		return;
	
	_gssServiceStatus.dwCheckPoint = 0 ;
	_gssServiceStatus.dwWaitHint = 0 ;
	_gssServiceStatus.dwCurrentState = SERVICE_RUNNING ;
	
	SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus );
	
	_gfServerMain( _gfServerParameter );
	
	return;
}

static int BindDaemonServer( char *pcServerName , int (* ServerMain)( void *pv ) , void *pv , int (* pfuncControlMain)(long lControlStatus) )
{
	SERVICE_TABLE_ENTRY ste[] =
	{
		{ _gacServerName , ServiceMainProc },
		{ NULL , NULL }
	} ;
	
	BOOL		bret ;
	
	memset( _gacServerName , 0x00 , sizeof( _gacServerName ) );
	if( pcServerName )
	{
		strncpy( _gacServerName , pcServerName , sizeof( _gacServerName ) - 1 );
	}
	
	_ghServerHandle = GetCurrentProcess() ;
	_gfServerMain = ServerMain ;
	_gfServerParameter = pv ;
	_iRTF_g_pfControlMain = pfuncControlMain ;
	
	bret = StartServiceCtrlDispatcher( ste ) ;
	if( bret != TRUE )
		return -1;
	
	return 0;
}

static int monitor( void *command_line )
{
	BOOL			bret = TRUE ;
	
	/* 创建子进程，并监控其结束事件，重启之 */
	while(1)
	{
		memset( & g_si , 0x00 , sizeof(STARTUPINFO) );
		g_si.dwFlags = STARTF_USESHOWWINDOW ;
		g_si.wShowWindow = SW_HIDE ;
		memset( & g_li , 0x00 , sizeof(LPPROCESS_INFORMATION) );
		bret = CreateProcess( NULL , (char*)command_line , NULL , NULL , TRUE , 0 , NULL , NULL , & g_si , & g_li ) ;
		if( bret != TRUE )
		{
			fprintf( stderr , "CreateProcess failed , errno[%d]\n" , GetLastError() );
			return 1;
		}
		
		CloseHandle( g_li.hThread );
		
		WaitForSingleObject( g_li.hProcess , INFINITE );
		CloseHandle( g_li.hProcess );
		
		Sleep(10*1000);
	}
	
	return 0;
}

static int control( long control_status )
{
	/* 当收到WINDOWS服务结束事件，先结束子进程 */
	if( control_status == SERVICE_CONTROL_STOP || control_status == SERVICE_CONTROL_SHUTDOWN )
	{
		TerminateProcess( g_li.hProcess , 0 );
	}
	
	return 0;
}

int main( int argc , char *argv[] )
{
	struct TcpdaemonEntryParameter	para ;
	
	char				command_line[ 256 + 1 ] ;
	long				c , len ;
	
	int				nret = 0 ;
	
	if( argc == 1 )
	{
		usage();
		exit(0);
	}
	
	if( argc == 1 + 1 && STRCMP( argv[1] , == , "--uninstall-winservice" ) )
	{
		/* 卸载WINDOWS服务 */
		nret = UninstallDaemonServer( "TcpDaemon" ) ;
		if( nret )
		{
			printf( "Uninstall Windows Service Failure , error code[%d]\n" , nret );
		}
		else
		{
			printf( "Uninstall Windows Service Successful\n" );
		}
		
		return 0;
	}
	
	/* 填充入口参数结构 */
	memset( & para , 0x00 , sizeof(struct TcpdaemonEntryParameter) );
	para.call_mode = TCPDAEMON_CALLMODE_CALLBACK ;
	para.max_process_count = 0 ;
	para.max_requests_per_process = 0 ;
	para.log_level = LOGLEVEL_INFO ;
	
	/* 解析命令行参数 */
	nret = ParseCommandParameter( argc , argv , & para ) ;
	if( nret )
		return nret;
	
	/* 检查命令行参数 */
	nret = CheckCommandParameter( & para ) ;
	if( nret )
		return nret;
	
	if( para.install_winservice )
	{
		/* 安装WINDOWS服务 */
		memset( command_line , 0x00 , sizeof(command_line) );
		len = 0 ;
		for( c = 1 ; c < argc ; c++ )
		{
			if( STRCMP( argv[c] , != , "--install-winservice" ) && STRCMP( argv[c] , != , "--uninstall-winservice" ) )
			{
				if( strchr( argv[c] , ' ' ) || strchr( argv[c] , '\t' ) )
					len += SNPRINTF( command_line + len , sizeof(command_line)-1 - len , " \"%s\"" , argv[c] ) ;
				else
					len += SNPRINTF( command_line + len , sizeof(command_line)-1 - len , " %s" , argv[c] ) ;
			}
		}
		
		nret = InstallDaemonServer( "TcpDaemon" , command_line ) ;
		if( nret )
		{
			printf( "Install Windows Service Failure , error code[%d]\n" , nret );
		}
		else
		{
			printf( "Install Windows Service Successful\n" );
		}
		
		return 0;
	}
	
	if( para.daemon_level == 1 )
	{
		/* 父进程，转化为服务 */
		memset( command_line , 0x00 , sizeof(command_line) );
		len = SNPRINTF( command_line , sizeof(command_line) , "%s" , "tcpdaemon.exe" ) ;
		for( c = 1 ; c < argc ; c++ )
		{
			if( STRCMP( argv[c] , != , "--install-winservice" ) && STRCMP( argv[c] , != , "--uninstall-winservice" ) && STRCMP( argv[c] , != , "--daemon-level" ) )
			{
				if( strchr( argv[c] , ' ' ) || strchr( argv[c] , '\t' ) )
					len += SNPRINTF( command_line + len , sizeof(command_line)-1 - len , " \"%s\"" , argv[c] ) ;
				else
					len += SNPRINTF( command_line + len , sizeof(command_line)-1 - len , " %s" , argv[c] ) ;
			}
		}
		
		nret = BindDaemonServer( "TcpDaemon" , monitor , command_line , & control ) ;
	}
	else
	{
		/* 子进程，干活 */
		{
		WSADATA		wsd;
		if( WSAStartup( MAKEWORD(2,2) , & wsd ) != 0 )
			return 1;
		}
		
		/* 调用tcpdaemon函数 */
		nret = tcpdaemon( & para ) ;
		
		WSACleanup();
	}
	
	return -nret;
}

#endif
