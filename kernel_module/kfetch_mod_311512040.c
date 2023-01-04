#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/cpu.h> //for cpu_data ,cpuinfo_x86
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/utsname.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/cpumask.h>  /* for num_online_cpus */ 
#include <linux/sysinfo.h>
#include <linux/topology.h>
#include <linux/device.h>
#include <linux/jiffies.h> /* for jiffies */

/* Prototypes - this would normally go in a .h file*/
int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "kfetch"   /* Dev name as it appears in /proc/devices   */
          

#define KFETCH_BUF_SIZE 1024
static int kfetch_mask;
static char kfetch_buf[KFETCH_BUF_SIZE];

#define KFETCH_NUM_INFO 6
#define KFETCH_RELEASE   (1 << 0)
#define KFETCH_NUM_CPUS  (1 << 1)
#define KFETCH_CPU_MODEL (1 << 2)
#define KFETCH_MEM       (1 << 3)
#define KFETCH_UPTIME    (1 << 4)
#define KFETCH_NUM_PROCS (1 << 5)
#define KFETCH_FULL_INFO ((1 << KFETCH_NUM_INFO) - 1);

/* Global variables are declared as static, so are global within the file.*/

static int Major;               /* Major number assigned to our device driver */
static int Device_Open = 0;     /* Is device open? Used to prevent multiple access to device */
static struct class *cls;

static struct file_operations chardev_fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

/* This function is called when the module is loaded */
int init_module(void)
{
    Major = register_chrdev(0, DEVICE_NAME, &chardev_fops);

    if (Major < 0) {
        pr_alert("Registering char device failed with %d\n", Major);
        return Major;
    }
    pr_info("I was assigned major number %d.\n", Major);

    cls = class_create(THIS_MODULE, DEVICE_NAME);
    device_create(cls, NULL, MKDEV(Major, 0), NULL, DEVICE_NAME);

    pr_info("Device created on /dev/%s\n", DEVICE_NAME);

    return SUCCESS;
}

/* This function is called when the module is unloaded */
void cleanup_module(void)
{
    device_destroy(cls, MKDEV(Major, 0));
    class_destroy(cls);

    /* Unregister the device */
    unregister_chrdev(Major, DEVICE_NAME);
}

/* Methods*/

/*Called when a process tries to open the device file, like"cat /dev/mycharfile"*/
static int device_open(struct inode *inode, struct file *file)
{

    if (Device_Open)
        return -EBUSY;

    Device_Open++;
    try_module_get(THIS_MODULE);
    return SUCCESS;
}

/* Called when a process closes the device file.*/
static int device_release(struct inode *inode, struct file *file)
{
    Device_Open--;          /* We're now ready for our next caller */

    /*Decrement the usage count, or else once you opened the file, you'llnever get get rid of the module.*/
    module_put(THIS_MODULE);

    return SUCCESS;
}

/*Called when a process, which already opened the dev file, attempts to read from it.*/
static ssize_t device_read(struct file *filp,   /* see include/linux/fs.h   */
                           char *buffer,        /* buffer to fill with data */
                           size_t length,       /* length of the buffer     */
                           loff_t * offset)
{
    /* retrieve system information */
    unsigned long free_mem ,total_mem;
    int len = 0;
    struct new_utsname *uts = utsname(); 
    struct sysinfo si;
    si_meminfo(&si);
    unsigned int cpu = 0;
    struct cpuinfo_x86 *c;
    c = &cpu_data(cpu);  //about cpu name
    int num_total_cpus = num_active_cpus(); //cpu number
    free_mem = (si.freeram*PAGE_SIZE);   //1KB = 1024B，再除以 1024 一次是因為 1MB = 1024KB 
    total_mem = (si.totalram*PAGE_SIZE); 
    free_mem = (free_mem/1000000);   
    total_mem = (total_mem/1000000);
    unsigned long uptime = jiffies_to_msecs(jiffies) / 60000 ;
    struct task_struct* task_list;
    int thread_counter = 0;
    for_each_process(task_list) {
    thread_counter += task_list->signal->nr_threads;
    }

    char *penguin[8];
    penguin[7] = "                     ";
    penguin[0] = "       .-.           ";
    penguin[1] = "      (.. |          ";
    penguin[2] = "      <>  |          ";
    penguin[3] = "     / --- \\         ";
    penguin[4] = "    ( |   | |        ";
    penguin[5] = "  |\\\\_)___/\\)/\\      ";
    penguin[6] = " <__)------(__/      ";

    
    char info_fun0[50]; 
    char info_fun1[50]; 
    char info_fun2[50]; 
    char info_fun3[50]; 
    char info_fun4[50]; 
    char info_fun5[50]; 
    sprintf(info_fun0, "Kernel: %s\n", uts->release);
    sprintf(info_fun1, "CPU: %s\n", c->x86_model_id);
    sprintf(info_fun2, "CPUs: %d / %d\n", num_online_cpus(), num_total_cpus);
    sprintf(info_fun3, "Mem: %ld / %ld MB\n", free_mem, total_mem);
    sprintf(info_fun4, "Procs: %d\n", thread_counter);
    sprintf(info_fun5, "Uptime: %ld minutes\n", uptime);

    char *info_buf[6];
    info_buf[0]=info_fun0;
    info_buf[1]=info_fun1;
    info_buf[2]=info_fun2;
    info_buf[3]=info_fun3;
    info_buf[4]=info_fun4;
    info_buf[5]=info_fun5;
    
            
    len += sprintf(kfetch_buf + len, "%s %s\n",penguin[7],uts->nodename);  //len為最後一個資訊的長度
    len += sprintf(kfetch_buf + len, "%s %s\n",penguin[0], "---------------");

    int checkarray [6] = {0};
    if (kfetch_mask & KFETCH_RELEASE) {
    // len += sprintf(kfetch_buf + len, "Kernel: %s\n", uts->release);
    checkarray[0] = 1;
    }
    if (kfetch_mask & KFETCH_CPU_MODEL) {
    // len += sprintf(kfetch_buf + len, "CPU: %s\n", c->x86_model_id);
    checkarray[1] = 1;
    }
    if (kfetch_mask & KFETCH_NUM_CPUS) {
    // len += sprintf(kfetch_buf + len, "CPUs: %d / %d\n", num_online_cpus(), num_total_cpus);
    checkarray[2] = 1;
    }
    if (kfetch_mask & KFETCH_MEM) {
    // len += sprintf(kfetch_buf + len, "Mem: %ld / %ld MB\n", free_mem, total_mem);
    checkarray[3] = 1;
    }
    if (kfetch_mask & KFETCH_UPTIME) {
    // len += sprintf(kfetch_buf + len, "Uptime: %ld minutes\n", uptime);
    checkarray[5] = 1;
    }
    if (kfetch_mask & KFETCH_NUM_PROCS) {
    // len += sprintf(kfetch_buf + len, "Procs: %d\n", thread_counter);
    checkarray[4] = 1;
    }
    int count=0;
    for (int i=1; i<=6; i++){   
        
        for (int j=0; j<=5; j++){
            if (checkarray[j+count]==1  && ((j+count) <=5)){
                len+= sprintf(kfetch_buf+len, "%s %s",penguin[i],info_buf[j+count]); //不用換行
                // count = count + j +1;
                count += j +1;
                break;
            }
            else{
                if (j==5){
                    len+= sprintf(kfetch_buf+len, "%s\n",penguin[i]);
                    break;
                }
                else
                    continue;
            }
        }
    }

    if (copy_to_user(buffer, kfetch_buf, len)) {
    pr_alert("Failed to copy data to user");
    return 0;
    }
    /* clean up */
    return len; 
}

/* Called when a process writes to dev file */
static ssize_t device_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset)
{
    int mask_info;
    /* copy the information mask from the user-space program */
    if (copy_from_user(&mask_info,buffer,length)) {
    pr_alert("Failed to copy data from user");
    return 0;
    }
    /* set the information mask in the kernel module */
    kfetch_mask = mask_info;

    return SUCCESS;  

}

MODULE_AUTHOR("Angus");
MODULE_DESCRIPTION("Print system information");
MODULE_LICENSE("GPL");
