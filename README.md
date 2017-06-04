tcpdaemon
=========

# 0.快速开始 #
老师交给小明一个开发任务，实现一个TCP网络迭代并发服务器，用于回射任何接收到的通讯数据。小明很懒，他在开源中国项目库里搜到了开源库tcpdaemon来帮助他快速完成任务。首先他安装好tcpdaemon，然后写了一个C程序文件test_callback_echo.c

    $ vi test_callback_echo.c
	#include "tcpdaemon.h"
	
	_WINDLL_FUNC int tcpmain( struct TcpdaemonServerEnvirment *p_env , int sock , void *p_addr )
	{
		char	buffer[ 4096 ] ;
		long	len ;
		
		len = recv( sock , buffer , sizeof(buffer) , 0 ) ;
        if( len <= 0 )
            return len;
        
		len = send( sock , buffer , len , 0 ) ;
        if( len < 0 )
            return len;
		
		return 0;
	}

他编译链接成动态库test_callback_echo.so，最后用tcpdaemon直接挂接执行

    $ tcpdaemon -m IF -l 0:9527 -s test_callback_echo.so -c 10 --tcp-nodelay --logfile $HOME/log/test_callback_echo.log

OK，总共花了五分钟，圆满完成老师作业。老师说这个太简单了，小明你给我改成像Apache经典的Leader-Follow服务端模型，小明说没问题，他把启动命令参数`-m IF`改成了`-m LF`，再次执行，完成老师要求，总共花了五秒钟。老师问你怎么这么快就改好了，小明说全靠开源项目tcpdaemon帮了大忙啊 ^_^

# 1.概述 #
tcpdaemon是一个TCP通讯服务端平台/库，它封装了众多常见服务端进程/线程管理和TCP连接管理模型（Forking、Leader-Follow、IO-Multiplex、WindowsThreads Leader-Follow），使用者只需加入TCP通讯数据收发和应用逻辑代码就能快速构建出完整的TCP应用服务器。

| 服务模型 | 模型说明 |
| ---- | ---- |
| Forking | 单进程主守护，每当一条TCP新连接进来后，接受之，创建子进程进入回调函数tcpmain处理之。一条连接对应一个子进程（短生命周期） |
| Leader-Follow | 单进程管理进程，预先创建一组子进程（长生命周期）并监控其异常重启。子进程等待循环争抢TCP新连接调用回调函数tcpmain处理之 |
| IO-Multiplex | 单进程主守护，IO多路复用等待TCP新连接进来事件、TCP数据到来事件，TCP数据可写事件，调用回调函数tcpmain处理之 |
| WindowsThreads Leader-Follow | 同Leader-Follow，区别在于预先创建一组子线程而非子进程 |

tcpdaemon提供了三种与使用者代码对接方式：(注意：.exe只是为了说明自己是可执行文件，在UNIX/Linux中可执行文件一般没有扩展名)

| 链接模式 | 链接关系 | 说明 |
| ---- | ----- | -- |
| 回调模式 | tcpdaemon.exe+user.so(tcpmain) | 可执行程序tcpdaemon通过启动命令行参数挂接用户动态库，获得动态库中函数tcpmain指针。当建立TCP连接后 或 IO多路复用模式下当可读可写事件发生时 调用回调函数tcpmain |
| 主调模式 | user.exe(main,tcpmain)+libtcpdaemon.a(tcpdaemon) | 用户可执行程序user.exe隐式链接库libtcpdaemon.a。用户函数main(user.exe)初始化tcpdaemon参数结构体，并设置回调函数tcpmain，调用函数tcpdaemon(libtcpdaemon.so)。当建立TCP连接后 或 IO多路复用模式下当可读可写事件发生时 调用回调函数tcpmain |
| 主调+回调模式 | user.exe(main)+libtcpdaemon.a(tcpdaemon) + user.so(tcpmain) | 同上，区别在于用户函数main不直接设置回调函数tcpmain而设置user.so文件名。函数tcpdaemon负责挂接动态库user.so并获得函数tcpmain指针 |

一般简单情况下，使用者采用回调模式即可，只要编写一个动态库user.so（内含回调函数tcpmain）被可执行程序tcpdaemon挂接上去运行。如果使用者想订制一些自定义处理，如初始化环境，可以采用主调模式，实现函数main里把自定义参数传递给tcpdaemon穿透给tcpmain。如果想实现运行时选择回调函数tcpmain则可以采用主调+回调模式。

# 2.编译安装 #
以Linux操作系统为例，下载到最新源码安装包tcpdaemon-x.y.z.tar.gz到某目录，解压之

    $ tar xvzf tcpdaemon-x.y.z.tar.gz
    ...
    $ cd tcpdaemon
    $ cd src
    $ make -f makefile.Linux install
    rm -f LOGC.o
    rm -f tcpdaemon_lib.o
    rm -f tcpdaemon_main.o
    rm -f tcpdaemon
    rm -f libtcpdaemon.a
    gcc -g -fPIC -O2 -Wall -Werror -fno-strict-aliasing -I. -I/home/calvin/include  -c tcpdaemon_lib.c
    gcc -g -fPIC -O2 -Wall -Werror -fno-strict-aliasing -I. -I/home/calvin/include  -c LOGC.c
    ar rv libtcpdaemon.a tcpdaemon_lib.o LOGC.o
    ar: 正在创建 libtcpdaemon.a
    a - tcpdaemon_lib.o
    a - LOGC.o
    gcc -g -fPIC -O2 -Wall -Werror -fno-strict-aliasing -I. -I/home/calvin/include  -c tcpdaemon_main.c
    gcc -g -fPIC -O2 -Wall -Werror -fno-strict-aliasing  -o tcpdaemon tcpdaemon_main.o tcpdaemon_lib.o LOGC.o -L. -L/home/calvin/lib -lpthread -ldl 
    cp -rf tcpdaemon /home/calvin/bin/
    cp -rf libtcpdaemon.a /home/calvin/lib/
    cp -rf tcpdaemon.h /home/calvin/include/tcpdaemon/

可以看到可执行程序tcpdaemon安装到$HOME/bin，静态库libtcpdaemon.a安装到$HOME/lib，开发用的头文件安装到$HOME/include/tcpdaemon。

# 3.使用示例 #
## 3.1.服务模型Forking，链接模式1，接收HTTP请求报文然后发送HTTP响应报文 ##
使用者只需编写一个函数tcpmain，实现同步的接收HTTP请求报文然后发送HTTP响应报文回去

    $ vi test_callback_http_echo.c
	#include "tcpdaemon.h"
	
	_WINDLL_FUNC int tcpmain( struct TcpdaemonServerEnvirment *p_env , int sock , void *p_addr )
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
		
		http_len = sprintf( http_buffer , "HTTP/1.0 200 OK\r\nContent-length: 17\r\n\r\nHello Tcpdaemon\r\n" ) ;
		SEND( sock , http_buffer , http_len , 0 );
		
		return 0;
	}

编译链接成test_callback_http_echo.so

    $ gcc -g -fPIC -O2 -Wall -Werror -fno-strict-aliasing -I. -I/home/calvin/include/tcpdaemon  -c test_callback_http_echo.c
    $ gcc -g -fPIC -O2 -Wall -Werror -fno-strict-aliasing -shared -o test_callback_http_echo.so test_callback_http_echo.o -L. -L/home/calvin/lib -lpthread -ldl 
    
用tcpdaemon直接挂接即可

    $ tcpdaemon -m IF -l 0:9527 -s test_callback_http_echo.so -c 10 --tcp-nodelay --logfile $HOME/log/test_callback_http_echo.log --loglevel-debug

可执行程序tcpdaemon所有命令行参数可以不带参数的执行而得到

    $ tcpdaemon
    USAGE : tcpdaemon -m IF -l ip:port -s so_pathfilename [ -c max_process_count ]
                      -m LF -l ip:port -s so_pathfilename -c process_count [ -n max_requests_per_process ]
            other options :
                      -v
                      [ --daemon-level ]
                      [ --work-path work_path ]
                      [ --work-user work_user ]

执行后可在$HOME/log下可以看到tcpdaemon的日志。

通过curl发测试请求

    $ curl "http://localhost:9527/"
    Hello Tcpdaemon

测试成功！

**所有代码在源码安装包的test目录里下可以找到**

## 3.2.服务模型IO-Multiplex，链接模式2，非堵塞的接收HTTP请求报文然后发送HTTP响应报文 ##
使用者编写一个函数main

    $ test_main_IOMP.c
    #include "tcpdaemon.h"
    
    extern int tcpmain( struct TcpdaemonServerEnvirment *p_env , int sock , void *p_addr );
    
    int main()
    {
    	struct TcpdaemonEntryParameter	ep ;
    	
    	memset( & ep , 0x00 , sizeof(struct TcpdaemonEntryParameter) );
    	
    	snprintf( ep.log_pathfilename , sizeof(ep.log_pathfilename) , "%s/log/test_main_IOMP.log" , getenv("HOME") );
    	ep.log_level = LOGLEVEL_DEBUG ;
    	
    	strcpy( ep.server_model , "IOMP" );
    	strcpy( ep.ip , "0" );
    	ep.port = 9527 ;
        ep.tcp_nodelay = 1 ;
    	
    	ep.process_count = 1 ;
    	
    	ep.pfunc_tcpmain = & tcpmain ;
    	ep.param_tcpmain = NULL ;
    	
    	return -tcpdaemon( & ep );
    }

结构体TcpdaemonEntryParameter所有成员说明在tcpdaemon.h里可以找到

    struct TcpdaemonEntryParameter
    {
    	int		daemon_level ;	/* 是否转化为守护服务 1:转化 0:不转化（缺省） */
    	
    	char		log_pathfilename[ 256 + 1 ] ;	/* 日志输出文件名，不设置则输出到标准输出上 */
    	int		log_level ;	/* 日志等级 */
    	
    	char		server_model[ 10 + 1 ] ;	/* TCP连接管理模型
    							LF:领导者-追随者预派生进程池模型 for UNIX,Linux
    							IF:即时派生进程模型 for UNIX,Linux
    							WIN-TLF:领导者-追随者预派生线程池模型 for win32
    							*/
    	int		process_count ;	/* 当为领导者-追随者预派生进程池模型时为工作进程池进程数量，当为即时派生进程模型时为最大子进程数量，当为IO多路复用模型时为工作进程池进程数量 */
    	int		max_requests_per_process ;	/* 当为领导者-追随者预派生进程池模型时为单个工作进程最大处理应用次数 */
    	char		ip[ 20 + 1 ] ;	/* 本地侦听IP */
    	int		port ;	/* 本地侦听PORT */
    	char		so_pathfilename[ 256 + 1 ] ;	/* 用绝对路径或相对路径表达的应用动态库文件名 */
    	
    	char		work_user[ 64 + 1 ] ;	/* 切换为其它用户运行。可选 */
    	char		work_path[ 256 + 1 ] ;	/* 切换到指定目录运行。可选 */
    	
    	func_tcpmain	*pfunc_tcpmain ;	/* 当函数调用模式时，指向把TCP连接交给应用入口函数指针 */
    	void		*param_tcpmain ;	/* 当函数调用模式时，指向把TCP连接交给应用入口函数的参数指针。特别注意：自己保证线程安全 */
    	
    	int		tcp_nodelay ;	/* 启用TCP_NODELAY选项 1:启用 0:不启用（缺省）。可选 */
    	int		tcp_linger ;	/* 启用TCP_LINGER选项 >=1:启用并设置成参数值 0:不启用（缺省）。可选 */
    	
    	/* 以下为内部使用 */
    	int		install_winservice ;
    	int		uninstall_winservice ;
    } ;


使用者再编写一个函数tcpmain，实现非堵塞的接收HTTP请求报文然后发送HTTP响应报文回去

    $ vi test_callback_http_echo_nonblock.c
    #include "tcpdaemon.h"
    
    struct AcceptedSession
    {
    	int		sock ;
    	struct sockaddr	addr ;
    	char		http_buffer[ 4096 + 1 ] ;
    	int		read_len ;
    	int		write_len ;
    	int		wrote_len ;
    } ;
    
    _WINDLL_FUNC int tcpmain( struct TcpdaemonServerEnvirment *p_env , int sock , void *p_addr )
    {
    	struct AcceptedSession	*p_accepted_session = NULL ;
    	struct epoll_event	event ;
    	int			len ;
    	
    	int			nret = 0 ;
    	
    	switch( TDGetIoMultiplexEvent(p_env) )
    	{
    		case IOMP_ON_ACCEPTING_SOCKET :
    			p_accepted_session = (struct AcceptedSession *)malloc( sizeof(struct AcceptedSession) ) ;
    			if( p_accepted_session == NULL )
    				return -1;
    			memset( p_accepted_session , 0x00 , sizeof(struct AcceptedSession) );
    			
    			p_accepted_session->sock = sock ;
    			memcpy( & (p_accepted_session->addr) , p_addr , sizeof(struct AcceptedSession) );
    			
    			memset( & event , 0x00 , sizeof(struct epoll_event) );
    			event.events = EPOLLIN | EPOLLERR ;
    			event.data.ptr = p_accepted_session ;
    			nret = epoll_ctl( TDGetThisEpoll(p_env) , EPOLL_CTL_ADD , p_accepted_session->sock , & event ) ;
    			if( nret == -1 )
    				return -1;
    			
    			break;
    			
    		case IOMP_ON_CLOSING_SOCKET :
    			p_accepted_session = (struct AcceptedSession *) p_addr ;
    			
    			epoll_ctl( TDGetThisEpoll(p_env) , EPOLL_CTL_DEL , p_accepted_session->sock , NULL );
    			close( p_accepted_session->sock );
    			free( p_accepted_session );
    			
    			break;
    			
    		case IOMP_ON_RECEIVING_SOCKET :
    			p_accepted_session = (struct AcceptedSession *) p_addr ;
    			
    			len = RECV( p_accepted_session->sock , p_accepted_session->http_buffer+p_accepted_session->read_len , sizeof(p_accepted_session->http_buffer)-1-p_accepted_session->read_len , 0 ) ;
    			if( len == 0 )
    				return 1;
    			else if( len == -1 )
    				return 1;
    			
    			p_accepted_session->read_len += len ;
    			
    			if( strstr( p_accepted_session->http_buffer , "\r\n\r\n" ) )
    			{
    				p_accepted_session->write_len = sprintf( p_accepted_session->http_buffer , "HTTP/1.0 200 OK\r\n"
    											"Content-length: 17\r\n"
    											"\r\n"
    											"Hello Tcpdaemon\r\n" ) ;
    				
    				memset( & event , 0x00 , sizeof(struct epoll_event) );
    				event.events = EPOLLOUT | EPOLLERR ;
    				event.data.ptr = p_accepted_session ;
    				nret = epoll_ctl( TDGetThisEpoll(p_env) , EPOLL_CTL_MOD , p_accepted_session->sock , & event ) ;
    				if( nret == -1 )
    					return -1;
    				
    				return 0;
    			}
    			
    			if( p_accepted_session->read_len == sizeof(p_accepted_session->http_buffer)-1 )
    				return 1;
    			
    			break;
    			
    		case IOMP_ON_SENDING_SOCKET :
    			p_accepted_session = (struct AcceptedSession *) p_addr ;
    			
    			len = SEND( p_accepted_session->sock , p_accepted_session->http_buffer+p_accepted_session->wrote_len , p_accepted_session->write_len-p_accepted_session->wrote_len , 0 ) ;
    			if( len == -1 )
    				return 1;
    			
    			p_accepted_session->wrote_len += len ;
    			
    			if( p_accepted_session->wrote_len == p_accepted_session->write_len )
    				return 1;
    			
    			break;
    			
    		default :
    			break;
    	}
    	
    	return 0;
    }
    
编译成test_main_IOMP

    gcc -g -fPIC -O2 -Wall -Werror -fno-strict-aliasing -I. -I/home/calvin/include/tcpdaemon  -c test_main_IOMP.c
    gcc -g -fPIC -O2 -Wall -Werror -fno-strict-aliasing -I. -I/home/calvin/include/tcpdaemon  -c test_callback_http_echo_nonblock.c
    gcc -g -fPIC -O2 -Wall -Werror -fno-strict-aliasing  -o test_main_IOMP test_main_IOMP.o test_callback_http_echo_nonblock.o -L. -L/home/calvin/lib -lpthread -ldl  -ltcpdaemon

执行test_main_IOMP

    $ ./test_main_IOMP

执行后可在$HOME/log下可以看到test_main_IOMP的日志。

通过curl发测试请求

    $ curl "http://localhost:9527/"
    Hello Tcpdaemon

测试成功！

**所有代码在源码安装包的test目录里下可以找到**

# 4.参考 #
## 4.1.TcpdaemonServerEnvirment环境函数集 ##

| 函数名 | TDGetTcpmainParameter |
| --:|:-- |
| 函数原型 | void *TDGetTcpmainParameter( struct TcpdaemonServerEnvirment *p_env ); |
| 输入参数 | struct TcpdaemonServerEnvirment *p_env tcpdaemon环境结构指针 |
| 返回值 | 调用tcpmain时传入TcpdaemonEntryParameter的param_tcpmain地址 |

| 函数名 | TDGetListenSocket |
| --:|:-- |
| 函数原型 | int TDGetListenSocket( struct TcpdaemonServerEnvirment *p_env ); |
| 输入参数 | struct TcpdaemonServerEnvirment *p_env tcpdaemon环境结构指针 |
| 返回值 | 侦听端口描述字 |

| 函数名 | TDGetListenSocketPtr |
| --:|:-- |
| 函数原型 | int *TDGetListenSocketPtr( struct TcpdaemonServerEnvirment *p_env ); |
| 输入参数 | struct TcpdaemonServerEnvirment *p_env tcpdaemon环境结构指针 |
| 返回值 | 侦听端口描述字的地址 |

| 函数名 | TDGetListenAddress |
| --:|:-- |
| 函数原型 | struct sockaddr_in TDGetListenAddress( struct TcpdaemonServerEnvirment *p_env ); |
| 输入参数 | struct TcpdaemonServerEnvirment *p_env tcpdaemon环境结构指针 |
| 返回值 | 侦听端口网络地址 |

| 函数名 | TDGetListenAddressPtr |
| --:|:-- |
| 函数原型 | struct sockaddr_in *TDGetListenAddressPtr( struct TcpdaemonServerEnvirment *p_env ); |
| 输入参数 | struct TcpdaemonServerEnvirment *p_env tcpdaemon环境结构指针 |
| 返回值 | 侦听端口网络地址的地址 |

| 函数名 | TDGetProcessCount |
| --:|:-- |
| 函数原型 | int TDGetProcessCount( struct TcpdaemonServerEnvirment *p_env ); |
| 输入参数 | struct TcpdaemonServerEnvirment *p_env tcpdaemon环境结构指针 |
| 返回值 | 配置最大并发度或静态进程池进程数量 |

| 函数名 | TDGetEpollArrayBase |
| --:|:-- |
| 函数原型 | int *TDGetEpollArrayBase( struct TcpdaemonServerEnvirment *p_env ); |
| 输入参数 | struct TcpdaemonServerEnvirment *p_env tcpdaemon环境结构指针 |
| 返回值 | IO多路复用epoll数组第一个元素的地址 |

| 函数名 | TDGetThisEpoll |
| --:|:-- |
| 函数原型 | int TDGetThisEpoll( struct TcpdaemonServerEnvirment *p_env ); |
| 输入参数 | struct TcpdaemonServerEnvirment *p_env tcpdaemon环境结构指针 |
| 返回值 | 当前IO多路复用epoll描述字 |

| 函数名 | TDGetIoMultiplexEvent |
| --:|:-- |
| 函数原型 | int TDGetIoMultiplexEvent( struct TcpdaemonServerEnvirment *p_env ); |
| 输入参数 | struct TcpdaemonServerEnvirment *p_env tcpdaemon环境结构指针 |
| 返回值 | 当前IO多路复用epoll事件 IOMP_ON_ACCEPTING_SOCKET:接受新连接事件 IOMP_ON_CLOSING_SOCKET:关闭连接事件 IOMP_ON_RECEIVING_SOCKET:接收通讯数据事件 IOMP_ON_SENDING_SOCKET:发送通讯数据事件 |

# 5.总结 #

tcpdaemon提供了多种服务模型和链接模式，旨在协助使用者快速构建TCP应用服务器，比如可以使用本人的另一个开源项目 [HTTP解析器fasterhttp](http://git.oschina.net/calvinwilliams/fasterhttp) 以百行以内代码构建出一个完整的Web服务器，还有一个完整的应用案例可参阅本人的另一个开源项目 [分布式发号器](http://git.oschina.net/calvinwilliams/coconut) ，经过tcpdaemon改造后应用代码缩短了一半。

tcpdaemon源码托管在 [开源中国码云](http://git.oschina.net/calvinwilliams/tcpdaemon)，你也可以通过 [邮箱](calvinwilliams@163.com) 联系到作者

能帮助到您是我的荣幸 ^_^
