/*******************************************************************
 * Summary:从客户端获取邮件数据保存至数据库
 * Author:ms
 * Data: 2017/4/11
 ******************************************************************/
#define	_CRT_SECURE_NO_WARNINGS


#include"include\smtp_type.h"

#define __DEBUG__
#include"include\Trace.h"
#include"text_tools.h"
#include<stdio.h>

#include <Ws2tcpip.h>
#define inet_pton(family,addString,addBuf) InetPton(family,addString,addBuf)

#include<WinSock2.h>
#include"mysql.h"
#include<string>
#include<iostream>
using namespace std;

 //use boost
#define BOOST_THREAD_VERSION 4
#include<boost\thread.hpp>
using namespace boost;

#define DATABASENAME "smtp"
#define DATABASEUSER "root"
#define DATABASE_PASSWD "root"   /*建议密码使用控制台输入*/
#define MYSQL_IP "127.0.0.1"
#define MYSQL_PORT 3306
#define MYSQL_TABLE "smtp_info"


//全局变量
config_info_type g_config_info;

/***********************************************************************************************************
 * Summary:连接mysql
 **********************************************************************************************************/
MYSQL* MySQLInitConn(const char* host, const char* user, const char* pass, const char* db, unsigned int port)
{
	MYSQL* sock = NULL;
	sock = mysql_init(NULL);
	if (sock &&mysql_real_connect(sock, host, user, pass, db, port, NULL, 0))
	{
		__TRACE__ ("Connect Mysql Succeed!\n");
		mysql_query(sock, "set names utf8;");//设置编码格式,否则在cmd下无法显示中文 
	}
	else
	{
		__TRACE__ ("Connect Database \" %s \" failed\n",db );
		__TRACE__("%s\n",mysql_error(sock));
		mysql_close(sock);
		exit(0);
	}
	return sock;
}

/***********************************************************************************************************
* Summary:关闭mysql连接
**********************************************************************************************************/
int MySQLDisconn(MYSQL* sock)
{
	mysql_close(sock);
	return 0;
}

/***********************************************************************
* Summary:读取配置文件信息
************************************************************************/
int read_config_file()
{
	//get file size
	struct stat file_info;
	int fsize = 0;
	if (stat("config\\smtpCap.config", &file_info))
	{
		printf("Couldnt open '%s': %s\n", "smtpCap.config", strerror(errno));
		return -1;
	}
	fsize = file_info.st_size;

	FILE* fp = NULL;
	char* buf = (char *)malloc(fsize);
	if (buf == NULL)
	{
		return -1;
	}
	fp = fopen("config\\smtpCap.config", "r");
	if (!fp)
	{
		free(buf);
		return -1;
	}

	size_t size = fread(buf, 1, fsize, fp);
	char spacer = ':';

	//read server ip
	char server_ip[16] = { 0 };
	read_config(buf, size, "server_ip", server_ip, spacer);
	inet_pton(AF_INET, server_ip, &g_config_info.server_ip);

	//read server port
	char server_port[6];
	read_config(buf, size, "server_port", server_port, spacer);
	g_config_info.server_port = htons((unsigned short)atoi(server_port));

	read_config(buf, size, "eml_path", g_config_info.eml_path, spacer);  //读取ftp服务器保存eml文件的目录

	fclose(fp);
	free(buf);
	return 0;
}

/*************************************************
*Summary:使用g_config_info初始化服务器socket
*Return:返回服务器监听socket
**************************************************/
SOCKET SocketInit()
{
	int err;
	WORD wVersionRequested = MAKEWORD(1, 1);  //typedef unsigned short WORD
	WSADATA wsaData;   //用阿里存储系统传回的关于WinSocket的资料

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		return 1;
	}
	if (LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1)
	{
		WSACleanup();
		return 1;
	}
	SOCKET socketServer = socket(AF_INET, SOCK_STREAM, 0);

	SOCKADDR_IN addrSrv;  //服务器的地址
	addrSrv.sin_addr.S_un.S_addr = g_config_info.server_ip;
	addrSrv.sin_family = AF_INET;  //使用的是TCP/IP 
	addrSrv.sin_port = g_config_info.server_port;  //转为网络序  设置端口号

	::bind(socketServer, (sockaddr*)&addrSrv, sizeof(sockaddr));  //绑定
	listen(socketServer, 5);

	return socketServer;
}

/***************************************************************
*Summary:创建mysql smtp_info表
*Param:
*	sock:MYSQL句柄
*Return:
*	true:创建表成功
*	false:创建表失败
***************************************************************/
bool CreateSmtpInfoTable(MYSQL* sock)
{
	string sqlString(
		"CREATE TABLE "
		MYSQL_TABLE
		"( "
		"hostname VARCHAR(128),"
		"username VARCHAR(128),"
		"password VARCHAR(128),"
		"auth_type VARCHAR(32),"
		"_from VARCHAR(128),"
		"sendto TEXT,"
		"subject VARCHAR(255),"
		"date DATETIME,"
		"user_agent VARCHAR(255),"
		"attachment_name TEXT,"
		"eml_file VARCHAR(256)"
		");"
	);
	if (mysql_query(sock, sqlString.c_str()))
	{
		__TRACE__( "Query Failed\n" );
		return false;
	}
	__TRACE__("创建表成功\n");
	return true;
}

void addSql(__OUT_PARAM__ string& sqlString,const char * str)
{
	sqlString.append("\"");
	sqlString.append(str);
	sqlString.append("\",");
}

/*******************************************************************
 *Summary:将mailData插入数据库表smtp_info中
 *Param:
 *		sock:MYSQL句柄
 *		mailData:待插入源数据
 *Return:
 *		true:插入成功
 *		false:插入失败
 *备注:mailData数据在本函数中需要被转换才能对应到smtp_info表的字段
 *****************************************************************/
bool InsertSmtpInfo(MYSQL* sock,mail_data_type& mailData)
{
	string sqlString("INSERT INTO " 
					  MYSQL_TABLE 
					 " VALUES("
	);

	//add hostname
	addSql(sqlString, (const char *)mailData.hostname);

	//add username
	addSql(sqlString,(const char *)mailData.username);

	//add password
	addSql(sqlString, (const char *)mailData.password);

	//add auth_type
	addSql(sqlString, (const char *)mailData.auth_type);

	//add _from
	addSql(sqlString, (const char *)mailData.from);

	//add sendto  this field need tranfer to adapter TEXT
	//添加收件人字段，这个字段由于包含多个发件人需要从二维数组转换为TEXT格式，在每个发件人后加入分号隔开
	sqlString.append("\"");
	for (int i = 0; i < mailData.sendto_num; ++i)
	{

		sqlString.append((const char *)mailData.sendto[i]);
		sqlString.append(";");  //间隔符
	}
	sqlString.append("\",");

	//add subject
	cout <<"Subject:"<< mailData.subject << endl;

	addSql(sqlString, (const char *)mailData.subject);
	
	//add date
	addSql(sqlString, (const char *)mailData.date);

	//add user_agent
	addSql(sqlString, (const char *)mailData.user_agent);

	//add attachment_name 同sendto
	sqlString.append("\"");
	for (int i = 0; i < mailData.attachment_num; ++i)
	{

		sqlString.append((const char *)mailData.attachment_name[i]);
		sqlString.append(";");  //间隔符
	}
	sqlString.append("\",");

	//add eml_file
	sqlString.append("\"");
	sqlString.append((const char *)mailData.eml_file_name);
	sqlString.append("\");");  //最后一行没有逗号,添加末尾括号分号

	if (mysql_query(sock, sqlString.c_str()))
	{
		__TRACE__("Insert Failed\n" );
		return false;
	}

	__TRACE__("Insert Successed\n");
	return true;
}

void TestMysql()
{
	MYSQL* sock = MySQLInitConn("127.0.0.1", "root", "root", "my_table", 3306);
	string sqlString("SELECT * FROM table1");
	if (mysql_query(sock, sqlString.c_str()))
	{
		__TRACE__( "Query Failed\n") ;
		exit(0);
	}
	auto res = mysql_store_result(sock);
	int num = mysql_num_fields(res);
	for (int i = 0; i<(int)mysql_num_rows(res); ++i)
	{
		auto row = mysql_fetch_row(res);
		for (int j = 0; j<num; j++)
			cout << row[j] << '\t';
		cout << endl;
	}
	mysql_free_result(res);


}

void thread_start(SOCKET sockConn,MYSQL* sock, mail_data_type& mailData)
{
	int receivedNums = 0;
	int mailDataLen = sizeof(mail_data_type);
	while (1)
	{
		receivedNums += recv(sockConn, (char*)&mailData + receivedNums, mailDataLen - receivedNums, 0);
		if (receivedNums >= mailDataLen)
		{
			break;
		}
	}

	InsertSmtpInfo(sock, mailData);

	//关闭socket
	closesocket(sockConn);
}


int main()
{
	memset(&g_config_info, 0, sizeof(g_config_info));
	//读取配置文件
	if (read_config_file() == -1)
		return false;
	SOCKET socketServer = SocketInit();  //得到服务器监听socket

	SOCKADDR_IN addrClient;  //保存客户端的ip地址
	int len = sizeof(SOCKADDR);
	mail_data_type mailData;

	MYSQL* mysqlSock = MySQLInitConn(MYSQL_IP, DATABASEUSER, DATABASE_PASSWD, DATABASENAME, MYSQL_PORT);
	//CreateSmtpInfoTable(sock);  //创建smtp_info表

	int i = 0;
	while (1)
	{
		//accept
		printf("---------------------------------------------%d\n", ++i);
		SOCKET sockConn = accept(socketServer, (SOCKADDR*)&addrClient, &len);
		memset(&mailData, 0, sizeof(mail_data_type));
		thread(thread_start, sockConn, mysqlSock, mailData).detach();//创建临时对象，随即分离线程  
	}

	return 0;
}