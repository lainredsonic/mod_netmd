#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/ioctl.h>
#include "netmd.h"
#define TEST_MAGIC 'k'
#define IOCTL_NETMD_STOP _IO(TEST_MAGIC, 0)
#define IOCTL_NETMD_PLAY _IO(TEST_MAGIC, 1)
#define IOCTL_NETMD_TACK _IO(TEST_MAGIC, 2)
#define IOCTL_NETMD_WRIT _IO(TEST_MAGIC, 3)
#define IOCTL_NETMD_INFO _IO(TEST_MAGIC, 4)
#define IOCTL_NETMD_DISC _IO(TEST_MAGIC, 5)
#define IOCTL_NETMD_GROUP _IO(TEST_MAGIC, 6)
#define IOCTL_NETMD_TRAK _IO(TEST_MAGIC, 7)
//#define IOCTL_NETMD_DISC_TITLE _IO(TEST_MAGIC, 8)
//#define	IOCTL_NETMD_GROUP_TITLE _IO(TEST_MAGIC, 9)
#define IOCTL_NETMD_POS _IO(TEST_MAGIC,10)
#define IOCTL_NETMD_SET_TIE _IO(TEST_MAGIC,11)

/*typedef struct netmd_prop{
	unsigned char hex;
	char *name;
}netmd_prop_t;

typedef struct {
	int header_length;
	struct netmd_group *groups;
	int group_count;
}minidisc;

netmd_prop_t const codecs[]={
	{0x00,"ATRAC"},
	{0x03,"ATRAC3"},
	{0,0}
};

netmd_prop_t const bitrates[]={
	{0x90,"STEREO"},
	{0x92,"LP2"},
	{0x93,"LP4"},
	{0,0}
};

netmd_prop_t const unknow_prop={0x00,"UNKNOWN"};
*/


static void get_disc_info(char *rawheader){
	int header_size=255;
	char *tok=NULL;
	char *next_tok=NULL;
	int group_count=0;
	if(header_size!=0){
		tok=rawheader;
		next_tok=strstr(rawheader,"//");
		while(next_tok){
			*next_tok+=2;
			group_count++;
			tok=next_tok;
		}
	}
}

static char * format_dinfo(char *rawheader){
	char *next_tok=NULL;
	char *tok=NULL;
	int group_count=0;
	int rawh_size=255;
	if(rawh_size!=0){
		tok=rawheader;
		next_tok=strstr(rawheader,"//");
		while(next_tok){
			next_tok+=2;
			group_count++;
			tok=next_tok;
			next_tok=strstr(tok,"//");
		}
		
	}
}



int main(int argc,char *argv[])
{
        int fd;
	int i;
	int total_tracks=0;
	int *track=malloc(sizeof(int));
	char *rawheader=malloc(255);
	struct netmd_tracks *c_netmd_tracks;
	struct netmd_groups *c_netmd_groups;
	struct netmd_disc *c_netmd_disc;
	struct pos_time *cur_pos=malloc(sizeof(struct pos_time));
	struct netmd_tracks *mod_track;

	if(argc==1){
		printf("please inter command\n");
		exit;
	}
	printf("cmd :%s\n",argv[1]);
        fd = open("/dev/netmd0", O_RDWR);
        if (fd == -1)
                goto err;
	switch(argv[1][0]){
		case 'p':
			if(argc==3)
				*track=atoi(argv[2]);
        			if (ioctl(fd, IOCTL_NETMD_PLAY,track) == -1){
                			perror("ioctl IOCTL_NETMD_PLAY");
				}
			break;
        		if (ioctl(fd, IOCTL_NETMD_PLAY)== -1){
				perror("ioctl IOCTL_NETMD_PLAY");
			}	
		break;
		case 's':
			if (ioctl(fd, IOCTL_NETMD_STOP) == -1){
				perror("ioctl IOCTL_NETMD_STOP");
			}
		break;
		case 'q':
			if (ioctl(fd,IOCTL_NETMD_TACK,track) == -1){
				perror("ioctl IOCTL_NETMD_TACK");
				break;
			}
			printf("track no. :%d\n",*track+1);
			if (ioctl(fd,IOCTL_NETMD_POS,cur_pos) == -1){
				perror("ioctl IOCTL_NETMD_POS");
				break;
			}
			printf("track pos:%d:%d:%d\n",cur_pos->min,cur_pos->sec,cur_pos->ten);
			
		break;
		case 'i':
			if (ioctl(fd,IOCTL_NETMD_INFO,rawheader) == -1){
				perror("ioctl IOCTL_NETMD_INFO");
				break;
			}
			printf("disc_header:%s\n",rawheader);
		break;
		case 'd':
//////////////////////////////////////////////////////////////////////////
			c_netmd_disc=malloc(sizeof(struct netmd_disc));
			c_netmd_disc->disc_title=malloc(255);
			if (ioctl(fd,IOCTL_NETMD_DISC,c_netmd_disc) == -1){
				perror("ioctl IOCTL_NETMD_DISC");
				break;
			}
			/*if (ioctl(fd,IOCTL_NETMD_DISC_TITLE,c_netmd_disc->disc_title)==-1){
				perror("ioctl IOCTL_NETMD_DISC_TITLE");
			}
			*/
			printf("disc_title:%s\n",c_netmd_disc->disc_title);
			printf("total groups:%d\n",c_netmd_disc->group_count);
			printf("total tracks:%d\n",c_netmd_disc->track_count);
//////////////////////////////////////////////////////////////////////////
			c_netmd_groups=malloc(sizeof(struct netmd_groups)*(c_netmd_disc->group_count));
			c_netmd_tracks=malloc(sizeof(struct netmd_tracks)*(c_netmd_disc->track_count));
			for(i=0;i<c_netmd_disc->group_count;i++){
				(c_netmd_groups+i)->group_title=malloc(255);
			}
			for(i=0;i<c_netmd_disc->track_count;i++){
				(c_netmd_tracks+i)->tname=malloc(255);	
				(c_netmd_tracks+i)->codec=malloc(10);
				(c_netmd_tracks+i)->bitrate=malloc(10);
			}
			if (ioctl(fd,IOCTL_NETMD_GROUP,c_netmd_groups)==-1){
				perror("ioctl IOCTL_NETMD_DISC_GROUP");
				break;
			}
			c_netmd_disc->netmd_groups=c_netmd_groups;
			c_netmd_disc->netmd_tracks=c_netmd_tracks;
			for(i=0;i<c_netmd_disc->group_count;i++){
				//(c_netmd_groups+i)->group_title=malloc(255);
			//	total_tracks+=(c_netmd_disc->netmd_groups+i)->end_track-(c_netmd_disc->netmd_groups+i)->start_track+1;
				printf("disc_group_index:%d\tgroup_title:%s\tstart_track:%d\tend_track:%d\n",i,(c_netmd_disc->netmd_groups+i)->group_title,(c_netmd_disc->netmd_groups+i)->start_track,(c_netmd_disc->netmd_groups+i)->end_track);
			}
			/*if(ioctl(fd,IOCTL_NETMD_GROUP_TITLE,c_netmd_groups)==-1){
				perror("ioctl IOCTL_NETMD_GROUP_TITLE");
			}
			for(i=0;i<c_netmd_disc->group_count;i++){
				printf("disc_group_index:%d,group_title:%s\n",i,(c_netmd_disc->netmd_groups+i)->group_title);
			}
			*/
///////////////////////////////////////////////////////////////////////////
			if (ioctl(fd,IOCTL_NETMD_TRAK,c_netmd_tracks)==-1){
				perror("ioctl IOCTL_NETMD_TRAK");
				break;
			}
			for(i=0;i<c_netmd_disc->track_count;i++){
				printf("disc_track_index:%d\ttrack_title:%s\ttrack_codec:%s\ttrack_bitrate:%s\t%d:%d:%d\n",i,(c_netmd_disc->netmd_tracks+i)->tname,(c_netmd_disc->netmd_tracks+i)->codec,(c_netmd_disc->netmd_tracks+i)->bitrate,(c_netmd_disc->netmd_tracks+i)->minute,(c_netmd_disc->netmd_tracks+i)->second,(c_netmd_disc->netmd_tracks+i)->tenth);
			}
		break;
		case 't':
			if(argv[2]&&argv[3]){
			mod_track=malloc(sizeof(struct netmd_tracks));
			mod_track->index=atoi(argv[2]);
			mod_track->tname=strdup(argv[3]);
			if(ioctl(fd,IOCTL_NETMD_SET_TIE,mod_track)==-1){		
				perror("ioctl IOCTL_NETMD_SET_TIE");
				printf("Write new title error\n");
				break;
			}else{
				printf("Write new title success\n");
			}
			free(mod_track);
			}
			
		break;
/*		case 'w':
			
			if(ioctl(fd,IOCTL_NETMD_WRIT,buf)){
				perror("ioctl IOCTL_NETMD_WRITE");
			}
		break;
*/
		
	}
        return 0;
err:
	printf("device file open failed\n");
        return -1;
	if(close(fd)==-1)
		printf("close I/O failed!\n");
}
