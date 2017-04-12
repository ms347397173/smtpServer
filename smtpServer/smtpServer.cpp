/*******************************************************************
 * Summary:�ӿͻ��˻�ȡ�ʼ����ݱ��������ݿ�
 * Author:ms
 * Data: 2017/4/11
 ******************************************************************/
#define	_CRT_SECURE_NO_WARNINGS

#include"include\smtp_type.h"
#include"include\Trace.h"
#include"text_tools.h"
#include<stdio.h>

#include <Ws2tcpip.h>
#define inet_pton(family,addString,addBuf) InetPton(family,addString,addBuf)

#include<WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#include"mysql.h"
#include<string>
#include<iostream>
using namespace std;

#define DATABASENAME "smtp"
#define DATABASEUSER "root"
#define DATABASE_PASSWD "root"   /*��������ʹ�ÿ���̨����*/
#define MYSQL_IP "127.0.0.1"
#define MYSQL_PORT 3306
#define MYSQL_TABLE "smtp_info"

//ȫ�ֱ���
config_info_type g_config_info;

/***********************************************************************************************************
 * Summary:����mysql
 **********************************************************************************************************/
MYSQL* MySQLInitConn(const char* host, const char* user, const char* pass, const char* db, unsigned int port)
{
	MYSQL* sock = NULL;
	sock = mysql_init(NULL);
	if (sock &&mysql_real_connect(sock, host, user, pass, db, port, NULL, 0))
	{
		cout << "Connect Mysql Succeed!" << endl;
		mysql_query(sock, "set names gb2312;");//���ñ����ʽ,������cmd���޷���ʾ���� 
	}
	else
	{
		cout << "Connect Database \" " << db << " \" failed" << endl;
		cout << mysql_error(sock) << endl;
		mysql_close(sock);
		system("pause");
		exit(0);
	}
	return sock;
}

/***********************************************************************************************************
* Summary:�ر�mysql����
**********************************************************************************************************/
int MySQLDisconn(MYSQL* sock)
{
	mysql_close(sock);
	return 0;
}

/***********************************************************************
* Summary:��ȡ�����ļ���Ϣ
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

	read_config(buf, size, "eml_path", g_config_info.eml_path, spacer);  //��ȡftp����������eml�ļ���Ŀ¼

	fclose(fp);
	free(buf);
	return 0;
}

/*************************************************
*Summary:ʹ��g_config_info��ʼ��������socket
*Return:���ط���������socket
**************************************************/
SOCKET SocketInit()
{
	int err;
	WORD wVersionRequested = MAKEWORD(1, 1);  //typedef unsigned short WORD
	WSADATA wsaData;   //�ð���洢ϵͳ���صĹ���WinSocket������

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

	SOCKADDR_IN addrSrv;  //�������ĵ�ַ
	addrSrv.sin_addr.S_un.S_addr = g_config_info.server_ip;
	addrSrv.sin_family = AF_INET;  //ʹ�õ���TCP/IP 
	addrSrv.sin_port = g_config_info.server_port;  //תΪ������  ���ö˿ں�

	bind(socketServer, (sockaddr*)&addrSrv, sizeof(sockaddr));  //��
	listen(socketServer, 5);

	return socketServer;
}

/***************************************************************
*Summary:����mysql smtp_info��
*Param:
*	sock:MYSQL���
*Return:
*	true:�������ɹ�
*	false:������ʧ��
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
		cout << "Query Failed" << endl;
		return false;
	}
	cout << "�������ɹ�" << endl;;
	return true;
}

void addSql(__OUT_PARAM__ string& sqlString,const char * str)
{
	sqlString.append("\"");
	sqlString.append(str);
	sqlString.append("\",");
}

/*******************************************************************
 *Summary:��mailData�������ݿ��smtp_info��
 *Param:
 *		sock:MYSQL���
 *		mailData:������Դ����
 *Return:
 *		true:����ɹ�
 *		false:����ʧ��
 *��ע:mailData�����ڱ���������Ҫ��ת�����ܶ�Ӧ��smtp_info�����ֶ�
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
	//�����ռ����ֶΣ�����ֶ����ڰ��������������Ҫ�Ӷ�ά����ת��ΪTEXT��ʽ����ÿ�������˺����ֺŸ���
	sqlString.append("\"");
	for (int i = 0; i < mailData.sendto_num; ++i)
	{

		sqlString.append((const char *)mailData.sendto[i]);
		sqlString.append(";");  //�����
	}
	sqlString.append("\",");

	//add subject
	addSql(sqlString, (const char *)mailData.subject);
	
	//add date
	addSql(sqlString, (const char *)mailData.date);

	//add user_agent
	addSql(sqlString, (const char *)mailData.user_agent);

	//add attachment_name ͬsendto
	sqlString.append("\"");
	for (int i = 0; i < mailData.attachment_num; ++i)
	{

		sqlString.append((const char *)mailData.attachment_name[i]);
		sqlString.append(";");  //�����
	}
	sqlString.append("\",");

	//add eml_file
	sqlString.append("\"");
	sqlString.append((const char *)mailData.eml_file_name);
	sqlString.append("\");");  //���һ��û�ж���,����ĩβ���ŷֺ�

	if (mysql_query(sock, sqlString.c_str()))
	{
		cout << "insert failed" << endl;
		return false;
	}

	cout << "Insert Successed" << endl;
	return true;
}

void TestMysql()
{
	MYSQL* sock = MySQLInitConn("127.0.0.1", "root", "root", "my_table", 3306);
	string sqlString("SELECT * FROM table1");
	if (mysql_query(sock, sqlString.c_str()))
	{
		cout << "Query Failed" << endl;
		system("pause");
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



int main()
{
	memset(&g_config_info, 0, sizeof(g_config_info));
	//��ȡ�����ļ�
	if (read_config_file() == -1)
		return false;
	SOCKET socketServer = SocketInit();  //�õ�����������socket

	SOCKADDR_IN addrClient;  //����ͻ��˵�ip��ַ
	int len = sizeof(SOCKADDR);
	mail_data_type mailData;
	int mailDataLen = sizeof(mail_data_type);
	int receivedNums = 0;

	MYSQL* sock = MySQLInitConn(MYSQL_IP, DATABASEUSER, DATABASE_PASSWD, DATABASENAME, MYSQL_PORT);
	//CreateSmtpInfoTable(sock);  //����smtp_info��

	while (1)
	{
		//accept
		SOCKET sockConn = accept(socketServer, (SOCKADDR*)&addrClient, &len);
		//��������
		receivedNums = 0;
		while (1)
		{
			receivedNums += recv(sockConn, (char*)&mailData + receivedNums, mailDataLen-receivedNums, 0);
			if (receivedNums >= mailDataLen)
			{
				break;
			}
		}

		//�������ݿ⣨��һ���ع�Ϊ�����̴߳������ݿ⣩
		InsertSmtpInfo(sock, mailData);
		
		//�ر�socket
		closesocket(sockConn);
	}

	closesocket(socketServer);

	return 0;
}