/***************************************************************************//**
*  \file       driver.c
*
*  \details    Simple Linux device driver (IOCTL)
*
*  \author     EmbeTronicX
*
*  \Tested with Linux raspberrypi 5.10.27-v7l-embetronicx-custom+
*
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include<linux/slab.h>                 //kmalloc()
#include<linux/uaccess.h>              //copy_to/from_user()
#include <linux/ioctl.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <linux/arm-smccc.h>
 
dev_t dev = 0;
static struct class *dev_class;
static struct cdev etx_cdev;

/*
** Function Prototypes
*/
static int      __init etx_driver_init(void);
static void     __exit etx_driver_exit(void);
static int      etx_open(struct inode *inode, struct file *file);
static int      etx_release(struct inode *inode, struct file *file);
static ssize_t  etx_read(struct file *filp, char __user *buf, size_t len,loff_t * off);
static ssize_t  etx_write(struct file *filp, const char *buf, size_t len, loff_t * off);
static long     etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int      etx_mmap(struct file *filp, struct vm_area_struct *vma);

/*
** File operation sturcture
*/
static struct file_operations fops =
{
        .owner          = THIS_MODULE,
        .read           = etx_read,
        .write          = etx_write,
        .open           = etx_open,
        .mmap           = etx_mmap,
        .unlocked_ioctl = etx_ioctl,
        .release        = etx_release,
};

typedef struct {
    unsigned long long int smc_fid;
    unsigned long long int x1;
    unsigned long long int x2;
    unsigned long long int x3;
    unsigned long long int x4;
} SmcArgs;

typedef struct {
    size_t addr;  // 用于存储物理地址
    size_t size;       // 用于存储读取的大小
    size_t recv_addr;
}AAR_arg;

typedef struct {
    size_t addr;  // 用于存储物理地址
    size_t size;       // 用于存储读取的大小
    size_t val;
}AAW_arg;

#define CMD_SMC 0x13370001
#define CMD_AAR 0x13370002
#define CMD_AAW 0x13370003

SmcArgs smcArg;
AAR_arg aar_arg;
AAW_arg aaw_arg;
unsigned char *buf[0x1000];

static unsigned long long int smc_call(
    unsigned long long int smc_fid, 
    unsigned long long int arg1, 
    unsigned long long int arg2, 
    unsigned long long int arg3,
    unsigned long long int arg4,
    struct arm_smccc_res *res
    ){
    arm_smccc_smc(smc_fid, arg1, arg2, arg3, arg4, 0, 0, 0, res);
    return res->a0;
}
static int etx_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long offset = vma->vm_pgoff;

    if (offset >= __pa(high_memory) || (filp->f_flags & O_SYNC))
        vm_flags_set(vma, vma->vm_flags | VM_IO);
    vm_flags_set(vma, vma->vm_flags | (VM_DONTEXPAND | VM_DONTDUMP));

    if (io_remap_pfn_range(vma, vma->vm_start, offset, 
        vma->vm_end-vma->vm_start, vma->vm_page_prot))
        return -EAGAIN;
    return 0;
}

/*
** This function will be called when we open the Device file
*/
static int etx_open(struct inode *inode, struct file *file)
{
        pr_info("Device File Opened...!!!\n");
        return 0;
}

/*
** This function will be called when we close the Device file
*/
static int etx_release(struct inode *inode, struct file *file)
{
        pr_info("Device File Closed...!!!\n");
        return 0;
}

/*
** This function will be called when we read the Device file
*/
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
        pr_info("Read Function\n");
        return 0;
}

/*
** This function will be called when we write the Device file
*/
static ssize_t etx_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
        pr_info("Write function\n");
        return len;
}

/*
** This function will be called when we write IOCTL on the Device file
*/
static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
         switch(cmd) {
                case 0x13370001:
                        if( copy_from_user(&smcArg ,(SmcArgs*) arg, sizeof(SmcArgs)) )
                        {
                                pr_err("smc arg : Err!\n");
                                return -EFAULT;
                        }
                        int ret = smc_call(smcArg.smc_fid, smcArg.x1, smcArg.x2, smcArg.x3, smcArg.x4, (struct arm_smccc_res *)&smcArg);
                        pr_info("smc ret: 0x%llx\n", ret);
                        copy_to_user((SmcArgs*) arg, &smcArg, sizeof(SmcArgs));
                        return ret;
                        break;
                case CMD_AAR:
                        if( copy_from_user(&aar_arg ,(AAR_arg*) arg, sizeof(AAR_arg)) )
                        {
                                pr_err("aar arg : Err!\n");
                                return -EFAULT;
                        }

                        unsigned char* ptr = (unsigned char*)aar_arg.addr;
                        if( copy_to_user((unsigned char*)aar_arg.recv_addr, ptr,aar_arg.size) )
                        {
                                pr_err("Data Read : Err!\n");
                                return -EFAULT;
                        }

                        break;
                case CMD_AAW:
                        if (copy_from_user(&aaw_arg, (unsigned long *)arg, sizeof(aaw_arg)))
                        {
                            pr_err("aaw arg : Err!\n");
                            return -EFAULT;
                        }

                        // 将物理地址映射到内核空间
                        void *mapped_address = ioremap(aaw_arg.addr, 16);
                        if (!mapped_address)
                        {
                            return -ENOMEM;
                        }

                        // 写入数据
                        iowrite64(aaw_arg.val, mapped_address);

                        // 解除映射
                        iounmap(mapped_address);
                        break;
                default:
                        pr_info("Default\n");
                        break;
        }
        return 0;
}

 
/*
** Module Init function
*/
static int __init etx_driver_init(void)
{
        /*Allocating Major number*/
        if((alloc_chrdev_region(&dev, 0, 1, "etx_Dev")) <0){
                pr_err("Cannot allocate major number\n");
                return -1;
        }
        pr_info("Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));
 
        /*Creating cdev structure*/
        cdev_init(&etx_cdev,&fops);
 
        /*Adding character device to the system*/
        if((cdev_add(&etx_cdev,dev,1)) < 0){
            pr_err("Cannot add the device to the system\n");
            goto r_class;
        }
 
        /*Creating struct class*/
        if(IS_ERR(dev_class = class_create("etx_class"))){
            pr_err("Cannot create the struct class\n");
            goto r_class;
        }
 
        /*Creating device*/
        if(IS_ERR(device_create(dev_class,NULL,dev,NULL,"etx_device"))){
            pr_err("Cannot create the Device 1\n");
            goto r_device;
        }
        pr_info("Device Driver Insert...Done!!!\n");
        return 0;
 
r_device:
        class_destroy(dev_class);
r_class:
        unregister_chrdev_region(dev,1);
        return -1;
}

/*
** Module exit function
*/
static void __exit etx_driver_exit(void)
{
        device_destroy(dev_class,dev);
        class_destroy(dev_class);
        cdev_del(&etx_cdev);
        unregister_chrdev_region(dev, 1);
        pr_info("Device Driver Remove...Done!!!\n");
}
 
module_init(etx_driver_init);
module_exit(etx_driver_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("EmbeTronicX <embetronicx@gmail.com>");
MODULE_DESCRIPTION("Simple Linux device driver (IOCTL)");
MODULE_VERSION("1.5");