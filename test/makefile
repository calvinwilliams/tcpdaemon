# 此文件由makeobj.sh自动生成
############################################################
# 项目名 : 
# 模块名 : 
# 备  注 : 
############################################################

###### 源文件配置区
#@ c_FILE
c_FILE		=	\
			test_callback_http_echo.c \
			test_callback_http_echo_nonblock.c \
			test_main_IF.c \
			test_main_LF.c \
			test_main_IOMP.c \

###### 目标文件、安装目录配置区
NOINST_OBJ	=	test_callback_http_echo.so \
			test_callback_http_echo_nonblock.so \
			test_main_IF \
			test_main_LF \
			test_main_IOMP \

###### 编译选项
CFLAGS		=	-I. \
			-I$(HOME)/include/tcpdaemon \

###### 链接选项
LFLAGS		=	-L. \
			-L$(HOME)/lib \
			-lpthread \
			-ldl \

###### 额外宏定义区
CLEAN_ADDITION	=

###### 加载mktpl模板库
#@ make_all
#@ make_clean

include $(MKTPLDIR)/makeobj_$(MKTPLOS).inc

###### 目标文件依赖关系
test_callback_http_echo.so:	test_callback_http_echo.o
	$(CC) $(SOFLAGS) -o $@ test_callback_http_echo.o $(LFLAGS)

test_callback_http_echo_nonblock.so:	test_callback_http_echo_nonblock.o
	$(CC) $(SOFLAGS) -o $@ test_callback_http_echo_nonblock.o $(LFLAGS)

test_main_IF	:	test_main_IF.o test_callback_http_echo.o
	$(CC) $(ESOFLAGS) -o $@ test_main_IF.o test_callback_http_echo.o $(LFLAGS) -ltcpdaemon

test_main_LF	:	test_main_LF.o
	$(CC) $(ESOFLAGS) -o $@ test_main_LF.o $(LFLAGS) -ltcpdaemon

test_main_IOMP	:	test_main_IOMP.o test_callback_http_echo_nonblock.o
	$(CC) $(ESOFLAGS) -o $@ test_main_IOMP.o test_callback_http_echo_nonblock.o $(LFLAGS) -ltcpdaemon

