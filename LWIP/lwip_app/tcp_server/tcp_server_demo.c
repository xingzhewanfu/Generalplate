#include "tcp_server_demo.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "led.h"
#include "lwip/lwip_sys.h"
#include "lwip/api.h"
#include "lcd.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407开发板
//NETCONN API编程方式的TCP服务器测试代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2014/8/15
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
//*******************************************************************************
//修改信息
//无
////////////////////////////////////////////////////////////////////////////////// 	   
 

 typedef struct{
		u8 Head[2];									//帧头1		0x88 0x66	
		u8 TotalControlEnable;		//总控使能  0/Enable  1/Disable
		u8 LeftWheelIncrement[4];				//左轮增量，单位mm 量程为0-4228.250625KM
		u8 RightWheelIncrement[4];				//右轮增量，单位mm  量程为0-4228.250625KM
		u8 CCWAngleIncrement[2];		//逆时针角度增量(0.1°)
		u8 EncoderErrorCode;			//编码器故障码
		u8 SpeedErrorCode;				//电机转数故障码
		u8 AngleErrorCode;				//电机角度故障码
		u8 DriverErrorCode[2];				//驱动器故障码
		u8 IMUErrorCode;					//IMU故障码
		u8 CurrentBatteryLevel;		//当前电量百分比
		u8 BatteryErrorCode;			//电池故障码
		u8 ChargingPileAngle;			//充电桩偏角
		u8 ChargingCircuitCurrent;//充电回路电流标志
		u8 LocationErrorCode;			//定位故障码
		u8 UltrasonicDistanceNo1[2];//1号超声波距离
		u8 UltrasonicDistanceNo2[2];//2号超声波距离
		u8 UltrasonicDistanceNo3[2];//3号超声波距离
		u8 UltrasonicDistanceNo4[2];//4号超声波距离
	 	u8 UltrasonicDistanceNo5[2];//5号超声波距离
		u8 UltrasonicDistanceNo6[2];//6号超声波距离
		u8 UltrasonicDistanceNo7[2];//7号超声波距离
		u8 UltrasonicDistanceNo8[2];//8号超声波距离
		u8 UltrasonicErrorCode;		//超声波故障码
		u8 InfraredRanging[4];			//红外测距   1/车轮悬空 0/车轮未悬空 InfraredRanging[0] 红外1 InfraredRanging[1] 红外2 InfraredRanging[2] 红外3 InfraredRanging[3]红外4  测距范围0-255mm
		u8 InfraredErrorCode;			//红外测距错误码
		u8 CrashSensorStatus[2];		//碰撞传感器状态 CrashSensorStatus[0] 传感器1状态 CrashSensorStatus[1]传感器2状态
		u8 CommunicationErrorCode;//碰撞传感器错误码
		u8 Reserved1;						//保留位
		u8 Reserved2;						//保留位
		u8 Reserved3;						//保留位
		u8 Reserved4;						//保留位
		u8 Timestamp;							//时间戳
		u8 CRCCheck[2];							//CRC校验
 }TCP_PACK;
u8 TCP_PACK_BUFSIZE=56;
TCP_PACK Tcp_Server_Recvbuf;
TCP_PACK Tcp_Server_Transbuf;
u8 tcp_server_recvbuf[TCP_SERVER_RX_BUFSIZE];	//TCP客户端接收数据缓冲区
u8 *tcp_server_sendbuf="Explorer STM32F407 NETCONN TCP Server send data\r\n";	
u8 tcp_server_flag;								//TCP服务器数据发送标志位

//TCP客户端任务
#define TCPSERVER_PRIO		6
//任务堆栈大小
#define TCPSERVER_STK_SIZE	300
//任务控制块
OS_TCB TCPSERVERTaskTCB;
//任务堆栈
CPU_STK TCPSERVER_TASK_STK[TCPSERVER_STK_SIZE];
  

//tcp服务器任务
static void tcp_server_thread(void *arg)
{
//	OS_ERR err_os;
	CPU_SR_ALLOC();
	u32 data_len = 0;
	struct pbuf *q;
	err_t err,recv_err;
	u8 remot_addr[4];
	struct netconn *conn, *newconn;
	static ip_addr_t ipaddr;
	static u16_t 			port;
	 
	LWIP_UNUSED_ARG(arg);

	conn = netconn_new(NETCONN_TCP);  //创建一个TCP链接
	netconn_bind(conn,IP_ADDR_ANY,TCP_SERVER_PORT);  //绑定端口 8号端口
	netconn_listen(conn);  		//进入监听模式
	conn->recv_timeout = 10;  	//禁止阻塞线程 等待10ms
	while (1) 
	{
		err = netconn_accept(conn,&newconn);  //接收连接请求
		if(err==ERR_OK)newconn->recv_timeout = 10;

		if (err == ERR_OK)    //处理新连接的数据
		{ 
			struct netbuf *recvbuf;

			netconn_getaddr(newconn,&ipaddr,&port,0); //获取远端IP地址和端口号
			
			remot_addr[3] = (uint8_t)(ipaddr.addr >> 24); 
			remot_addr[2] = (uint8_t)(ipaddr.addr>> 16);
			remot_addr[1] = (uint8_t)(ipaddr.addr >> 8);
			remot_addr[0] = (uint8_t)(ipaddr.addr);
			printf("主机%d.%d.%d.%d连接上服务器,主机端口号为:%d\r\n",remot_addr[0], remot_addr[1],remot_addr[2],remot_addr[3],port);
			Tcp_Server_Transbuf.Head[0] =0x66;
			Tcp_Server_Transbuf.Head[1] =0x88;
			Tcp_Server_Transbuf.ChargingPileAngle =0xcf;
			while(1)
			{
				if((tcp_server_flag & LWIP_SEND_DATA) == LWIP_SEND_DATA) //有数据要发送
				{
//					err = netconn_write(newconn ,tcp_server_sendbuf,strlen((char*)tcp_server_sendbuf),NETCONN_COPY); //发送tcp_server_sendbuf中的数据
					
					err = netconn_write(newconn ,&Tcp_Server_Transbuf,TCP_PACK_BUFSIZE,NETCONN_COPY); //发送tcp_server_sendbuf中的数据
					if(err != ERR_OK)
					{
						printf("发送失败\r\n");
					}
					tcp_server_flag &= ~LWIP_SEND_DATA;
				}
				if((recv_err = netconn_recv(newconn,&recvbuf)) == ERR_OK)  	//接收到数据
				{	
					OS_CRITICAL_ENTER();	//关中断					
					memset(&Tcp_Server_Recvbuf,0,TCP_PACK_BUFSIZE);  //数据接收缓冲区清零
//					memset(tcp_server_recvbuf,0,TCP_PACK_BUFSIZE);  //数据接收缓冲区清零
					for(q=recvbuf->p;q!=NULL;q=q->next)  //遍历完整个pbuf链表
					{
						//判断要拷贝到TCP_SERVER_RX_BUFSIZE中的数据是否大于TCP_SERVER_RX_BUFSIZE的剩余空间，如果大于
						//的话就只拷贝TCP_SERVER_RX_BUFSIZE中剩余长度的数据，否则的话就拷贝所有的数据
						if(q->len > (TCP_PACK_BUFSIZE-data_len)) memcpy(&Tcp_Server_Recvbuf+data_len,q->payload,(TCP_PACK_BUFSIZE-data_len));//拷贝数据
						else memcpy(&Tcp_Server_Recvbuf+data_len,q->payload,q->len);
						data_len += q->len;  	
						if(data_len > TCP_PACK_BUFSIZE) break; //超出TCP客户端接收数组,跳出	
					}
					OS_CRITICAL_EXIT();	//开中断
					data_len=0;  //复制完成后data_len要清零�
					if((Tcp_Server_Recvbuf.Head[0] == 0x88)&&(Tcp_Server_Recvbuf.Head[1] == 0x66))
					{					
						printf("Tcp_Server_Recvbuf.Head check ok head1==0x88 head2==0x66\r\n");  //数据有误
					}
					LCD_ShowString(30,270,210,200,16,(u8*)&Tcp_Server_Recvbuf);
					printf("rece:%s\r\n",(char*)&Tcp_Server_Recvbuf);  //通过串口发送接收到的数据
					memset(&Tcp_Server_Recvbuf,0,TCP_SERVER_RX_BUFSIZE);  //数据接收缓冲区清零
					netbuf_delete(recvbuf);
				}else if(recv_err == ERR_CLSD)  //关闭连接
				{
					netconn_close(newconn);
					netconn_delete(newconn);
					printf("主机:%d.%d.%d.%d断开与服务器的连接\r\n",remot_addr[0], remot_addr[1],remot_addr[2],remot_addr[3]);
					break;
				}
				
			}
		}
	}
}


//创建TCP服务器线程
//返回值:0 TCP服务器创建成功
//		其他 TCP服务器创建失败
u8 tcp_server_init(void)
{
	OS_ERR err;
	CPU_SR_ALLOC();
	
	OS_CRITICAL_ENTER();	//进入临界区
	//创建TCP服务器线程
	OSTaskCreate((OS_TCB 	* )&TCPSERVERTaskTCB,		
				 (CPU_CHAR	* )"tcp server task", 		
                 (OS_TASK_PTR )tcp_server_thread, 			
                 (void		* )0,					
                 (OS_PRIO	  )TCPSERVER_PRIO,     	
                 (CPU_STK   * )&TCPSERVER_TASK_STK[0],	
                 (CPU_STK_SIZE)TCPSERVER_STK_SIZE/10,	
                 (CPU_STK_SIZE)TCPSERVER_STK_SIZE,		
                 (OS_MSG_QTY  )0,					
                 (OS_TICK	  )0,					
                 (void   	* )0,				
                 (OS_OPT      )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR, 
                 (OS_ERR 	* )&err);
	OS_CRITICAL_EXIT();		//退出临界区
	return 0;
}







