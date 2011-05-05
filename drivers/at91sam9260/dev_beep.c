/*********************************************************************************
 *  Copyright(c)  2011, GuoWenxue<guowenxue@gmail.com>.
 *  All ringhts reserved.
 *
 *     Filename:  dev_beep.c
 *  Description:  AT91SAM9260 platform beep driver.
 *
 *     ChangLog:
 *      1,   Version: 1.0.0
 *              Date: 2011-05-02
 *            Author: guowenxue <guowenxue@gmail.com>
 *       Descrtipion: Initial first version
 *
 ********************************************************************************/

#include "include/plat_driver.h"

#define DRV_AUTHOR                "GuoWenxue<guowenxue@gmail.com>"
#define DRV_DESC                  "AT91SAM9260 beep driver"

/*Driver version*/
#define DRV_MAJOR_VER             1
#define DRV_MINOR_VER             0
#define DRV_REVER_VER             0

#define DEV_NAME                  DEV_BEEP_NAME

#ifndef DEV_MAJOR
#define DEV_MAJOR                 0 /*dynamic major by default */
#endif

struct cdev *beep_cdev = NULL;

static int debug = DISABLE;
static int dev_major = DEV_MAJOR;
static int dev_minor = 0;

module_param(debug, int, S_IRUGO);
module_param(dev_major, int, S_IRUGO);
module_param(dev_minor, int, S_IRUGO);

volatile unsigned int *PMC_SCER;
volatile unsigned int *PMC_SCDR;
volatile unsigned int *PMC_PCK1;
volatile unsigned int *PMC_SR;

static unsigned int m_uiPrescaler = 0x5;

#define dbg_print(format,args...) if(DISABLE!=debug) \
                                  {printk("[kernel] ");printk(format, ##args);}

static int beep_open(struct inode *inode, struct file *file)
{
    at91_set_B_periph(BEEP_PIN, DISPULLUP);

    *PMC_SCDR |= (0x1 << 9);
    *PMC_PCK1 = ((m_uiPrescaler & 0xff) << 2);
    while ((0x1 << 9) != ((*PMC_SR) & (0x1 << 9))) ;

    return 0;
}

static int beep_release(struct inode *inode, struct file *file)
{
    return 0;
}

static int beep_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd)
    {
      case BEEP_ENALARM:
          at91_set_B_periph(BEEP_PIN, DISPULLUP);
          *PMC_SCER |= (0x1 << 9);
          break;

      case BEEP_DISALARM:
          *PMC_SCDR |= (0x1 << 9);
          at91_set_gpio_output(BEEP_PIN, 1);
          break;

      case SET_DRV_DEBUG:
          printk("%s %s driver debug now.\n", DISABLE == arg ? "Disable" : "Enable", DEV_NAME);

          if (0 == arg)
              debug = DISABLE;
          else
              debug = ENABLE;

          break;

      case GET_DRV_VER:
          print_version(DRV_VERSION);
          return DRV_VERSION;
          break;

      default:
          printk("%s driver don't support ioctl command=%d\n", DEV_NAME, cmd);
          return -ENOTTY;
    }

    return 0;
}

static struct file_operations beep_fops = {
    .owner = THIS_MODULE,
    .open = beep_open,
    .release = beep_release,
    .ioctl = beep_ioctl,
};

static struct class * dev_class;

static void beep_cleanup(void)
{
    dev_t devno = MKDEV(dev_major, dev_minor);

    *PMC_SCDR |= (0x1 << 9);
    at91_set_gpio_output(BEEP_PIN, 1);

    iounmap(PMC_SCER);
    iounmap(PMC_SCDR);
    iounmap(PMC_PCK1);
    iounmap(PMC_SR);

    device_destroy (dev_class, devno);
    class_destroy (dev_class);

    cdev_del(beep_cdev);
    unregister_chrdev_region(devno, 1);

    printk("%s driver removed\n", DEV_NAME);
}

static int __init beep_init(void)
{
    int result;
    dev_t devno;

    /*Alloc for the device for driver */
    if (0 != dev_major)
    {
        devno = MKDEV(dev_major, dev_minor);
        result = register_chrdev_region(devno, 1, DEV_NAME);
    }
    else
    {
        result = alloc_chrdev_region(&devno, dev_minor, 1, DEV_NAME);
        dev_major = MAJOR(devno);
    }

    /*Alloc for device major failure */
    if (result < 0)
    {
        printk("%s driver can't get major %d\n", DEV_NAME, dev_major);
        return result;
    }

    /*Alloc cdev structure */
    beep_cdev = cdev_alloc();;
    if (NULL == beep_cdev)
    {
        printk("%s driver can't alloc for beep_cdev\n", DEV_NAME);
        goto ERROR;
    }

    /*Initialize cdev structure and register it */
    beep_cdev->owner = THIS_MODULE;
    beep_cdev->ops = &beep_fops;
    result = cdev_add(beep_cdev, devno, 1);
    if (0 != result)
    {
        printk("%s driver can't alloc for beep_cdev\n", DEV_NAME);
        goto ERROR;
    }

    /*Create device /dev/$DEV_NAME */
    dev_class = class_create (THIS_MODULE, DEV_NAME);
    if(dev_class)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,24)
        device_create (dev_class, NULL, devno, NULL, "%s", DEV_NAME);
#else
        device_create (dev_class, NULL, devno, "%s", DEV_NAME);
#endif
    else
        printk ("%s driver can't create device class\n", DEV_NAME);


    PMC_SCER = ioremap(AT91_PMC + AT91_BASE_SYS + 0x00, 0x04);
    PMC_SCDR = ioremap(AT91_PMC + AT91_BASE_SYS + 0x04, 0x04);
    PMC_PCK1 = ioremap(AT91_PMC + AT91_BASE_SYS + 0x44, 0x04);
    PMC_SR = ioremap(AT91_PMC + AT91_BASE_SYS + 0x68, 0x04);
    udelay(100);

    printk("%s driver version %d.%d.%d initiliazed\n", DEV_NAME, DRV_MAJOR_VER, DRV_MINOR_VER,
           DRV_REVER_VER);

    return 0;

  ERROR:
    beep_cleanup();
    return -ENODEV;

}

module_init(beep_init);
module_exit(beep_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRV_AUTHOR);
MODULE_DESCRIPTION(DRV_DESC);
