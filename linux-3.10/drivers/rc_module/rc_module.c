#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>

#define RC_KB 1024
#define RC_MB 1024*RC_KB
#define RC_FRAME_SIZ 20*RC_KB
#define RC_NUM_FRAME 50

#define FRAME_TABLE_INIT {NULL, 0,0,0}

typedef struct {
	int size;
	char *buf;
	struct mutex lock;
} Frame;

typedef struct {
	Frame *arr;
	int len;
	int head;
	int tail;
} FrameBuffer;

static FrameBuffer frame_buffer = FRAME_TABLE_INIT;

static int rc_create(){

	int i=0;
	frame_buffer.arr = (Frame *)vmalloc(10*sizeof(Frame));

	if(NULL == frame_buffer.arr){
		printk(KERN_INFO "Frame buffer creation failed");
		return -1;
	}

	for(i=0; i<RC_NUM_FRAME; i++){
		frame_buffer.arr[i].buf = (char *)vmalloc(RC_FRAME_SIZ);
		mutex_init(&(frame_buffer.arr[i].lock));
		if(NULL == frame_buffer.arr[i].buf){
			printk(KERN_INFO "Frame creation failed");
			return -1;
		}
	}

	printk(KERN_INFO "[RC_CAMERA] RC Buffer has been created\n");

	return 0;
}

static int rc_free(void){
	int i = 0;

	if(NULL == frame_buffer.arr){
		printk(KERN_INFO "Frame buffer freeing failed");
		return -1;
	}

	for(i=0; i<RC_NUM_FRAME; i++){
		if(NULL == frame_buffer.arr[i].buf){
			printk(KERN_INFO "Frame freeing failed");
			return -1;
		}

		vfree(frame_buffer.arr[i].buf);
		frame_buffer.arr[i].buf = NULL;
	}

	vfree(frame_buffer.arr);
	frame_buffer.arr = NULL;

	return 0;

}
static int rc_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "RC_opend\n");
	return 0;
}

static int rc_close(struct inode *inode, struct file *file){
	
	printk(KERN_INFO "rc closed\n");
	return 0;
}

static int rc_enqueue(const char __user *buff, size_t size){
	int len;
	if(NULL == buff){
		printk(KERN_INFO "User buffer is null\nn");
		return -1;
	}

	mutex_lock(&(frame_buffer.arr[frame_buffer.tail].lock));
	
	if(RC_NUM_FRAME == frame_buffer.len){
		frame_buffer.head++;
		if(RC_NUM_FRAME == frame_buffer.head)
			frame_buffer.head = 0;
		frame_buffer.len--;
	}
	frame_buffer.arr[frame_buffer.tail].size = size;
	printk(KERN_INFO "Buffer size %d\n", size);
	len = copy_from_user(frame_buffer.arr[frame_buffer.tail].buf, buff, size);
	if(0 != len){
		printk(KERN_INFO "copy from user failed\n");
	}
	frame_buffer.tail++;
	frame_buffer.len++;
	if(RC_NUM_FRAME == frame_buffer.tail){
		frame_buffer.tail = 0;
		mutex_unlock(&(frame_buffer.arr[RC_NUM_FRAME-1].lock));
	}
	else{
		mutex_unlock(&(frame_buffer.arr[frame_buffer.tail-1].lock));
	}

	if(len)
		return -1;
	return 0;
}

static size_t rc_dequeue(const char __user *buff, size_t size){
	size_t res=0;
	int len;
	if(NULL == buff){
		printk(KERN_INFO "User buffer is null");
		return 0;
	}

	printk(KERN_INFO "Frame length : %d\n", frame_buffer.len);
	if(0 == frame_buffer.len){
		printk(KERN_INFO "Frame length is 0\n");
		return 0;
	}

	mutex_lock(&(frame_buffer.arr[frame_buffer.head].lock));

	res = frame_buffer.arr[frame_buffer.head].size;
	printk(KERN_INFO "Size : %d, frame: %d\n", size, frame_buffer.arr[frame_buffer.head].size);
	if(size < res){
		printk(KERN_INFO "User buffer size is too small\n");
		mutex_unlock(&(frame_buffer.arr[frame_buffer.head].lock));
		return 0;
	}
	
	len = copy_to_user(buff, frame_buffer.arr[frame_buffer.head].buf, res);
	if(0 != len){
		printk(KERN_INFO "copy_to_user failed\n");
	}
	frame_buffer.head++;
	frame_buffer.len--;
	if(RC_NUM_FRAME == frame_buffer.head){
		frame_buffer.head = 0;
		mutex_unlock(&(frame_buffer.arr[RC_NUM_FRAME-1].lock));
	}
	else{
		mutex_unlock(&(frame_buffer.arr[frame_buffer.head-1].lock));
	}

	return res-len;
}
static int rc_read(struct file *file, char __user *buff, size_t size,
		loff_t *loff){
	
	printk(KERN_INFO "rc read\n");
	
	return  rc_dequeue(buff, size);
}

static int rc_write(struct file *file, const char __user *buff, size_t
		size, loff_t *offset){

	printk(KERN_INFO "rc writed\n");
	if(0 != rc_enqueue(buff,size))
		return 0;
	else
		return size;
}

static const struct file_operations rc_operations = {
	.owner	= THIS_MODULE,
	.write	= rc_write,
	.read	= rc_read,
	.open	= rc_open,
	.release= rc_close,
};

struct miscdevice rc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "rc_mem",
	.fops = &rc_operations,
	.mode = 0666,
};

static int __init misc_init(void)
{
	int error;
	printk(KERN_INFO "rc_device Registering");
	error = misc_register(&rc_device);
	if(error){
		printk(KERN_INFO "can't register misc device");
		return error;
	}
	if(0 != rc_create())
	{
		printk(KERN_INFO "rc_init failed");
		return -1;
	}

	printk(KERN_INFO "rc_device registered");

	return 0;
}

static void __exit misc_exit(void)
{
	misc_deregister(&rc_device);
	if(0 != rc_free()){
		printk(KERN_INFO "rc_exit failed");
		return -1;
	}
	printk(KERN_INFO "rc_device deregeistered");
}

module_init(misc_init);
module_exit(misc_exit);

MODULE_DESCRIPTION("RC Device");
MODULE_AUTHOR("Eunsoo Park <esevan.park@gmail.com>");
MODULE_LICENSE("GPL");
