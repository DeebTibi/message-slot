


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/errno.h>
#define MAJOR_NUM 235
#define BUF_LEN 128
#define NODE_NOT_FOUND -2
#define NODE_DEL_SUCCESS 0
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)

MODULE_LICENSE("GPL");

typedef struct
{
    int channel_num;
    size_t buffer_len;
    size_t message_length;
    char* buffer;
    spinlock_t lock;
    int devopen_flag;
} slot_channel;

typedef struct channel_node
{
    slot_channel* data;
    struct channel_node* next; 
} channel_node;

typedef struct
{
    channel_node* channel_list;
    int active_channel;
    int minor;
    
} message_slot;

typedef struct slot_node
{
    message_slot* data;
    struct slot_node* next;
} slot_node;

static channel_node* add_channel(channel_node* head, message_slot* slot, int channel_num);
static channel_node* get_channel(channel_node* head, int channel_num);
static int delete_channel(channel_node* head, message_slot* slot, int channel_num);

static slot_node* add_slot(slot_node** head, int minor);
static message_slot* get_slot(slot_node* head, int minor);
static int delete_slot(slot_node** head, int minor);

static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset);
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset);
static int device_release(struct inode* inode, struct file* file);
static int device_open(struct inode* inode, struct file* file);
static long int device_ioctl(struct file *, unsigned int cmd, unsigned long param);

static int __init initialize(void);
static void __exit cleanup(void);
