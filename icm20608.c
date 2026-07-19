#include "icm20608.h"

struct icm20608_data{
    signed int gyro_x_adc;		/* 陀螺仪X轴原始值 	 */
	signed int gyro_y_adc;		/* 陀螺仪Y轴原始值		*/
	signed int gyro_z_adc;		/* 陀螺仪Z轴原始值 		*/
	signed int accel_x_adc;		/* 加速度计X轴原始值 	*/
	signed int accel_y_adc;		/* 加速度计Y轴原始值	*/
	signed int accel_z_adc;		/* 加速度计Z轴原始值 	*/
	signed int temp_adc;		/* 温度原始值 			*/
};

struct icm20608_dev{
    struct cdev cdev;
    dev_t devid;
    int major;
    int minor;
    struct class *class;
    struct device *device;
    int gpioid;
    struct spi_device *m_spi_device;
    struct device_node *nd;
    struct icm20608_data data;
};

static int icm20608_read_regs(struct icm20608_dev *dev, u8 reg, void *buf, int len)
{
    int ret;
    u8 tx_buf = reg | 0x80; 
    struct spi_device *spi = dev->m_spi_device;
    struct spi_transfer t[2] = {0};
    struct spi_message msg;
    t[0].tx_buf = &tx_buf;
    t[0].len = 1;
    t[1].rx_buf = buf;
    t[1].len = len;

    spi_message_init(&msg);
    spi_message_add_tail(&t[0],&msg);
    spi_message_add_tail(&t[1], &msg);

    gpio_set_value(dev->gpioid, 0);          // 手动拉低片选
    ret = spi_sync(spi, &msg);               // 一次性执行两个传输
    gpio_set_value(dev->gpioid, 1);          // 手动拉高片选

    return ret;
}

static int icm20608_write_regs(struct icm20608_dev *dev, u8 reg, u8 *buf, u8 len)
{
    int ret;
    u8 tx_buf = reg & (~0x80); 
    struct spi_message msg;
    struct spi_device *spi = dev->m_spi_device;
    struct spi_transfer t[2] =  {0};
    t[0].tx_buf = &tx_buf;
    t[0].len = 1;
    t[1].tx_buf = buf;
    t[1].len = len;
    
    spi_message_init(&msg);
    spi_message_add_tail(&t[0], &msg);
    spi_message_add_tail(&t[1], &msg);
    gpio_set_value(dev->gpioid, 0);          // 手动拉低片选
    ret = spi_sync(spi, &msg);               // 一次性执行两个传输
    gpio_set_value(dev->gpioid, 1);          // 手动拉高片选

    return ret;
}

static unsigned char icm20608_read_onereg(struct icm20608_dev *dev, u8 reg)
{
	u8 data = 0;
	icm20608_read_regs(dev, reg, &data, 1);
	return data;
}

static void icm20608_write_onereg(struct icm20608_dev *dev, u8 reg, u8 value)
{
	u8 buf = value;
	icm20608_write_regs(dev, reg, &buf, 1);
}

void icm20608_reginit(struct icm20608_dev *dev)
{
    u8 value = 0;
	
	icm20608_write_onereg(dev, ICM20_PWR_MGMT_1, 0x80);
	mdelay(50);
	icm20608_write_onereg(dev, ICM20_PWR_MGMT_1, 0x01);
	mdelay(50);

	value = icm20608_read_onereg(dev, ICM20_WHO_AM_I);
	printk("ICM20608 ID = %#X\r\n", value);	

	icm20608_write_onereg(dev, ICM20_SMPLRT_DIV, 0x00); 	/* 输出速率是内部采样率					*/
	icm20608_write_onereg(dev, ICM20_GYRO_CONFIG, 0x18); 	/* 陀螺仪±2000dps量程 				*/
	icm20608_write_onereg(dev, ICM20_ACCEL_CONFIG, 0x18); 	/* 加速度计±16G量程 					*/
	icm20608_write_onereg(dev, ICM20_CONFIG, 0x04); 		/* 陀螺仪低通滤波BW=20Hz 				*/
	icm20608_write_onereg(dev, ICM20_ACCEL_CONFIG2, 0x04); /* 加速度计低通滤波BW=21.2Hz 			*/
	icm20608_write_onereg(dev, ICM20_PWR_MGMT_2, 0x00); 	/* 打开加速度计和陀螺仪所有轴 				*/
	icm20608_write_onereg(dev, ICM20_LP_MODE_CFG, 0x00); 	/* 关闭低功耗 						*/
	icm20608_write_onereg(dev, ICM20_FIFO_EN, 0x00);		/* 关闭FIFO						*/
}

void readdata(struct icm20608_dev *dev)
{
    unsigned char data[14] = { 0 };
	icm20608_read_regs(dev, ICM20_ACCEL_XOUT_H, data, 14);
    
    dev->data.accel_x_adc = (signed short)((data[0] << 8) | data[1]); 
    dev->data.accel_y_adc = (signed short)((data[2] << 8) | data[3]); 
    dev->data.accel_z_adc = (signed short)((data[4] << 8) | data[5]); 
    dev->data.gyro_x_adc = (signed short)((data[6] << 8) | data[7]); 
    dev->data.gyro_y_adc = (signed short)((data[8] << 8) | data[9]); 
    dev->data.gyro_z_adc = (signed short)((data[10] << 8) | data[11]); 
    dev->data.temp_adc = (signed short)((data[12] << 8) | data[13]);

}

static ssize_t icm20608_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
    unsigned int data[7];
    int ret;
    size_t data_size = sizeof(data);
    size_t copy_size;
    struct icm20608_dev *dev = filp->private_data;
    struct icm20608_data *icm20608_data = &dev->data;
    if(cnt == 0) return -EFAULT;
    readdata(dev);
    data[0] = icm20608_data->accel_x_adc;
    data[1] = icm20608_data->accel_y_adc;
    data[2] = icm20608_data->accel_z_adc;
    data[3] = icm20608_data->gyro_x_adc;
    data[4] = icm20608_data->gyro_y_adc;
    data[5] = icm20608_data->gyro_z_adc;
    data[6] = icm20608_data->temp_adc;
    copy_size = (cnt < data_size) ? cnt : data_size;
    ret = copy_to_user(buf, data, copy_size);
    if (ret) {
        printk("Error: %d bytes not copied\n", ret);
        return -EFAULT;
    }
    return copy_size;
}

static ssize_t icm20608_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *off)
{
    return 0;
}

static int icm20608_open(struct inode *inode, struct file *filp)
{
    filp->private_data  = container_of(inode->i_cdev, struct icm20608_dev, cdev);
    return 0;
}

static int icm20608_release (struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations icm20608_opts = {
    .owner = THIS_MODULE,
    .open = icm20608_open,
    .release = icm20608_release,
    .read = icm20608_read,
    .write = icm20608_write,
};

static struct of_device_id icm20608_of_match[] = {
    {.compatible = "alientek,icm20608"},
    {/*sentinel*/}
};

static struct spi_device_id icm20608_spi_id[] = {
    {"alientek-icm20608", 0},
	{}
};

static int icm20608_probe(struct spi_device *spi)
{
    int ret = 0;
    struct icm20608_dev *dev = devm_kzalloc(&spi->dev, sizeof(*dev), GFP_KERNEL);
    if(!dev) return -ENOMEM;
    //get device id
    if(dev->major){
        dev->devid = MKDEV(dev->major, 0);
        ret = alloc_chrdev_region(&dev->devid, 0, MODULE_NUM, MODULE_NAME);
    }else{
        ret = alloc_chrdev_region(&dev->devid, 0, MODULE_NUM, MODULE_NAME);
        dev->major = MAJOR(dev->devid);
        dev->minor = MINOR(dev->devid);
    }
    if(ret < 0){
        printk("error alloc chrdev region\r\n");
        return -1;
    }
    printk("major : %d   minor : %d\r\n", dev->major, dev->minor);
    //bangding device options
    dev->cdev.owner = THIS_MODULE;
    cdev_init(&dev->cdev, &icm20608_opts);
    cdev_add(&dev->cdev, dev->devid, MODULE_NUM);

    dev->class = class_create(THIS_MODULE, MODULE_NAME);
    if(IS_ERR(dev->class)){
        return PTR_ERR(dev->class);
    }
    dev->device = device_create(dev->class, NULL, dev->devid, NULL, MODULE_NAME);
    if(IS_ERR(dev->device)){
        return PTR_ERR(dev->device);
    }

    dev->nd = of_get_parent(spi->dev.of_node);
	dev->gpioid = of_get_named_gpio(dev->nd, "cs-gpios", 0);
    printk("%d\n", dev->gpioid);
	ret = gpio_request(dev->gpioid, "cs");
    printk("%d\n", ret);
    ret = gpio_direction_output(dev->gpioid, 1);
    printk("%d\n", ret);
	dev->m_spi_device = spi;
    icm20608_reginit(dev);
    spi_set_drvdata(spi, dev);
    return ret;
}

static int icm20608_remove(struct spi_device *spi){
    struct icm20608_dev *dev = spi_get_drvdata(spi);
    device_destroy(dev->class, dev->devid);
    class_destroy(dev->class);
    cdev_del(&dev->cdev);
    unregister_chrdev_region(dev->devid, MODULE_NUM);
    gpio_free(dev->gpioid);
    return 0;
}

static struct spi_driver icm20608_driver = {
    .probe = icm20608_probe,
    .remove = icm20608_remove,
    .driver = {
		.name = "alientek,icm20608",
		.owner = THIS_MODULE,
		.of_match_table = icm20608_of_match,
    },
    .id_table = icm20608_spi_id,
};

static int __init icm20608_init(void)
{
    int ret = 0;
    ret = spi_register_driver(&icm20608_driver);
    return ret;
}

static void __exit icm20608_exit(void)
{
    spi_unregister_driver(&icm20608_driver);
}

module_init(icm20608_init);
module_exit(icm20608_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zws");