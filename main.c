//hl7消息處理程序
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <sys/ipc.h>
#include <sys/msg.h>

#define SERVER_PORT 4601
#define MAX_TEXT 512
struct my_msg_st
{
    long int my_msg_type;
	char some_text[MAX_TEXT];
};
struct my_msg_st some_data;
int msgid;

//異常處理
void my_handler(int sig){
		//發送結束消息 給消息隊列
		char *edd = "end";
		strcpy(some_data.some_text,edd);
		/*添加消息*/
		if(msgsnd(msgid,(void *)&some_data,MAX_TEXT,0)==-1)
		{
			fprintf(stderr,"msgsed failed\n");
			exit(EXIT_FAILURE);
		}
}

//全局變量,alarm需要用到
int clientSocket;
char hearthl7[50]={0};
char queryhl7[200]={0};




//向字符串src的pos位置添加字符
void insert(char *src, int pos, char c)
{
    int i, L;
    L = (int)strlen(src);

    if (L < pos)
    {
        src[L] = c;
        src[L + 1] = '\0';
    }
    else
    {
        for (i = L; i > pos; i --)
        {
            src[i] = src[i - 1];
        }
        src[pos] = c;
        src[L + 1] = '\0';
    }
}
void addchar(char *src,char c){
	int i,len;
	len = (int)strlen(src);
	for(i=len;i>0;i--)
		src[i] = src[i-1];
	src[i] = c;
	src[len+1] = '\0';
}
void addnchar(char *src,char c){
	int len = (int)strlen(src);
	src[len] = c;
	src[len+1] = '\0';
		
}
//msh段的獲取，從hl7消息段中第一個0x0d之前的部分就是msh段
char *getMshSegment(char *hl7){
	if(hl7==NULL ||strncmp(hl7,"MSH",3) != 0)
		return NULL;
	char *res = (char *)malloc(sizeof(char *));
	char *flag = strchr(hl7,0x0d);
	int i = 0;
	while(flag != (hl7+i))
		i++;
	strncpy(res,hl7,i-1);
	return res;
}
//刪除第一個段
char * delSeg(char *hl7){
	if(hl7==NULL)
		return NULL;
	char *flag = strchr(hl7,0x0d);
	char *end = flag+1;
	if(end ==NULL){
		hl7 = NULL;
		return NULL;
	}
	return end;
}

char *getSeg(char *hl7,char *res){
	if(hl7==NULL)
		return NULL;
	char *flag = strchr(hl7,0x0d);
	int i = 0;
	char *end = flag+1;
	if(end == 0x00){
		hl7 = NULL;
		res[0] = 0;
		return NULL;
	}
	while(flag != (hl7+i))
		i++;
	strncpy(res,hl7,i);
	res[i]=0;

	flag++;
	hl7 = flag;
	return res;
}
char * getFie(int i,int j,char *segment,char *res){
	if(segment==NULL){
		printf("參數segment空");
		return NULL;
	}
	int k=0;
	char *flag;
	for(;k<i;k++){
		flag = strchr(segment,0x7c);// |的ascii是0x7c	
		flag++;
		segment = flag;	
	}	
	k=0;
	while(*(flag+k) != 0x7c)
		k++;
	strncpy(res,flag,k);
	res[k]=0;
	//flag = strchr(segment,0x7c);
	//*flag = 0x00;回報錯
	return 	NULL;
}


int parrse(char *hl7){
	if(hl7 == NULL)
		return 0;	
	char seg[50];
	char msgId[10];//標志位
	char param[10];//指標
	char value[10];//值
//每次從hl7中取得第一個段賦值給seg,剩餘hl7返回
	getSeg(hl7,seg);
	if(seg == NULL)
		printf("segment null\n");
//msh段 且信息標志位204,則該hl7有病人數據，進一步解析
	if(strncmp(seg,"MSH",3) != 0)
		printf("fail to find MSH\n");	
	getFie(9,10,seg,msgId);
	if(strncmp(msgId,"204",3)!=0)
		return 0;
//如果是則準備寫入文件
	FILE *fp = NULL;
	fp = fopen("hl7_4.txt", "a");

	char ltime[25];
	time_t now;
	struct tm *ttime;
	time(&now);//unix時間戳
	ttime=localtime(&now);
	strftime(ltime,25,"%Y-%m-%d %H:%M:%S",ttime);
	fputs(ltime,fp);
	fputc((int)'\n',fp);

//構造消息 消息隊列
	strcpy(some_data.some_text,ltime);

//依次取出OBX段，截取數據
	while(seg != NULL){
		hl7 = delSeg(hl7);
		if(strcmp(hl7,"") == 0)  // “” 不等於 null
			break;
		getSeg(hl7,seg);
		if(strncmp(seg,"OBX",3)!=0)
			continue;
		getFie(3,4,seg,param);
		getFie(5,6,seg,value);
		printf("%s:%s\n",param,value);
		//構造消息	
		strcat(some_data.some_text,",");
		strcat(some_data.some_text,param);
		strcat(some_data.some_text,":");
		strcat(some_data.some_text,value);

		//記錄
		fputs(param,fp);
		fputc(':',fp);
		fputs(value,fp);
		fputc((int)'\n',fp);
//get first segment from hl7,return the rest hl7;
	}	
	fputc((int)'\n',fp);
	fclose(fp);
	//向消息隊列發送消息

	strcat(some_data.some_text,"\n");
	if(msgsnd(msgid,(void *)&some_data,MAX_TEXT,0)==-1)
		{
		    fprintf(stderr,"msgsed failed\n");
			exit(EXIT_FAILURE);
		}
	return 1;
}

//找到msh段的第10個域,每個域用 | 分割
char* get_msh_10(char *segment){
	char *msh = "MSH";
	char *flag;
	char *pos;
	char *result = (char *)malloc(sizeof(char *));	
	if(strncmp(msh,segment,3)!=0){
		printf("沒有找到msh三個字母");
		return 0;
	}
	int i=0;
	while(i!=9){
		i++;
		flag = strchr(segment,'|');// '|'的ascii是7c
		flag++;
		segment = flag;
	}
	pos = flag;	
	flag = strchr(segment,'|');// '|'的ascii是7c
	printf("%s\n",pos);
	printf("%s\n",flag);
	i = 0;
	while(*(pos+i)!=0x7c)
		i++;
	printf("%d\n",i);
	strncpy(result,pos,i);
	//*flag =0x00; 回報錯
	return result;
}
//得到segment段中第i個'|'和第j個'|'之間的域的值
//eg:   OBX||NM|101^HR|2101|60||||||F  要得到101^HR,調用函數i=3,j=4即可
char * getField(int i,int j,char *segment){
	if(segment==NULL){
		printf("參數segment空");
		return NULL;
	}
	int k=0;
	char *flag;
	for(;k<i;k++){
		flag = strchr(segment,0x7c);// |的ascii是0x7c	
		flag++;
		segment = flag;	
	}	
	k=0;
	while(*(flag+k) != 0x7c)
		k++;
	char *res= (char *)malloc(sizeof(char *));
	strncpy(res,flag,k);
	//flag = strchr(segment,0x7c);
	//*flag = 0x00;回報錯
	return 	res;
}


//seperate hl7 string with \r
//用\r分段，每段的起始位置用指針存儲在result指針數組中，數組最後1個元素是空指針 
char **  split(char *hl7){
	if(hl7 == NULL)
		return 0;
	char *flag;// \r出現的位置
	char **result = (char **)malloc(1*sizeof(char *)); 
	if(result == NULL)
		return 0;
	int length = 1;
	result[0] = hl7;
	while(strchr(hl7,0x0d) != NULL){
		flag = strchr(hl7,0x0d);
		//printf("%c",*(flag+1));
		*flag = 0x00;
		flag++;
		if(flag != NULL){
			hl7 = flag;
			length++;
			result = (char **)realloc(result,length*sizeof(char *));
			if(result == NULL){
				printf("realloc error");
				return 0;
			}
			result[length-1] = hl7;
		}
	}
	length++;
	result = (char **)realloc(result,length*sizeof(char *));
	result[length-1] = NULL;
	return result;
}


//查詢多少個段
int count(char **segments){
	int k=0;	
	while(*segments != NULL){
		k++;
		*segments++;
	}
	return k;

}


void parseHl7(char * hl7msg){
	int i;
	char *param;
	char *value;
	char *temp1;
	char *temp2;
	//msh段
	char *msh = getMshSegment(hl7msg);			
	char *msgId = getField(9,10,msh);
	if(strncmp(msgId,"204",3)!=0)
		return;
	free(msh);
	free(msgId);
	//如果msg control ID 是204,則該hl7有病人檢測數據
	char **segs = split(hl7msg);
	int segNum = count(segs);
	//写入文件 
	FILE *fp = NULL;
	fp = fopen("record3.txt", "a");
	for(i=0;i<segNum;i++){
		if(segs[i]!=NULL){
			//判斷是不是obx段
			if(strncmp(segs[i],"OBX",3)!=0)
				continue;
			param = getField(3,4,segs[i]);
			value = getField(5,6,segs[i]);
			//fprintf(fp, "This is testing for fprintf...\n");
			//fputc((int)'\n',fp);
			fputs(param,fp);
			fputc(':',fp);
			fputs(value,fp);
			fputc((int)'\n',fp);
			//釋放空間
			free(param);
			free(value);
//		if(fputs(param":"value, fp) < 0){
//			perror("file wiriting fail");
//		} 
		}
	}
	free(segs);
	fclose(fp);

}


void build_hearthl71(char *res){
	res = "MSH|^~\\&|||||||ORU^R01|106|P|2.3.1|";
	addchar(res,0x0b);
	addnchar(res,0x0d);
	addnchar(res,0x1c);
	addnchar(res,0x0d);
}
void build_queryhl7()
{
	char r[150];
	char start[2] = {0x0b,'\0'};
	char end1[2] = {0x1c,'\0'};
	char end[3] = {0x1c,0x0d,'\0'};
	char *msh="MSH|^~\\&|||||||QRY^R02|1203|P|2.3.1\r";
	char qrd[50]="QRD|";		
	char *qrd1 = "|R|I|Q839572|||||RES\r";
	char *qrf1="QRF|MON||||0&0^1^1^0^101&151&160&161\r";
	char *qrf2="QRF|MON||||0&0^1^1^0^200&350\r";//不夠啦可以繼續加
	char *qrf3="QRF|MON||||0&0^1^1^1^\r";//查詢所有參數
	
	//qrd段需要時間
	char hl7time[18];
	time_t now;
	struct tm *ttime;
	time(&now);//unix時間戳
	ttime=localtime(&now);
	strftime(hl7time,18,"%Y%m%d%H%M%S",ttime);

	strcat(qrd,hl7time);
	strcat(qrd,qrd1);
	
	strcpy(r,start);
	strcat(r,msh);
	strcat(r,qrd);
	strcat(r,qrf1);
	strcat(r,qrf2);
	strcat(r,end);
	memcpy(queryhl7,r,strlen(r));
	//queryhl7 =r;
	//return queryhl7;	
}
//構造心跳包
//每秒1次
//ipm系列監護儀 如果未收到心跳包會主動斷開鏈接
//TCP維持包: MSH|^~\&|||||||ORU^R01|106|P|2.3.1|
//hl7消息+1頭2尾  0x0b{hl7 msg}0x1c0x0d
//段的結尾還有1個0x0d
void build_hearthl7(){
	char r[40]={0};
	char *hl7 = "MSH|^~\\&|||||||ORU^R01|106|P|2.3.1|";
	char start[2] = {0x0b,'\0'};
	char end1[2] = {0x1c,'\0'};
	char end[4] = {0x0d,0x1c,0x0d,'\0'};
	//char *r = (char *)malloc(sizeof(char *)*40);
	strcpy(r,start);	
	strcat(r,hl7);
	strcat(r,end);
	memcpy(hearthl7,r,strlen(r));
	//hearthl7 = r;
	//return hearthl7;
}
void heartBeat(){
	send(clientSocket, hearthl7, strlen(hearthl7), 0);
	send(clientSocket, queryhl7, strlen(queryhl7), 0);
	//free(hearthl7);
	printf("心跳包發送ing...%d\n",(int)strlen(hearthl7));
	printf("查詢病人數據ing...%d\n",(int)strlen(queryhl7));
	signal(SIGALRM,heartBeat);
	alarm(1);
}

int main()

{
	//遇到ctrl+c,向消息隊列發送結束標志 end
	signal(SIGINT,my_handler);

	//char *queryhl7=(char *)malloc(sizeof(char *)*40);
	//char *hearthl7=(char *)malloc(sizeof(char *)*150);
	//queryhl7 = hearthl7=NULL;
	//構造心跳包
	//hearthl7 = build_hearthl7();
	//構造查詢包
	//queryhl7 = build_queryhl7();
	build_hearthl7();
	build_queryhl7();
	FILE *fe = NULL;
	fe = fopen("sent.txt", "a");
	fputc((int)'\n',fe);
	fputs(hearthl7,fe);
	fputc((int)'\n',fe);
	fputs(queryhl7,fe);
	fclose(fe);

	//print(hearthl7);
	//print(queryhl7);
    //客户端只需要一个套接字文件描述符，用于和服务器通信
    
    
    //描述服务器的socket
    
    struct sockaddr_in serverAddr;
    
	char hl7msg[800];
	char lastbyte;
	int count = 0;
	char hehe[1024];

   // msg queue 
	char *msgpath = "/home/xiaofeng/hl7/ipc/";
	key_t key= ftok(msgpath,3);
	

	//創建消息隊列
	msgid=msgget(key,0666 | IPC_CREAT);
	if(msgid==-1)
	{
	    fprintf(stderr,"msgget failed with error:%d\n",errno);
		exit(EXIT_FAILURE);
    }
	some_data.my_msg_type =1;	

    unsigned char byte;//網絡劉中讀取字節
    
    int recvlen;
    
    if((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    
    {
    
    perror("socket fail");
    
    return 1;
    
    }
    
    serverAddr.sin_family = AF_INET;
    
    serverAddr.sin_port = htons(SERVER_PORT);

    //指定服务器端的ip，本地测试：127.0.0.1
    
    //inet_addr()函数，将点分十进制IP转换成网络字节序IP
    
    serverAddr.sin_addr.s_addr = inet_addr("192.168.1.33");
    
    if(connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    
    {
    
    perror("connect fail");
    
    return 1;
    
    }
    printf("连接到监护仪...\n");
    
	//心跳，每秒1次
	heartBeat();
	//printf("hello");
	//while(1){};
	
	int cp =0;//是否復制到hl7msg
	int x=0;// hehe 遊標
	int y=0; //hl7msg 遊標
    while(1)
    
    {
	//循環讀取1024字節

			  	if(recv(clientSocket, hehe, 1024, 0) == 0){
					printf("read byte from stream fail. \n");
					break;
				}
				//FILE *ff = NULL;
				//ff = fopen("hl7_1.txt", "a");
				//fputc((int)'\n',ff);
				//if(fputs(hehe, ff) < 0){
				//	perror("file wiriting fail");
				//	return 1;
				//} 
				//fclose(ff);
	//從1024字節中截取hl7msg字符串
				y=0;//hl7字符串數組下標
				cp=0;//是否開始記錄hl7,遇到開始字符11設爲1,遇到結束字符28設0

				//ff = fopen("hl7_2.txt", "a");
				//fputc((int)'\n',ff);
				//fputc((int)'\n',ff);
				//fputc((int)'\n',ff);
				for(x=0;x<1024;x++){
					if(hehe[x]==11 ){				
						cp=1;
						y=0;
						continue;
					}
					if(cp==1){
						if(hehe[x]==28){
								cp=0;
								hl7msg[y] = 0x00;
								y=0;
								//printf("%s\n",hl7msg);
								//fputc((int)'\n',ff);
								//if(fputs(hl7msg, ff) < 0){
								//	perror("file wiriting fail");
								//	return 1;
								//} 
								parrse(hl7msg);
								continue;
						}
						hl7msg[y]= hehe[x];
						y++;
						continue;
					}
					
				}
				//fclose(ff);
				continue;
			
		  	if(recv(clientSocket, &byte, 1, 0) == 0){
				printf("read byte from stream fail. \n");
				break;
			} 
			if(byte == 11){ //hl7開端 \x0b
				count = 0;
				continue;	
			}
			if(byte == 28){ //hl7結尾 \x1c \x0d
				lastbyte = byte;
				//再讀1字節
			  	if(recv(clientSocket, &byte, 1, 0) == 0){
					printf("read byte from stream fail. \n");
					break;
				}
				if(byte == 13){//hl7結尾 \x0d
					//本條消息結束，開始拼接，處理，寫入文件	
					hl7msg[count] = '\0';
					//printf("hl7消息是%s",hl7msg);
					parrse(hl7msg);
					//写入文件 
				FILE *ff = NULL;
				ff = fopen("hl7rec.txt", "a");
				fputc((int)'\n',ff);
				if(fputs(hl7msg, ff) < 0){
					perror("file wiriting fail");
					return 1;
				} 
				fclose(ff);
					count = 0;

				}else{
					hl7msg[count++] = lastbyte;
					hl7msg[count++] = byte;
				}
				continue;
			}else{
				hl7msg[count++] = byte;
			}
			//printf("zifushi %d\n",byte);
			//printf("%d\n%d\n",byte,ntohs(byte));
    }
    close(clientSocket);

    return 0;

}

