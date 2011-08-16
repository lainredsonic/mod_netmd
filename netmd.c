/*
 *USB driver for SONY HiMD RH-1 (only NETMD mode)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
//#include <linux/math64.h>
#include <linux/parser.h>
#include "netmd.h"
/*
#define BCD_TO_PROPER(x) ((((x)&0xf0)>>4)*10+((x)&0x0f))
#define USB_MD_VENDOR_ID 0x054c
#define USB_MD_PRODUCT_ID 0x0286

#define to_netmd_dev(d) container_of(d, struct netmd_device, netmd_kref)
struct netmd_device;
static int netmd_stop(struct netmd_device *);
static int netmd_playback_control(struct netmd_device *,unsigned char); 
static int netmd_set_track(struct netmd_device *,int );
static int netmd_set_title(struct netmd_device *netmd,int track,char *buffer,int size);
static int netmd_get_current_track(struct netmd_device *);
static int request_disc_rawheader(struct netmd_device *mdev,char *buffer,int size);

//////////////////NetMD track properites
struct netmd_pair{
	unsigned char hex;
	char *t_name;
};

struct netmd_pair const codecs[] = 
{
	{0x00, "ATRAC"},
	{0x03, "ATRAC3"},
	{0, 0} // terminating pair 
};

struct netmd_pair const bitrates[] = 
{
	{0x90, "Stereo"},
	{0x92, "LP2"},
	{0x93, "LP4"},
	{0, 0} // terminating pair
};
struct netmd_pair const unknown_pair={0x00,"UNKNOWN"};
*/
/*
struct netmd_pair const* find_pair(int hex, struct netmd_pair const* array)
{
	int i = 0;
	for(; array[i].t_name != NULL; i++)
	{
		if(array[i].hex == hex)
			return &array[i];
	}

	return &unknown_pair;
}

/////////////////////////NetMD track properites////
*/
static const struct usb_device_id netmd_type_table[] = {
	{ USB_DEVICE(USB_MD_VENDOR_ID,USB_MD_PRODUCT_ID)},
	{USB_DEVICE(0x04dd,0x9014)},
	{}
};
MODULE_DEVICE_TABLE(usb, netmd_type_table);

/*
////////////////////////NetMD disc and track infomation
struct netmd_tracks{
	int index;
	int minute;
	int second;
	int tenth;
	int ogroup;
	char *tname;
	char *codec;
	char *bitrate;
};

struct netmd_groups{
	int start_track;
	int end_track;
	char * group_title;
};

struct netmd_disc {
	struct netmd_groups *netmd_groups;
	char *disc_title;
	int group_count;
	int disc_header_len;
};

*/
////////////////////////NetMD disc and track infomation////////

#define USB_NETMD_MINOR_BASE 192
struct netmd_device {
	struct usb_device *udev;
	struct usb_interface *netmd_if;
	int num_endpoint;
	struct netmd_disc *netmd_disc_info;
	struct mutex io_mutex;
	struct kref netmd_kref;
	size_t bulk_in_size;
	__u8   bulk_in_endpointAddr;
	__u8   bulk_out_endpointAddr;
	unsigned char *bulk_in_buffer;
};
static struct usb_driver netmd_driver;

//////////////////////////device file operator
static int netmd_open(struct inode *inode,struct file *file){
	struct netmd_device *mdev;
	struct usb_interface *interface;
	int subminor;
	int retvar = 0;
	printk(KERN_INFO "netmd_device open\n");
	subminor=iminor(inode);
	interface=usb_find_interface(&netmd_driver,subminor);
	if(!interface){
		err("%s -error ,can't find device for minor %d",__func__,subminor);
		retvar= -ENODEV;
		goto exit;
	}
	mdev=usb_get_intfdata(interface);
	if(!mdev){
		retvar= -ENODEV;
		goto exit;
	}
	kref_get(&mdev->netmd_kref);	
	mutex_lock(&mdev->io_mutex);
	file->private_data=mdev;
	mutex_unlock(&mdev->io_mutex);
exit:
	return retvar;
}

static void waitforsync(struct netmd_device  *mdev)
{
	char syncmsg[4];
	dev_info(&mdev->udev->dev,"wait for sync...");
	do{
		usb_control_msg(mdev->udev,usb_rcvctrlpipe(mdev->udev,0),0x01,0x80|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,0,0,syncmsg,0x04,5000);
	}while(memcmp(syncmsg,"\0\0\0\0",4)!=0);
}

static char* sedcmd(struct netmd_device *mdev,char* str , int len,char* response,int rlen){
	int i;
	int ret;
	int size;
	char size_request[4];
	char* buf;
	
	waitforsync(mdev);
	dev_info(&mdev->udev->dev,"prepare for track write cmd...");
	ret=usb_control_msg(mdev->udev,usb_sndctrlpipe(mdev->udev,0),0x80,0x00|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,0,0,str,len,800);	
	if(ret<0){
		dev_info(&mdev->udev->dev,"sedcmd bad ret code,return early");
		return NULL;
	}
	usb_control_msg(mdev->udev,usb_rcvctrlpipe(mdev->udev,0),0x01,0x80|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,0,0,size_request,0x04,500);
	size=size_request[2];
	if(size<1){
		dev_info(&mdev->udev->dev,"sedcmd:invalid size ,ignoring");
		return NULL;
	}
	buf=kmalloc(size,GFP_KERNEL);
	usb_control_msg(mdev->udev,usb_rcvctrlpipe(mdev->udev,0),0x81,0x80|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,0,0,buf,size,500);
	switch(buf[0]){
		case 0x0f:dev_info(&mdev->udev->dev,"Success for record like command");break;
		case 0x0c:dev_info(&mdev->udev->dev,"**Unknow Header");break;
		case 0x09:dev_info(&mdev->udev->dev,"Command successful");break;
		case 0x08:dev_info(&mdev->udev->dev,"**Unknwo command");break;
		case 0x0a:dev_info(&mdev->udev->dev,"***Error on recode");break;
		default:dev_info(&mdev->udev->dev,"***Unknow return code");break;		
	}
	if(response!=NULL){
		int c=0;
		for(i=0;i<min(rlen,size);i++){
			if(response[i]!=buf[i])
				c++;
		}
	dev_info(&mdev->udev->dev,"Differ:%d",c);
	}
	return buf;
	
}

static int netmd_release(struct inode *inode,struct file *file){

}
static int netmd_read(struct file *file,char *buffer,size_t count,loff_t *ppos){
}
static int netmd_write(struct file *file,const char *user_buffer ,size_t count,loff_t *ppos){
	struct netmd_device *mdev;
	char begintitle[] = {0x00, 0x18, 0x08, 0x10, 0x18, 0x02, 0x03, 0x00}; /* Some unknown command being send before titling */
	
	char endrecord[] =  {0x00, 0x18, 0x08, 0x10, 0x18, 0x02, 0x00, 0x00};  /* Some unknown command being send after titling */
	char fintoc[] =     {0x00, 0x18, 0x00, 0x08, 0x00, 0x46, 0xf0, 0x03, 0x01, 0x03, 0x48, 0xff, 0x00, 0x10, 0x01, 0x00, 0x25, 0x8f, 0xbf, 0x09, 0xa2, 0x2f, 0x35, 0xa3, 0xdd}; /* Command to finish toc flashing */
	char movetoendstartrecord[] = {0x00, 0x18, 0x00, 0x08, 0x00, 0x46, 0xf0, 0x03, 0x01, 0x03, 0x28, 0xff, 0x00, 0x01, 0x00, 0x10,0x01, 0xff, 0xff, 0x00, 0x94, 0x02, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x04, 0x98};   /* Record command */
	char movetoendresp[] = {0x0f, 0x18, 0x00, 0x08, 0x00, 0x46, 0xf0, 0x03, 0x01, 0x03, 0x28, 0x00, 0x00, 0x01, 0x00, 0x10, 0x01, 0x00, 0x11, 0x00, 0x94, 0x02, 0x00, 0x00,0x43, 0x8c, 0x00, 0x32, 0xbc, 0x50}; /* The expected response from the record command. */
	char header[] =      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0xd4, 0x4b, 0xdc, 0xaa, 0xef, 0x68, 0x22, 0xe2}; /* Header to be inserted at every 0x3F10 bytes */
	//	char debug[]  =      {0x4c, 0x63, 0xa0, 0x20, 0x82, 0xae, 0xab, 0xa1};
	char *buf=NULL;
	char track_number='\0';
	int tmp_div;
	int omg_data_size=0;
	int omg_size=0;
	char* p=NULL;
	char size_request[4];
	unsigned int size;
	int OMG_SIZE_INFO_OFFSET=0x56;
	unsigned char *data=kmalloc(4096,GFP_KERNEL);
	char* ret_sedcmd;
	int* act_len_bulk_write={0};
	int ret=0;
	mdev=file->private_data;
	
	if(user_buffer==NULL){
		dev_info(&mdev->udev->dev,"OMG file size Zero");
	}
	omg_data_size=*(OMG_SIZE_INFO_OFFSET+user_buffer+0)<<24;
	omg_data_size+=*(OMG_SIZE_INFO_OFFSET+user_buffer+1)<<16;
	omg_data_size+=*(OMG_SIZE_INFO_OFFSET+user_buffer+2)<<8;
	omg_data_size+=*(OMG_SIZE_INFO_OFFSET+user_buffer+3);
	dev_info(&mdev->udev->dev,"OMG size:%d",omg_data_size);
	tmp_div=omg_data_size;
	//do_div(tmp_div,0x3f18);
	//omg_size=tmp_div*8+omg_data_size+8;
	omg_size=((long)omg_data_size/0x3f18)*8+(long)omg_data_size+8;  //plus number of data headers
	dev_info(&mdev->udev->dev,"total send byte:%d",omg_size);
	
	/********** Fill in information in start record command and send ***********/
	// not sure if size is 3 of 4 bytes in rec command...
	movetoendstartrecord[27]=(omg_size >> 16) & 0xff;
	movetoendstartrecord[28]=(omg_size >> 8) & 0xff;
	movetoendstartrecord[29]=omg_size & 0xff;
	
	ret_sedcmd = sedcmd(mdev,movetoendstartrecord,30,movetoendresp,0x1e);
	track_number = ret_sedcmd[0x12];
	kfree(ret_sedcmd); // clear the result
	
	/********** Prepare to send data*************************/
	omg_data_size+=90;
	*ppos+=90;
	waitforsync(mdev);

	/********** Send data**********/
	while (ret>=0){
		int bytes_to_send;  //the number of bytes that needs to send in this round
		int __bytes_left;   //the number of bytes left in the user buffer
		int __chunk_size;   //the number of bytes left in the 0x1000 chunk to send
		int __distance_to_header; //how far till the next header insert
		
		dev_info(&mdev->udev->dev,"pos:%d/%d",(int)*ppos,omg_data_size);
		if(*ppos>=omg_data_size){
			dev_info(&mdev->udev->dev,"Track write Done!");
			kfree(data);
			break;
		}

		__bytes_left=omg_data_size-*ppos;
		__chunk_size=min(0x1000,__bytes_left);
		tmp_div=*ppos-0x5a;
		//__distance_to_header=do_div(tmp_div,0x3f10);
		__distance_to_header=((long)*ppos-0x5a)%0x3f10;
		if(__distance_to_header!=0)
			__distance_to_header=0x3f10-__distance_to_header;
		bytes_to_send=__chunk_size;
		dev_info(&mdev->udev->dev,"Chunksize: %d\n",__chunk_size);
		dev_info(&mdev->udev->dev,"distance_to_header: %d\n",__distance_to_header);
		dev_info(&mdev->udev->dev,"Bytes left: %d\n",__bytes_left);
		
		if(__distance_to_header<=0x1000){
			dev_info(&mdev->udev->dev,"Inserting header\n");
			if(__chunk_size<0x1000){
				__chunk_size+=0x10;
				bytes_to_send=__chunk_size-0x08;
			}
			copy_from_user(data,user_buffer,__distance_to_header);
			*ppos+=__distance_to_header;
			__chunk_size-=__distance_to_header;
			p=data+__distance_to_header;
			memcpy(p,header,16);
			__bytes_left=min(0x3f00,__bytes_left-__distance_to_header-0x10);
			dev_info(&mdev->udev->dev,"bytes left in chunk:%d",__bytes_left);
			p[6]=__bytes_left>>8;
			p[7]=__bytes_left&255;
			__chunk_size-=0x10;
			p+=0x10;
			*ppos+=8;
			copy_from_user(data,user_buffer,__chunk_size);
			*ppos+=__chunk_size;
		}else {
			if(copy_from_user(data,user_buffer,__distance_to_header)==0){
				kfree(data);
				break;
			}
		}
		dev_info(&mdev->udev->dev,"Sending %d bytes to md",bytes_to_send);
		ret=usb_bulk_msg(mdev->udev,usb_sndbulkpipe(mdev->udev,0x02),(char*)data,bytes_to_send,act_len_bulk_write,5000);
	}
	if(ret<0){
	kfree(data);
	return ret;
	}
	/**********************End transfer wait for unit ready*******************/
	dev_info(&mdev->udev->dev,"Waiting for done:");
	do{
		usb_control_msg(mdev->udev,usb_sndctrlpipe(mdev->udev,0x0),0x01,0xc1,0,0,size_request,0x04,5000);
	}while(memcpy(size_request,"\0\0\0\0",4)==0);
	
	dev_info(&mdev->udev->dev,"Recieving response:");
	size=size_request[2];
	if(size <1){
		dev_info(&mdev->udev->dev,"Invalid size");
		return -1;
	}	
	buf=kmalloc(size,GFP_KERNEL);
	usb_control_msg(mdev->udev,usb_rcvctrlpipe(mdev->udev,0x0),0x81,0xc1,0,0,buf,size,500);
	kfree(buf);

	/**********************Title the transfered song***************************/
	buf=sedcmd(mdev,begintitle,8,NULL,0);
	kfree(buf);
	dev_info(&mdev->udev->dev,"Rename track %d",track_number);
	netmd_set_title(mdev,track_number,"new track",9);
	buf=sedcmd(mdev,endrecord,8,NULL,0);
	kfree(buf);
	
	/***********************End TOC Edit*************************************/
	ret=usb_control_msg(mdev->udev,usb_sndctrlpipe(mdev->udev,0x0),0x80,0x41,0,0,fintoc,0x19,800);
	dev_info(&mdev->udev->dev,"waiting for done :");
	do{
		usb_control_msg(mdev->udev,usb_rcvctrlpipe(mdev->udev,0x0),0x01,0xc1,0,0,size_request,0x04,5000);
	}while(memcmp(size_request,"\0\0\0\0",4)==0);
	
	return ret;
	
}

static int netmd_flush(struct file *file,fl_owner_t id){
}


static long netmd_ioctl(struct file *file,unsigned int cmd,unsigned long arg){	
	int i;
	struct netmd_device *mdev;
	char *buf=kmalloc(255,GFP_KERNEL);
	char *track=kmalloc(sizeof(char),GFP_KERNEL);
	char *p=kmalloc(255,GFP_KERNEL);
	int group_index=0;
	int track_index=0;
	mdev=file->private_data;
	if (cmd==_IO('k',0)){
		netmd_stop(mdev);
		printk(KERN_INFO "netmd stop\n");
	}
	if (cmd==_IO('k',1)){
		if(arg){
			if(copy_from_user(track,(void *)arg,1))
				return -EFAULT;
			printk(KERN_INFO "set track:%d\n",*track);
			netmd_set_track(mdev,*track);
		}
		netmd_playback_control(mdev,0x75);
		printk(KERN_INFO "netmd play\n");
	}
	if(cmd==_IO('k',2)){
		*track=netmd_get_current_track(mdev);
		dev_info(&mdev->udev->dev,"current track:%d",*track);
		if(copy_to_user((void *)arg,track,1))
			return -EFAULT;
	}
/*	if (cmd==_IO('k',3)){
		if(!arg)
			dev_info(&mdev->udev->dev,"netmd_ioctl_write:missing buffer");
	}
*/
	if (cmd==_IO('k',4)){
		if(arg){
			request_disc_rawheader(mdev,buf,255);	
			//if(copy_to_user((void *)arg,mdev->netmd_disc_info,sizeof(mdev->netmd_disc_info)))
			if(copy_to_user((void *)arg,buf,255))
				return -EFAULT;
		}
	}
	if (cmd==_IO('k',5)){  //get netmd_disc
		if(arg){
			if(copy_to_user((struct netmd_disc*)arg,mdev->netmd_disc_info,sizeof(struct netmd_disc)))
				return -EFAULT;
		}
	}
	if (cmd==_IO('k',6)){ //get netmd_groups
		if(arg){
			if(copy_to_user((struct netmd_groups*)arg,mdev->netmd_disc_info->netmd_groups,sizeof(struct netmd_groups)*(mdev->netmd_disc_info->group_count)))
				return -EFAULT;
		}
	}
	if (cmd==_IO('k',7)){ //get netmd_track
		if(arg){
		//	for(group_index=0;group_index<mdev->netmd_disc_info->group_count;group_index++)
		//	if(copy_to_user((struct netmd_tracks*)arg,(mdev->netmd_disc_info->netmd_groups+group_index)))
		}
	}
	if (cmd==_IO('k',8)){ //get netmd_disc->disc_title
		if(arg){
			if(copy_to_user((char *)arg,mdev->netmd_disc_info->disc_title,255))
				return -EFAULT;
		}
	}
	if (cmd==_IO('k',9)){ //get netmd_disc->netmd_groups->group_title
		if(arg){
			for(i=0;i<mdev->netmd_disc_info->group_count;i++){
				if(copy_to_user(((struct netmd_groups *)arg+i)->group_title,(mdev->netmd_disc_info->netmd_groups+i)->group_title,255))
					return -EFAULT;
			//dev_info(&mdev->udev->dev,"[debug]group name:%s",arg+8+i*sizeof(struct netmd_groups));
			}
		}
	}
	return 0;
}

static const struct file_operations netmd_fops = {
	.owner = THIS_MODULE,
	.open = netmd_open,
	.write = netmd_write,
	.read = netmd_read,
	.flush = netmd_flush,
	.release = netmd_release,
	.unlocked_ioctl= netmd_ioctl,
};
//////////////////////////device file operator

static struct usb_class_driver netmd_class = {
	.name = "netmd%d",
	.fops = &netmd_fops,
	.minor_base = USB_NETMD_MINOR_BASE,
};


static void netmd_delete(struct kref *kref)
{
	struct netmd_device *mdev = to_netmd_dev(kref);
	usb_put_dev(mdev->udev);
	kfree(mdev->bulk_in_buffer);
	kfree(mdev);
}
static int netmd_poll(struct netmd_device *mdev,unsigned char *buf,int tries){
	int i;

	for (i = 0; i < tries; i++) {
		/* send a poll message */
		memset(buf, 0, 4);
		if (usb_control_msg(mdev->udev,usb_rcvctrlpipe(mdev->udev,0) ,0x01 , 0x80 | USB_TYPE_VENDOR |USB_RECIP_INTERFACE,  0, 0, (char*) buf, 4,1000) < 0) {
			dev_info(&mdev->udev->dev,"netmd_poll: usb_control_msg failed");
			return -1;
		}
		if (buf[0] != 0) {
			break;
		}
		if (i > 0) {
			ssleep(1);
		}
	}	
	return buf[2];	
}

static int netmd_exch_message(struct netmd_device *mdev,unsigned char *cmd,int cmdlen,unsigned char *rsp){
	unsigned char	pollbuf[4];
	int		len;
	/* poll to see if we can send data */
	len = netmd_poll(mdev, pollbuf, 1);
	if (len != 0) {
		dev_info(&mdev->udev->dev,"netmd_exch_message: netmd_poll failed");
		return (len > 0) ? -2 : len;
	}		
	dev_info(&mdev->udev->dev,"netmd_poll ret:%d",len);
	if(usb_control_msg(mdev->udev,usb_sndctrlpipe(mdev->udev,0),0x80,0x00|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,0,0,(char*)cmd,cmdlen,1000)<0){
		dev_info(&mdev->udev->dev,"usb_control_msg cmd 0x80 snd failed!");
		return -1;
	}
	do{
		len = netmd_poll(mdev,pollbuf,30);
		if (len<=0){
			dev_info(&mdev->udev->dev,"netmd_poll cmd 0x80 failed!");	
			return (len==0)?-3 :len;
		}
		if(usb_control_msg(mdev->udev,usb_rcvctrlpipe(mdev->udev,0),(char)pollbuf[1],0x80|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,0,0,(char*)rsp,len,1000)<0){
			dev_info(&mdev->udev->dev,"usb_control_msg cmd 0x80 rcv failed");	
			return -1;
		}
	}while(rsp[0]==0x0f);
	return len;
}

char* find_pair(int hex,struct netmd_pair const* array){
	int i=0;
	for(;array[i].t_name!=NULL;i++){
		if(array[i].hex==hex)
			return array[i].t_name;
	}
	return unknow_pair.t_name;
}

static void set_track_data(struct netmd_device *mdev,struct netmd_tracks *netmd_tracks){
	int track_index;
	int title_size=1;
	unsigned char codec_id,bitrate_id;
	//for(track_index=0;track_index<mdev->netmd_disc_info->track_count;track_index++){
	for(track_index=0;title_size>=0;track_index++){
		(netmd_tracks+track_index)->tname=kmalloc(255,GFP_KERNEL);
		title_size=netmd_request_track_title(mdev,track_index,(netmd_tracks+track_index)->tname,255);
		if(title_size>=0){
			netmd_request_track_codec(mdev,track_index,(unsigned char*)&codec_id);
			netmd_request_track_bitrate(mdev,track_index,(unsigned char*)&bitrate_id);
			(netmd_tracks+track_index)->codec=find_pair(codec_id,codecs);
			(netmd_tracks+track_index)->bitrate=find_pair(bitrate_id,bitrates);
			mdev->netmd_disc_info->track_count=track_index+1;
			dev_info(&mdev->udev->dev,"[debug]track title:%s\tcodec:%s\tbitrate:%s",(netmd_tracks+track_index)->tname,(netmd_tracks+track_index)->codec,(netmd_tracks+track_index)->bitrate);
		}
	}
}

static void format_disc_info(struct netmd_device *mdev,char *disc_rawheader){
		char *next_tok=NULL;
		char *tok=NULL;
		int group_count=0;
		int rawh_size=256;
		int track;
		char *semicolon,*hyphen,*name;
		int start,end;
		struct netmd_tracks *netmd_tracks;

		if(rawh_size!=0){
			tok=disc_rawheader;
			next_tok=strstr(disc_rawheader,"//");
			while(next_tok){
				next_tok+=2;
				group_count++;
				tok=next_tok;
				next_tok=strstr(tok,"//");
			}
		}
/////////////////////////////////////////////////////////////////////////////////////////////////
		if(!group_count)
			group_count++; //at lest one group in disc : for disc title
		//mdev->netmd_disc_info->track_count=0;
		mdev->netmd_disc_info->group_count=group_count;
		mdev->netmd_disc_info->netmd_groups=kmalloc(sizeof(struct netmd_groups)*(mdev->netmd_disc_info->group_count),GFP_KERNEL);
		memset(mdev->netmd_disc_info->netmd_groups,0,sizeof(struct netmd_groups)*(mdev->netmd_disc_info->group_count));
		group_count=0;
////////////////////////////////////////////////////////////////////////////////////////////////
		if(mdev->netmd_disc_info->disc_header_len){
			track=simple_strtol(disc_rawheader,NULL,10);
			tok=disc_rawheader;
			next_tok=strstr(disc_rawheader,"//");	
			while(next_tok){
				*next_tok='\0';
				next_tok+=2;
				semicolon=strchr(tok,';');
				if((!track&&group_count==0)&&tok[0]!=';'){
					if(semicolon==NULL){
						name=tok;
						start=0;
						end=0;
						set_group_data(mdev,0,name,start,end);
					}
					else{
						if(!semicolon){
							name=kstrdup("<Untitled>",GFP_KERNEL);
							start=0;
							end=0;
							set_group_data(mdev,0,name,start,end);
							group_count++;
							name=semicolon+1;
							set_group_data(mdev,group_count,name,start,end);
						}else{
							name=semicolon+1;
							set_group_data(mdev,0,name,0,0);
						}	
					}
				}else{
					if(!group_count){
						name=kstrdup("<Untitled>",GFP_KERNEL);
						start=0;
						end=0;
						set_group_data(mdev,0,name,start,end);
						group_count++;
						continue;
					}
					name=semicolon+1;
					hyphen=strchr(tok,'-');
					if(hyphen){
						*hyphen='\0';
						//start=atoi(tok);
						start=simple_strtol(tok,NULL,10);
						//end=atoi(hyphen+1);
						end=simple_strtol(hyphen+1,NULL,10);
					}else{
						start=simple_strtol(tok,NULL,10);
						end=start;
					}
					if(end<start){
						dev_info(&mdev->udev->dev,"info format parse error");
						return -1;
					}
					set_group_data(mdev,group_count,name,start,end);
				}
				group_count++;
				tok=next_tok;
				next_tok=strstr(tok,"//");
			}
		//mdev->netmd_disc_info->track_count--; //except group 0 track
		netmd_tracks=kmalloc(sizeof(struct netmd_tracks)*99,GFP_KERNEL);
		set_track_data(mdev,netmd_tracks);
		dev_info(&mdev->udev->dev,"total tracks:%d",mdev->netmd_disc_info->track_count);
		
		}

	return ;
}
static void set_group_data(struct netmd_device *mdev,int group_count,char* name,int start,int end){
	dev_info(&mdev->udev->dev,"[debug]:group title:%s \n start:%d \n end:%d\n\n\n",name,start,end);
	//mdev->netmd_disc_info->track_count+=end-start+1;
	if(group_count==0){
		mdev->netmd_disc_info->disc_title=kstrdup(name,GFP_KERNEL);
	}
	mdev->netmd_disc_info->netmd_groups[group_count].group_title=kstrdup(name,GFP_KERNEL);
	mdev->netmd_disc_info->netmd_groups[group_count].start_track=start;
	mdev->netmd_disc_info->netmd_groups[group_count].end_track=end;
	return ;
}
int netmd_request_track_time(struct netmd_device *mdev,int track,struct netmd_tracks *netmd_track){
	int ret=0;
	int size=0;
	char request[]={0x00,0x18,0x06,0x02,0x20,0x10,0x01,0x00,0x01,0x30,0x00,0x01,0x00,0xff,0x00,0x00,0x00,0x00,0x00};
	char time_request[255];
	
	request[8]=track;
	ret=netmd_exch_message(mdev,(unsigned char*)request,0x13,(unsigned char*)time_request);
	if(ret<0){
		dev_info(&mdev->udev->dev,"request_track_time: bad ret code, returning early");
		return 0;
	}
	size=ret;
	netmd_track->minute=BCD_TO_PROPER(time_request[28]);
	netmd_track->second=BCD_TO_PROPER(time_request[29]);
	netmd_track->tenth=BCD_TO_PROPER(time_request[30]);
	netmd_track->index=track;
	return ret;
}

int * netmd_get_playback_position(struct netmd_device *mdev){
	int ret=0;
	char request[] = {0x00, 0x18, 0x09, 0x80, 0x01, 0x04, 0x30, 0x88, 0x02, 0x00, 0x30, 0x88, 0x05, 0x00, 0x30, 0x00, 0x03, 0x00, 0x30, 0x00, 0x02, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00};
	int position[3]={0,0,0};
	char buf[255];
	ret=netmd_exch_message(mdev,request,28,buf);
	position[0]=BCD_TO_PROPER(buf[38]);
	position[1]=BCD_TO_PROPER(buf[39]);
	position[2]=BCD_TO_PROPER(buf[40]);
	if(position[0]||position[1]||position[2])
		return position;
	else
		return 0;
}

int netmd_request_track_codec(struct netmd_device *mdev,int track,unsigned char *codec_id ){
	int ret=0;
	int size=0;
	char request[]={0x00,0x18,0x06,0x01,0x20,0x10,0x01,0x00,0xdd,0xff,0x00,0x00,0x01,0x00,0x08};	
	char reply[255];
	request[8]=track;
	ret=netmd_exch_message(mdev,request,0x13,reply);
	if(ret<0){
		dev_info(&mdev->udev->dev,"request_track_codec:bad ret code ,returning early");
		return 0;
	}
	size=ret;
	*codec_id=reply[size-1];
	return ret;
}

int netmd_request_track_bitrate(struct netmd_device *mdev,int track,unsigned char *bitrate_id){
	int ret=0;
	int size=0;
	char request[]={0x00,0x18,0x06,0x02,0x20,0x10,0x01,0x00,0xdd,0x30,0x80,0x07,0x00,0xff,0x00,0x00,0x00,0x00,0x00};
	unsigned char reply[255];
	request[8]=track;
	ret=netmd_exch_message(mdev,request,0x13,reply);
	if(ret<0){
		dev_info(&mdev->udev->dev,"request_track_bitrate:bad ret code ,returning early");	
		return 0;
	}
	size=ret;
	*bitrate_id=reply[size-2];
	return ret;
}

int netmd_request_track_title(struct netmd_device *mdev,int track ,char *buffer,int size){
	int ret=-1;
	int title_size=0;
	char title_request[0x13] = {0x00, 0x18, 0x06, 0x02, 0x20, 0x18, 0x02, 0x00, 0x00, 0x30, 0x00, 0xa, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00};
	char title[255];

	title_request[8]=track;
	ret=netmd_exch_message(mdev,(unsigned char *)title_request,0x13,(unsigned char*)title);
	if(ret<0){
		dev_info(&mdev->udev->dev,"request_track_title:bad ret code ,returning early ");
		return -1;
	}
	title_size=ret;
	if(title_size==0||title_size==0x13)
		return -1;
	if(title_size>size){
		dev_info(&mdev->udev->dev,"netmd_request_title:title to large for buffer");
		return -1;
	}
	memset(buffer,0,size);	
	if(title_size-25==0){
		//buffer=kstrdup("<untitled>",GFP_KERNEL);
		strncpy(buffer,"<untitled>",255);
	}else{
		strncpy(buffer,(title+25),title_size-25);
	}
	return title_size-25;
	
}

static  int request_disc_rawheader(struct netmd_device *mdev,char *buffer,int size){
	char title_request[0x13]={0x00, 0x18, 0x06, 0x02, 0x20, 0x18, 0x01, 0x00, 0x00, 0x30, 0x00, 0xa, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00};
	char title[255];
	int retval=-1;
	int title_size=0;
	retval = netmd_exch_message(mdev, (unsigned char*) title_request, 0x13, (unsigned char*) title);
	if(retval < 0)
	{
		dev_info(&mdev->udev->dev,"request_disc_title: bad ret code, returning early");
		return 0;
	}
	
	title_size = retval;
	
	if(title_size == 0 || title_size == 0x13)
		return -1; /* bail early somethings wrong */

	if(title_size > 256)
	{
		dev_info(&mdev->udev->dev,"request_disc_title: title too large for buffer");
		return -1;
	}
	memset(buffer,0,size);
	strncpy(buffer,(title+25),title_size-25);
	return title_size-25;	
}

static int netmd_set_title(struct netmd_device *mdev,int track,char *buffer,int size){
	int ret=1;
	char *title_request=NULL;
	char title_header[21]={0x00,0x18,0x07,0x02,0x20,0x18,0x02,0x00,0x00,0x30,0x00,0x0a,0x50,0x00,0x00,0x0a,0x00,0x00,0x00,0x0d};
	char reply[255];
	title_request=kmalloc(sizeof(char)*(0x15+size),GFP_KERNEL);
	memcpy(title_request,title_header,0x15);
	memcpy((title_request+0x15),buffer,size);
	title_request[8]=track;
	title_request[16]=title_request[20]=size;
	ret=netmd_exch_message(mdev,title_request,(int)(0x15+size),reply);
	if(ret<0)
	{
		dev_info(&mdev->udev->dev,"netmd_set_title:bad ret code");
		return 0;
	}
	kfree(title_request);
	return 1;
}

static int netmd_set_track(struct netmd_device *mdev,char track){
	int retval=0;
	unsigned char request[]={0x00,0x18,0x50,0xff,0x01,0x00,0x00,0x00,0x00,0x00,0x00};
	int size;
	unsigned char buf[255];
	request[10]=track-1;
	retval=netmd_exch_message(mdev,request,11,buf);
	if(retval<0){
		dev_info(&mdev->udev->dev,"bad ret code,returning early");
		return 0;
	}
	size=retval;
	if(size<1){
		dev_info(&mdev->udev->dev,"Invalid size");
		return -1;
	}
	dev_info(&mdev->udev->dev,"track retval:%d,buffer:%s",size,buf);
	return 1;
}

static int netmd_stop(struct netmd_device *mdev){
	int retvar=0;
	unsigned char request[]={0x00,0x18,0xc5,0xff,0x00,0x00,0x00,0x00};
	int size;
	unsigned char buf[255];
	dev_info(&mdev->udev->dev,"stop play");
	retvar=netmd_exch_message(mdev,request,8,buf);
	if(retvar<0){
		dev_info(&mdev->udev->dev,"[stop] bad ret code ,returning early");
		return 0;
	}
	size=retvar ;
	if(size<1){
		dev_info(&mdev->udev->dev,"[stop] invalid size");
		return 1;
	}
	return 1;
}

static int netmd_get_current_track(struct netmd_device *mdev){
	int retvar=0;
	unsigned char request[]={0x00,0x18,0x09,0x80,0x01,0x04,0x30,0x88,0x02,0x00,0x30,0x88,0x05,0x00,0x30,0x00,0x03,0x00,0x30,0x00,0x02,0x00,0xff,0x00,0x00,0x00,0x00,0x00};
	int track=0;
	unsigned char buf[255];
	retvar=netmd_exch_message(mdev,request,28,buf);
	track=buf[36];
	return track;
}

static int netmd_playback_control(struct netmd_device *mdev,unsigned char control_code){
	int retval=0;
	unsigned char request[]={0x00,0x18,0xc3,0xff,0x75,0x00,0x00,0x00};
	int size;
	unsigned char buf[255];
	
	request[4]=control_code;
	retval=netmd_exch_message(mdev,request,8,buf);
	size=retval;
	if(size<1){
		dev_info(&mdev->udev->dev,"invalid size");
		return -1;
	}
	return 1;
}

static void netmd_device_info(struct netmd_device *mdev){
	dev_info(&mdev->udev->dev,"product:%s,manufacturer:%s,serial:%s",mdev->udev->product,mdev->udev->manufacturer,mdev->udev->serial);
	dev_info(&mdev->udev->dev,"number of configuration:%d",mdev->udev->descriptor.bNumConfigurations);
}


/////////////////////////usb device operator
static int netmd_probe(struct usb_interface *interface,const struct usb_device_id *id){
	struct netmd_device *mdev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	size_t buffer_size;
	char disc_rawheader[256];
	int i;
	int retval = -ENOMEM;
	if(!(mdev= kzalloc(sizeof(*mdev),GFP_KERNEL))){
		err("out of memery!\n");
		goto error;
	}
	kref_init(&mdev->netmd_kref);
	mutex_init(&mdev->io_mutex);
	
	mdev->udev= usb_get_dev(interface_to_usbdev(interface));
	mdev->netmd_if= interface;
	iface_desc = mdev->netmd_if->cur_altsetting;

	mdev->num_endpoint=iface_desc->desc.bNumEndpoints;
	netmd_device_info(mdev);
	for(i=0;i<mdev->num_endpoint;i++){
		endpoint=&iface_desc->endpoint[i].desc;
		dev_info(&interface->dev,"endpoint type:%x,index:%x,direction:%x\n",endpoint->bmAttributes&USB_ENDPOINT_XFERTYPE_MASK,endpoint->bEndpointAddress&USB_ENDPOINT_NUMBER_MASK,endpoint->bEndpointAddress&USB_ENDPOINT_DIR_MASK);
			if(!mdev->bulk_in_endpointAddr&&usb_endpoint_is_bulk_in(endpoint)){
				buffer_size=le16_to_cpu(endpoint->wMaxPacketSize);
				mdev->bulk_in_size=buffer_size;
				dev_info(&interface->dev,"buffer size:%d\n",buffer_size);
				mdev->bulk_in_endpointAddr=endpoint->bEndpointAddress;
				mdev->bulk_in_buffer=kmalloc(buffer_size,GFP_KERNEL);
				if(!mdev->bulk_in_buffer){
					err("Could not alloc netmd device buffer\n");
					goto error;
				}
			}
			if(!mdev->bulk_out_endpointAddr&&usb_endpoint_is_bulk_out(endpoint))
				mdev->bulk_out_endpointAddr=endpoint->bEndpointAddress;
	}
	if(!(mdev->bulk_in_endpointAddr&&mdev->bulk_out_endpointAddr)){
		err("Could not find both bulk-in and bulk-out endpoint!\n");
		goto error;
	}
	usb_set_intfdata(interface,mdev);
	
	if((retval=usb_register_dev(interface,&netmd_class))){
		err("can't get a minor for netmd device!\n");
		usb_set_intfdata(interface,NULL);
		goto error;
	}
	dev_info(&interface->dev,"USB device NETMD attached to USB minor :%d",interface->minor);
	if(!(mdev->netmd_disc_info= kzalloc(sizeof(struct netmd_disc),GFP_KERNEL))){
		err("init netmd_disc : out of memery!\n");
		goto error;
	}
	mdev->netmd_disc_info->disc_header_len=request_disc_rawheader(mdev,disc_rawheader,256);
	format_disc_info(mdev,disc_rawheader);
	dev_info(&mdev->udev->dev,"disc title:%s",disc_rawheader);
	//netmd_set_track(mdev,2);
	//netmd_playback_control(mdev,0x75);
	return 0;
error:	
	if(mdev)
		kref_put(&mdev->netmd_kref,netmd_delete);
	return 0;
	
}
static void netmd_disconnect(struct usb_interface *intf){
	struct netmd_device *mdev;
	int minor = intf->minor;
	
	mdev = usb_get_intfdata(intf);
	usb_set_intfdata(intf,NULL);
	usb_deregister_dev(intf,&netmd_class);
	
	mutex_lock(&mdev->io_mutex);
	mdev->netmd_if= NULL;
	mutex_unlock(&mdev->io_mutex);
	
	kref_put(&mdev->netmd_kref,netmd_delete);
	dev_info(&intf->dev,"netmd #%d disconnected!",minor);
}
static int netmd_suspend(struct usb_interface *intf,pm_message_t message){
}
static int netmd_resume(struct usb_interface *intf){
}
static int netmd_pre_reset(struct usb_interface *intf){
}
static int netmd_post_reset(struct usb_interface *intf){
}

static struct usb_driver netmd_driver = {
	.name = "NetMD",
	.probe = netmd_probe,
	.disconnect = netmd_disconnect,
	.suspend = netmd_suspend,
	.resume = netmd_resume,
	.pre_reset = netmd_pre_reset,
	.post_reset = netmd_post_reset,
	.id_table = netmd_type_table,
	.supports_autosuspend = 1,
};
/////////////////////////usb device operator

////////////////////////////module initilize block
static int __init usb_netmd_init(void){
	int result;
	if((result = usb_register(&netmd_driver)))
		err("usb driver regist failed!,Error no.:%d\n",result);
	return result;
	
}
static void __exit usb_netmd_exit(void){
	usb_deregister(&netmd_driver);
}
module_init(usb_netmd_init);
module_exit(usb_netmd_exit);
MODULE_LICENSE("GPL");
