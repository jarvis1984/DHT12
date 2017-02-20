/** @mainpage dht12.c
 *  @file dht12.c
 *  @author JarvisWang
 *  @brief This is a i2c driver for dht12 sensor.
 *  This dirver simply use delay function to control timing of signals,
 *  so that the transferring rate could not be high(25KHZ) because of interrupts
 */

#include <linux/init.h>           
#include <linux/module.h>         
#include <linux/device.h>         
#include <linux/kernel.h>         
#include <linux/fs.h>             
#include <asm/uaccess.h>          
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/kobject.h>
 
/**
 * @defgroup MODULE_INFO
 * @{
 */
MODULE_LICENSE("GPL");              		
MODULE_AUTHOR("JarvisWang");      				
MODULE_DESCRIPTION("A DHT12 I2C driver");  	
MODULE_VERSION("0.10");
/** @} */

/**
 * @defgroup CONSTANTS
 * @{
 */
#define DEVICE_NAME "dht12_i2c"			///< The device name will appear at /dev/
#define CLASS_NAME  "dht12"				///< The device class
#define ISQC_SCL GPIOA(7)				///< Specify nanopi m1 GPIOA7(pin#32) to be the clock pin
#define ISQC_SDA GPIOA(8)				///< Specify nanopi m1 GPIOA8(pin#33) to be the data pin
#define ISQC_FORLOOP_PERCYCLE 40		///< Cycle time is 40 us, 25KHZ, dht12 recquire the rate to be less than 400KHZ
#define ISQC_FORLOOP_HALFCYCLE ISQC_FORLOOP_PERCYCLE/2
#define ISQC_FORLOOP_QUARTCYCLE ISQC_FORLOOP_PERCYCLE/4
#define ISQC_DHT12_TEMP_INT 0x02
#define ISQC_DHT12_TEMP_PNT 0x03
#define ISQC_DHT12_HUMID_INT 0x00
#define ISQC_DHT12_HUMID_PNT 0x01
#define ISQC_DHT12_CHKSUM 0x04
#define ISQC_DHT12_BYTES 5
#define ISQC_DHT12_ADR 0xB8
#define ISQC_WRITE_CMD 0x00
#define ISQC_READ_CMD 0x01
#define ISQC_HIGH_LEVEL 0x01
#define ISQC_LOW_LEVEL 0x00
/** @} */

/**
 *  @defgroup GLOBAL_VARIABLES
 *  @{
 */
static int debug = 0;        					///< Execution mode, debug info will be printed when true
module_param(debug, int, (S_IRUSR | S_IWUSR)); 	///< Param desc. 
MODULE_PARM_DESC(debug, "The execution mode");  ///< parameter description

static int    majorNumber;                  	///< Stores the device number -- determined automatically
static struct class*  dhtClass  = NULL; 		///< The device-driver class struct pointer
static struct device* dhtDevice = NULL; 		///< The device-driver device struct pointer
static char   value[ISQC_DHT12_BYTES] = {0};	///< Values read from dht12 sensor
static char   chkSum = 0, humInt = 0, humFlt = 0, tmpInt = 0, tmpFlt = 0;

/** @brief A callback function to output the humInt variable
 *  @param kobj represents a kernel object device that appears in the sysfs filesystem
 *  @param attr the pointer to the kobj_attribute struct
 *  @param buf the buffer to which to write the number of presses
 *  @return return the total number of characters written to the buffer (excluding null)
 */
static ssize_t humInt_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%d\n", humInt);
}

/** @brief Displays the floating part of humidity */
static ssize_t humFlt_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%d\n", humFlt);
}

/** @brief Displays the integer part of temperature */
static ssize_t tmpInt_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%d\n", tmpInt);
}

/** @brief Displays the floating part of temperature */
static ssize_t tmpFlt_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%d\n", tmpFlt);
}

/** @brief Displays check sum value */
static ssize_t chkSum_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%d\n", chkSum);
}

/**  The __ATTR_RO macro defines a read-only attribute. There is no need to identify that the
 *   function is called _show, but it must be present. __ATTR_WO can be  used for a write-only
 *   attribute but only in Linux 3.11.x on.
 */
static struct kobj_attribute humInt_attr = __ATTR_RO(humInt);	///< Integer part of humidity
static struct kobj_attribute humFlt_attr = __ATTR_RO(humFlt);	///< Floating part of humidity
static struct kobj_attribute tmpInt_attr = __ATTR_RO(tmpInt);	///< Integer part of temperature
static struct kobj_attribute tmpFlt_attr = __ATTR_RO(tmpFlt);	///< Floating part of temperature
static struct kobj_attribute chkSum_attr = __ATTR_RO(chkSum);	///< Check sum value

/**  The dht12_attrs[] is an array of attributes that is used to create the attribute group below.
 *   The attr property of the kobj_attribute is used to extract the attribute struct
 */
static struct attribute *dht12_attrs[] = {
      &humInt_attr.attr,                  	
      &humFlt_attr.attr,                  	
      &tmpInt_attr.attr,                   	
      &tmpFlt_attr.attr,                   	
      &chkSum_attr.attr,               		
      NULL,
};
 
/**  The attribute group uses the attribute array and a name, which is exposed on sysfs
 */
static struct attribute_group attr_group = {
      .name  = "dht12_attr",               ///< The name of this attribute group
      .attrs = dht12_attrs,                ///< The attributes array defined just above
};

static struct kobject *dht12_kobj;	///< kernel object
/** @} */

/** 
 *  @defgroup MACROS
 *  @{
 */
#define ISQC_SET_INPUT_PIN() do {gpio_direction_input(ISQC_SCL);gpio_direction_input(ISQC_SDA);} while (0)
#define ISQC_SET_OUTPUT_PIN() do {gpio_direction_output(ISQC_SCL, ISQC_LOW_LEVEL);\
								  gpio_direction_output(ISQC_SDA, ISQC_LOW_LEVEL);} while (0)
#define ISQC_SET_SCL_OUT(x) gpio_direction_output(ISQC_SCL, x)
#define ISQC_SET_SDA_OUT(x) gpio_direction_output(ISQC_SDA, x)
#define ISQC_SET_SCL_IN() gpio_direction_input(ISQC_SCL)
#define ISQC_SET_SDA_IN() gpio_direction_input(ISQC_SDA)
#define ISQC_SET_SCL(x) gpio_set_value(ISQC_SCL, x)
#define ISQC_SET_SDA(x) gpio_set_value(ISQC_SDA, x)
#define ISQC_GET_SCL() gpio_get_value(ISQC_SCL)
#define ISQC_GET_SDA() gpio_get_value(ISQC_SDA)
#define ISQC_BIT0(x) (x & 0x1)
#define ISQC_BIT1(x) ((x>>1) & 0x1)
#define ISQC_BIT2(x) ((x>>2) & 0x1)
#define ISQC_BIT3(x) ((x>>3) & 0x1)
#define ISQC_BIT4(x) ((x>>4) & 0x1)
#define ISQC_BIT5(x) ((x>>5) & 0x1)
#define ISQC_BIT6(x) ((x>>6) & 0x1)
#define ISQC_BIT7(x) ((x>>7) & 0x1)

/** @brief the unit is micro second */
#define ISQC_DELAY(x) udelay(x)
/** @brief print debug message in debug mode */
#define ISQC_DEBUG(x) debug ? printk(KERN_INFO "DBG:"x) : debug

/**@brief
 * a complete cycle to write a bit to device
 * start from the time allowed to write value
 * end with the time allowed to write value, either
 */
#define ISQC_WRITE_BIT(n, x) ISQC_SET_SCL(ISQC_LOW_LEVEL); \
                             ISQC_DELAY(ISQC_FORLOOP_QUARTCYCLE); \
                             ISQC_SET_SDA(ISQC_BIT##n(x)); \
                             ISQC_DELAY(ISQC_FORLOOP_QUARTCYCLE); \
                             ISQC_SET_SCL(ISQC_HIGH_LEVEL); \
                             ISQC_DELAY(ISQC_FORLOOP_HALFCYCLE); \
/**@brief
 * write a byte to device
 * start from msb
 */                               
#define ISQC_WRITE_BYTE(x) ISQC_SET_OUTPUT_PIN(); \
                           ISQC_WRITE_BIT(7, x); \
                           ISQC_WRITE_BIT(6, x); \
                           ISQC_WRITE_BIT(5, x); \
                           ISQC_WRITE_BIT(4, x); \
                           ISQC_WRITE_BIT(3, x); \
                           ISQC_WRITE_BIT(2, x); \
                           ISQC_WRITE_BIT(1, x); \
                           ISQC_WRITE_BIT(0, x);

/**@brief
 * a complete cycle to read a bit from device
 * start from the time allowed to read value
 * end with the time allowed to read value, either
 * SCL and SDA must be input pin first
 */
#define ISQC_READ_BIT(n, x) ISQC_SET_SCL(ISQC_LOW_LEVEL); \
                            ISQC_DELAY(ISQC_FORLOOP_HALFCYCLE); \
                            ISQC_SET_SCL(ISQC_HIGH_LEVEL); \
                            ISQC_DELAY(ISQC_FORLOOP_QUARTCYCLE); \
                            if (ISQC_GET_SDA()) x |= (0x01 << n); \
                            else x &= ~(0x01 << n); \
                            ISQC_DELAY(ISQC_FORLOOP_QUARTCYCLE);

/**@brief
 * read a byte to device
 * start from msb
 */                               
#define ISQC_READ_BYTE(x) ISQC_SET_SCL(ISQC_LOW_LEVEL); \
						  ISQC_SET_SDA_IN(); \
                          ISQC_READ_BIT(7, x); \
                          ISQC_READ_BIT(6, x); \
                          ISQC_READ_BIT(5, x); \
                          ISQC_READ_BIT(4, x); \
                          ISQC_READ_BIT(3, x); \
                          ISQC_READ_BIT(2, x); \
                          ISQC_READ_BIT(1, x); \
                          ISQC_READ_BIT(0, x);

/**@brief
 * send a nack signal to device to end communication
 */  
#define ISQC_SEND_NACK() ISQC_SET_OUTPUT_PIN(); \
                         ISQC_SET_SCL(ISQC_LOW_LEVEL); \
                         ISQC_SET_SDA(ISQC_HIGH_LEVEL); \
                         ISQC_DELAY(ISQC_FORLOOP_HALFCYCLE); \
                         ISQC_SET_SCL(ISQC_HIGH_LEVEL); \
                         ISQC_DELAY(ISQC_FORLOOP_HALFCYCLE);

/**@brief
 * wait a ack signal from device, or return a nack signal
 */
#define ISQC_WAIT_ACK() ISQC_SET_SCL(ISQC_LOW_LEVEL); \
                        ISQC_SET_SDA_IN(); \
                        ISQC_DELAY(ISQC_FORLOOP_HALFCYCLE); \
                        ISQC_SET_SCL(ISQC_HIGH_LEVEL); \
                        ISQC_DELAY(ISQC_FORLOOP_HALFCYCLE); \
                        if (ISQC_GET_SDA() != ISQC_LOW_LEVEL){ISQC_SEND_NACK(); \
                        ISQC_SET_INPUT_PIN(); return 0;}
                        
/**@brief
 * send a ack signal to device
 */  
#define ISQC_SEND_ACK() ISQC_SET_SCL(ISQC_LOW_LEVEL); \
						ISQC_DELAY(ISQC_FORLOOP_QUARTCYCLE); \
						ISQC_SET_SDA_OUT(ISQC_LOW_LEVEL);\
                        ISQC_DELAY(ISQC_FORLOOP_QUARTCYCLE); \
                        ISQC_SET_SCL(ISQC_HIGH_LEVEL); \
                        ISQC_DELAY(ISQC_FORLOOP_HALFCYCLE);
/** @} */

/** 
 *  @brief The device open function that is called each time the device is opened
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
   	ISQC_DEBUG("dev_open()\n");
	
	gpio_request(ISQC_SCL, "I2C CLOCK");
	gpio_direction_input(ISQC_SCL);
	gpio_request(ISQC_SDA, "I2C DATA");
	gpio_direction_input(ISQC_SDA);
	
   	return 0;
}
 
/** 
 *  @brief This function is called whenever device is being read
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
	ISQC_DEBUG("dev_read()\n");
	
	int dev = ISQC_DHT12_ADR + ISQC_WRITE_CMD;
	int adr = 0x0;
	ISQC_SET_INPUT_PIN();

	if (ISQC_GET_SCL() == ISQC_LOW_LEVEL || ISQC_GET_SDA() == ISQC_LOW_LEVEL) //some device is working
	{
		ISQC_DEBUG("busy\n");
		return 0;
	}

	/**
	 * start flag
	 */
	ISQC_SET_SDA_OUT(ISQC_LOW_LEVEL);
	//ISQC_SET_SDA(ISQC_LOW_LEVEL);
	ISQC_DELAY(ISQC_FORLOOP_HALFCYCLE);

	/*
	 * send slave address to write
	 */
	ISQC_WRITE_BYTE(dev);

	/*
	 * wait ack
	 */
	ISQC_WAIT_ACK();

	/*
	 * send register address to read
	 */
	ISQC_WRITE_BYTE(adr);

	/*
	 * wait ack
	 */
	ISQC_WAIT_ACK();

	/*
	 * read device
	 */
	dev = ISQC_DHT12_ADR + ISQC_READ_CMD;  

	/**
	 * start again
	 */
	ISQC_SET_SCL(ISQC_LOW_LEVEL);
	ISQC_DELAY(ISQC_FORLOOP_HALFCYCLE);
	ISQC_SET_SDA_OUT(ISQC_HIGH_LEVEL);
	ISQC_SET_SCL(ISQC_HIGH_LEVEL);
	ISQC_DELAY(ISQC_FORLOOP_HALFCYCLE);
	ISQC_SET_SDA(ISQC_LOW_LEVEL);
	ISQC_DELAY(ISQC_FORLOOP_HALFCYCLE);

	/*
	 * send slave address to read
	 */
	ISQC_WRITE_BYTE(dev);

	/*
	 * wait ack
	 */
	ISQC_WAIT_ACK();

	/*
	 * recieve humidity integer value
	 */
	ISQC_READ_BYTE(humInt);

	/*
	 * send ack
	 */
	ISQC_SEND_ACK();

	/*
	 * recieve humidity float value
	 */
	ISQC_READ_BYTE(humFlt);

	/*
	 * send ack
	 */
	ISQC_SEND_ACK();

	/*
	 * recieve temperature integer value
	 */
	ISQC_READ_BYTE(tmpInt);

	/*
	 * send ack
	 */
	ISQC_SEND_ACK();

	/*
	 * recieve temperature float value
	 */
	ISQC_READ_BYTE(tmpFlt);

	/*
	 * send ack
	 */
	ISQC_SEND_ACK();

	/*
	 * recieve checksum value
	 */
	ISQC_READ_BYTE(chkSum);

	/*
	 * send nack
	 */
	ISQC_SEND_NACK();

	/*
	 * send stop
	 */
	ISQC_SET_SCL(ISQC_LOW_LEVEL);
	ISQC_SET_SDA(ISQC_LOW_LEVEL);
	ISQC_DELAY(ISQC_FORLOOP_HALFCYCLE);
	ISQC_SET_SCL(ISQC_HIGH_LEVEL);
	ISQC_DELAY(ISQC_FORLOOP_QUARTCYCLE);
	ISQC_SET_SDA(ISQC_HIGH_LEVEL);
	ISQC_DELAY(ISQC_FORLOOP_QUARTCYCLE);

	ISQC_SET_INPUT_PIN();
	
	if (debug)
		printk(KERN_INFO "humInt:%d humFlt:%d tmpInt:%d tmpFlt:%d chkSum:%d\n", humInt, humFlt, tmpInt, tmpFlt, chkSum);

	if (chkSum == humInt + humFlt + tmpInt + tmpFlt)
	{
		value[ISQC_DHT12_HUMID_INT] = humInt;
		value[ISQC_DHT12_HUMID_PNT] = humFlt;
		value[ISQC_DHT12_TEMP_INT] = tmpInt;
		value[ISQC_DHT12_TEMP_PNT] = tmpFlt;
		value[ISQC_DHT12_CHKSUM] = chkSum;
		
		copy_to_user(buffer, value, ISQC_DHT12_BYTES);
		return ISQC_DHT12_BYTES;
	}
	
	ISQC_DEBUG("check sum error\n");
	return 0;
}
 
/** 
 *  @brief This function is called whenever the device is being written to.
 *  	   Since the device is not writable, this function should return an error directly.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 *  @return EBADF, this file is not for writing
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
   ISQC_DEBUG("dev_write()\n");
   return EBADF;
}
 
/** 
 *  @brief The device release function that is called whenever the device is closed/released by
 *  	   the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
   	ISQC_DEBUG("dev_release()\n");
	
	gpio_free(ISQC_SDA);
	gpio_free(ISQC_SCL);
   	return 0;
}

/** 
 *  @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

/** 
 *  @brief The LKM initialization function
 *  @return returns 0 if successful
 */
static int __init dht_init(void){
    printk(KERN_INFO "dht_init()\n");
	int result = 0;
	
	/** create the kobject sysfs entry at /sys/kernel/dht12 */
	dht12_kobj = kobject_create_and_add("dht12", kernel_kobj); // kernel_kobj points to /sys/kernel
	if(!dht12_kobj){
		printk(KERN_ALERT "dht_init: failed to create kobject mapping\n");
		return -ENOMEM;
	}
	// add the attributes to kobject sysfs entry
	result = sysfs_create_group(dht12_kobj, &attr_group);
	if(result) {
		printk(KERN_ALERT "dht_init: failed to create sysfs group\n");
		kobject_put(dht12_kobj);                          // clean up -- remove the kobject sysfs entry
		return result;
	}
	
	// Try to dynamically allocate a major number for the device
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber<0){
	  printk(KERN_ALERT "dht_init failed to register a major number\n");
	  return majorNumber;
	}
	printk(KERN_INFO "dht_init: registered correctly with major number %d\n", majorNumber);

	// Register the device class
	dhtClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(dhtClass)){                // Check for error and clean up if there is
	  unregister_chrdev(majorNumber, DEVICE_NAME);
	  printk(KERN_ALERT "dht_init() failed to register device class\n");
	  return PTR_ERR(dhtClass);          // Correct way to return an error on a pointer
	}
	printk(KERN_INFO "dht_init: device class registered correctly\n");

	// Register the device driver
	dhtDevice = device_create(dhtClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if (IS_ERR(dhtDevice)){               // Clean up if there is an error
	  class_destroy(dhtClass);           // Repeated code but the alternative is goto statements
	  unregister_chrdev(majorNumber, DEVICE_NAME);
	  printk(KERN_ALERT "dht_init() failed to create the device\n");
	  return PTR_ERR(dhtDevice);
	}
	
	printk(KERN_INFO "dht_init: device class created correctly\n");
    return 0;
}
 
/** 
 *  @brief The LKM cleanup function
 */
static void __exit dht_exit(void){
	kobject_put(dht12_kobj); 
	device_destroy(dhtClass, MKDEV(majorNumber, 0));     // remove the device
	class_unregister(dhtClass);                          // unregister the device class
	class_destroy(dhtClass);                             // remove the device class
	unregister_chrdev(majorNumber, DEVICE_NAME);         // unregister the major number
	printk(KERN_INFO "dht_exit()\n");
}
 
/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(dht_init);
module_exit(dht_exit);