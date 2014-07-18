#include "service.h"

#if ( defined __linux__ ) || ( defined __unix )

/* 转换为守护进程 */
int ConvertToDaemon()
{
	pid_t		pid ;
	int		sig ;
	/*
	int		fd ;
	*/
	
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
	
	/*
	for( fd = 0 ; fd <= 2 ; fd++ )
	{
		CLOSE( fd );
	}
	*/
	
	return 0;
}

#elif ( defined _WIN32 )

static int (* _iRTF_g_pfControlMain)(long lControlStatus) ;
SERVICE_STATUS		_gssServiceStatus ;
SERVICE_STATUS_HANDLE	_gsshServiceStatusHandle ;
HANDLE			_ghServerHandle ;
void			*_gfServerParameter ;
char			_gacServerName[257] ;
int			(* _gfServerMain)( void *pv ) ;

int InstallDaemonServer( char *pcServerName , char *pcRunParameter )
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

int UninstallDaemonServer( char *pcServerName )
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

void WINAPI ServiceCtrlHandler( DWORD dwControl )
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

void WINAPI ServiceMainProc( DWORD argc , LPTSTR *argv )
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

int BindDaemonServer( char *pcServerName , int (* ServerMain)( void *pv ) , void *pv , int (* pfuncControlMain)(long lControlStatus) )
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

#endif

