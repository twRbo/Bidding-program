#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h> 
#include <sys/stat.h>
#include <signal.h>

struct bid
{
	long mtype;
	int qid;
	int price;
	char *obj;
	int num;
};

struct sell
{
	int qid;
	char *obj;
};

#define LOGIN 1
#define BID 2
#define REPEAT 3

struct bid global_buy,global_tmp;

int bidding_num=0;
int total_num=0;
int global_qid[30] ={0};
int price_tmp;
char *global_sale_obj;

void *send(void * ptr)
{
	while(1)
	{
		int flag;
		int i;
		sleep(5);
		global_buy.mtype=REPEAT;
		for(i=0; i<total_num; i++)
		{
			msgsnd(global_qid[i],&global_buy,sizeof(struct bid)-sizeof(long),0);
		}
		printf("repeat %d.\n",bidding_num+1);
		bidding_num++;
		if(bidding_num==3)
		{
			printf("恭喜 Qid:%d得標，得標價錢為%d元！\n",global_buy.qid,global_buy.price);
			system("ipcrm -a");
			exit(0);
		}	
	}
}

void *receive(void * ptr)
{
	while(1)
	{
		int flag;
		if(global_buy.qid!=0)
		{
			flag=msgrcv(global_buy.qid,&global_tmp,sizeof(struct bid),REPEAT,0);
			if(price_tmp!=global_tmp.price)
			{
				printf("\n今日競標商品：%s\n出價者Qid:%d.\n",global_sale_obj,global_tmp.qid);
				bidding_num=0;
			}
			if(flag!=-1)
			{
				printf("%d元%d次！\n",global_tmp.price,bidding_num+1);
				printf("如果要繼續競標，請直接出價！\n");
			}
			bidding_num++;
			price_tmp=global_tmp.price;
			if(bidding_num==3 &&global_tmp.price!=0)
			{
				printf("恭喜 Qid:%d得標，得標價錢為%d元！\n",global_tmp.qid,global_tmp.price);
				exit(0);
			}
		}
	}
}

int main(int argc,char **argv)
{
	int option;
	int i;
	int flag;
	struct bid buy,tmp;
	struct sell sale;
	key_t key_sale;
	key_sale=0x777;
	sale.qid=msgget(key_sale,0600|IPC_CREAT);
	pthread_t thread1;         
	while((option=getopt(argc,argv,"sb"))!=-1)
	{	
		switch(option)
		{
			case's':                       ///////////////////////////////////////////賣方
				sale.obj="SwitchXXX";
				int *qid =NULL;
				int price_now=0;
				int pt=0;
				int pt2=0;
				int num=0;
				float total=0;
				qid = (int*)malloc(sizeof(int));
				printf("launching bidding system...\n");
				while(1)
				{		
					flag=msgrcv(sale.qid,&tmp,sizeof(struct bid),0,0);
					tmp.obj=sale.obj;
					if(tmp.mtype==LOGIN && flag!=-1)
					{
						qid[num]=tmp.qid;
						global_qid[num]=tmp.qid;
						printf("user:%d login ...\nNow online users:%d.\n",qid[num],num+1);
						num++;
						tmp.num=num;
						total_num=num;
						tmp.mtype=BID;
						if(num==1)
							msgsnd(qid[0],&tmp,sizeof(struct bid)-sizeof(long),0);
					}
					else if(flag!=-1)
					{
						pthread_cancel(thread1);
						pthread_create(&thread1,NULL,(void*)send,NULL);                
						global_buy.price=tmp.price;
						global_buy.qid=tmp.qid;
						tmp.mtype= BID;      //出價類別
						tmp.num=num;
						printf("qid:%d,price:%d\n",tmp.qid,tmp.price);
						if(price_now >= tmp.price && flag!=-1)   //出價低於目前價錢時，返回給此買家
						{	
							pt2=pt-1;
							if(pt2==-1)
								pt2=num-1;
							tmp.qid=qid[pt2];						
							tmp.price =price_now;
							msgsnd(qid[pt],&tmp,sizeof(struct bid)-sizeof(long),0); 
						}
						else if(price_now < tmp.price && flag!=-1)
						{
							bidding_num=0;     //結標倒數重設
							pt++;
							if(pt==num)
								pt=0;
							price_now=tmp.price;
							for(i=0; i<total_num; i++)
							{
								if(qid[i]==tmp.qid)
									continue;
								msgsnd(qid[i],&tmp,sizeof(struct bid)-sizeof(long),0);
							}
						}
					}
				}
				break;
			case'b':                   /////////////////////////////////////////////買方
				srand( time(NULL) );
				key_t key_buy;
				key_buy=0;
				buy.qid=msgget(key_buy,0600|IPC_CREAT|IPC_EXCL);
				while(buy.qid==-1 || buy.qid==sale.qid)
				{
					buy.qid=msgget(key_buy,0600|IPC_CREAT|IPC_EXCL);
				}
				buy.price=0;
				buy.mtype= LOGIN;               //登入類別
				msgsnd(sale.qid,&buy,sizeof(struct bid)-sizeof(long),0);
				tmp.num=0;
				printf("login ... \n");
				printf("您的Qid為%d.\n",buy.qid);
				global_buy.qid=buy.qid;
				while(1)
				{	
					pthread_create(&thread1,NULL,(void*)receive,NULL);
					flag=msgrcv(buy.qid,&tmp,sizeof(struct bid),BID,0);
					global_sale_obj=tmp.obj;      
					if(flag!=-1)
					{
						buy.mtype= BID;          //出價類別
						if(global_tmp.price==0)
						{
							printf("\n今日競標商品：%s\n目前價錢：%d元\n",tmp.obj,tmp.price);
							if(tmp.price!=0)
								printf("出價者Qid：%d\n",tmp.qid);
							printf("請出價！\n");
						}
						scanf("%d",&buy.price);
						msgsnd(sale.qid,&buy,sizeof(struct bid)-sizeof(long),0);
						if(buy.price <= tmp.price)
						{
							printf("出價低於目前價錢，請重新出價！\n");
						}
						else if(tmp.num>1)
						{
							printf("其他使用者出價中！請稍等...\n");
						}
					}
				}
				break;
			default:
				break;
		}
	}
}

