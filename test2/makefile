# 此文件由makeobj.sh自动生成
############################################################
# 项目名 : 
# 模块名 : 
# 备  注 : 
############################################################

###### 源文件配置区
#@ c_FILE
c_FILE		=	\
			IDL_hello_world.dsc.c \
			test_callback_tdhb_hello_nonblock.c \
			test_server.c \
			test_client.c \
			test_client_latency.c \

###### 目标文件、安装目录配置区
NOINST_OBJ	=	test_server \
			test_client \
			test_client_latency \

###### 编译选项
CFLAGS		=	-I. \
			-I$(HOME)/include/tcpdaemon \

###### 链接选项
LFLAGS		=	-L. \
			-L$(HOME)/lib \
			-ltcpdaemon \
			-lpthread \
			-ldl \

###### 额外宏定义区
CLEAN_ADDITION	=

###### 加载mktpl模板库
#@ make_all
#@ make_clean

include $(MKTPLDIR)/makeobj_$(MKTPLOS).inc

###### 目标文件依赖关系
test_server	:	test_server.o test_callback_tdhb_hello_nonblock.o IDL_hello_world.dsc.o
	$(CC) $(ESOFLAGS) -o $@ test_server.o test_callback_tdhb_hello_nonblock.o IDL_hello_world.dsc.o $(LFLAGS)

test_client	:	test_client.o IDL_hello_world.dsc.o
	$(CC) $(ESOFLAGS) -o $@ test_client.o IDL_hello_world.dsc.o $(LFLAGS)

test_client_latency	:	test_client_latency.o IDL_hello_world.dsc.o
	$(CC) $(ESOFLAGS) -o $@ test_client_latency.o IDL_hello_world.dsc.o $(LFLAGS)

