#define BCD_TO_PROPER(x) ((((x)&0xf0)>>4)*10+((x)&0x0f))
#define USB_MD_VENDOR_ID 0x054c
#define USB_MD_PRODUCT_ID 0x0286

#define to_netmd_dev(d) container_of(d, struct netmd_device, netmd_kref)
struct netmd_device;
static int netmd_stop(struct netmd_device *);
static int netmd_playback_control(struct netmd_device *,unsigned char); 
static int netmd_set_track(struct netmd_device *,char);
static int netmd_set_title(struct netmd_device *netmd,int track,char *buffer,int size);
static int netmd_get_current_track(struct netmd_device *);
static int request_disc_rawheader(struct netmd_device *mdev,char *buffer,int size);
static void set_group_data(struct netmd_device *,int,char * ,int,int);
int netmd_request_track_title(struct netmd_device *,int,char *,int);
int netmd_request_track_codec(struct netmd_device *,int,unsigned char*);
int netmd_request_track_bitrate(struct netmd_device *,int,unsigned char*);


//////////////////NetMD track properites
struct netmd_pair{
	unsigned char hex;
	char *t_name;
};

struct netmd_pair const codecs[] = 
{
	{0x00, "ATRAC"},
	{0x03, "ATRAC3"},
	{0, 0} /* terminating pair */
};

struct netmd_pair const bitrates[] = 
{
	{0x90, "Stereo"},
	{0x92, "LP2"},
	{0x93, "LP4"},
	{0, 0} /* terminating pair */
};

struct netmd_pair const unknow_pair={0x00,"UNKNOW"};
//////////////////NetMD track properites


////////////////////////NetMD disc 
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
	int track_count;
	int disc_header_len;
};

////////////////////////NetMD disc////////

