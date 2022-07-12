/*
 *  drivers/polyhex/polyhex_gpio.c
 *
 * 	polyhex
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
//#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>


#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

struct polyhex_gpio_data {
	const char *name;
	unsigned gpio[5];
	int irq;
	int gpios_mode;
	int gpios_port_num;
	int input_key;
	int current_fun;
	bool free_irq_flag;
	bool key_flag;
	struct input_dev *key_dev;
	struct delayed_work delayed_work;

	int major;
	char class_name[30];
	struct cdev gpio_cdev;
	struct class *gpio_drv_class;
	struct device *gpio_drv_class_dev;
	struct file_operations gpio_fops;
	struct mutex mutex;
};

static irqreturn_t polyhex_gpio_irq_handler(int irq, void *dev_id);

void free_irq_to_gpio(unsigned int irq, void *dev_id){
	free_irq(irq, dev_id);
}

static void polyhex_gpio_work(struct work_struct *work)
{
	int state;
	struct polyhex_gpio_data	*data =
		container_of(work, struct polyhex_gpio_data, delayed_work.work);

	state = gpio_get_value(data->gpio[0]);

	if(state==1 && data->key_dev!=NULL){
		input_report_key(data->key_dev, data->input_key, 1);
		input_sync(data->key_dev);
		printk("%s:button up(%s): input_key = %d\n",__func__,data->name,data->input_key);
	} else if(state==0 && data->key_dev!=NULL){
		input_report_key(data->key_dev, data->input_key, 0);
		input_sync(data->key_dev);
		printk("%s:button down(%s): input_key = %d\n",__func__,data->name,data->input_key);
	}
}

static void polyhex_gpio_poll_work(struct work_struct *work)
{
	int state;
	struct polyhex_gpio_data	*data =
		container_of(work, struct polyhex_gpio_data, delayed_work.work);

	state = gpio_get_value(data->gpio[0]);
	
	if(data->key_dev!=NULL && data->key_flag && state==0){
		input_report_key(data->key_dev, data->input_key, 1);
		input_sync(data->key_dev);
		//printk("%s:button DOWN(%s): input_key = %d\n",__func__,data->name,data->input_key);
		data->key_flag = false;
	} else if(data->key_dev!=NULL && !data->key_flag && state==1){
		input_report_key(data->key_dev, data->input_key, 0);
		input_sync(data->key_dev);
		//printk("%s:button UP(%s): input_key = %d\n",__func__,data->name,data->input_key);
		data->key_flag = true;
	}

	if(data->free_irq_flag)
		schedule_delayed_work(&data->delayed_work,HZ/50);
}


static irqreturn_t polyhex_gpio_irq_handler(int irq, void *dev_id)
{
	struct polyhex_gpio_data *gpio_data =
	    (struct polyhex_gpio_data *)dev_id;

	schedule_delayed_work(&gpio_data->delayed_work,HZ/50);
	return IRQ_HANDLED;
}

static int input_device_register(struct polyhex_gpio_data *pdata)
	{	
		int ret = -1;

		pdata->key_dev = input_allocate_device();
		if (NULL == pdata->key_dev ) {
			pr_err("fail,  allocate input device\n");
			ret = -ENOMEM;
			goto err_reg;
		}
	
		pdata->key_dev->name = pdata->name;
		pdata->key_dev->phys = "input0";
		pdata->key_dev->id.bustype = BUS_HOST;
		pdata->key_dev->id.vendor = 0x0001;
		pdata->key_dev->id.product = 0x0002;
		pdata->key_dev->id.version = 0x0100;
		pdata->key_dev->evbit[0] = BIT_MASK(EV_KEY);

		printk("...code:%d...\r\n",pdata->input_key);
		__set_bit( pdata->input_key, pdata->key_dev->keybit);
	
		ret = input_register_device(pdata->key_dev);
		if (ret) {
			goto err_reg;
		}
	
		printk("..reg success %s, key name(%s), key value(%d)\r\n",__func__,pdata->name,pdata->input_key);
		goto normal;
				
err_reg:
		input_free_device(pdata->key_dev);
		pdata->key_dev = NULL;
normal:
		return ret;
}

static int gpio_open(struct inode *inode, struct file *filp)
{
	struct polyhex_gpio_data *pdata = container_of(inode->i_cdev, struct polyhex_gpio_data, gpio_cdev);
	filp->private_data = pdata;
    return 0;
}

static int gpio_release(struct inode *inode, struct file *filp)
{	
	return 0;
}

static ssize_t gpio_read (struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	struct polyhex_gpio_data *gpio_data = (struct polyhex_gpio_data *)filp->private_data;
    ssize_t result = 0;
 	char read_data[2] = {0};

	mutex_lock(&gpio_data->mutex);
	
	count = 2;
	
	if(gpio_data->gpios_mode >= 3 && gpio_data->free_irq_flag){
		if(gpio_data->gpios_mode==3){
			free_irq_to_gpio(gpio_data->irq, gpio_data);
			input_free_device(gpio_data->key_dev);
			printk("%s: free %s --> %d\n",__func__,gpio_data->name,read_data[0]);
		}
		gpio_data->free_irq_flag = false;
		gpio_direction_input(gpio_data->gpio[0]);
	}
	
	if(gpio_data->current_fun!=0){
		gpio_direction_input(gpio_data->gpio[0]);
		gpio_data->current_fun = 0;
	}
	
	read_data[0] = gpio_get_value(gpio_data->gpio[0]);
	
	if(read_data[0]==1){
		read_data[0] = '1';
	} else {
		read_data[0] = '0';
	}
	read_data[1] = '\0';
 	//printk("%s: fun:%d  %s --> %d\n",__func__,gpio_data->current_fun,gpio_data->name,read_data[0]);
    if (copy_to_user(buff, &read_data, count)) {
        result = -EFAULT;
    }
    else{
        result = count;
    }

	mutex_unlock(&gpio_data->mutex); 
	
    return result;
}

static ssize_t  gpio_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	//printk("%s %s\n", __func__, buf);
	struct polyhex_gpio_data *gpio_data = (struct polyhex_gpio_data *)filp->private_data;
	char wrdata[2] = {0};
	int v = 0, i = 0;
	int ret = -1;
	
	mutex_lock(&gpio_data->mutex);
	
	if(gpio_data==NULL){
		printk("%s: gpio_data is NULL\n",__func__);
		return -1;
	}

	if(copy_from_user(&wrdata, buf, count)){
		printk("%s: write error (%s)\n",__func__, gpio_data->name);
        return -1;
	}

	ret = sscanf(&wrdata[0],"%d",&v);
	//printk("func=%d, %s=%d, ret=%d, mode=%d\n",gpio_data->current_fun,gpio_data->name,v, ret, gpio_data->gpios_mode);
	if(gpio_data->gpios_mode >= 3 && gpio_data->free_irq_flag){
		if(gpio_data->gpios_mode==3){
			free_irq_to_gpio(gpio_data->irq, gpio_data);
			input_free_device(gpio_data->key_dev);
			printk("%s: free %s\n",__func__,gpio_data->name);
		}
		gpio_data->free_irq_flag = false;
	}

	if(ret==1){
		ret = 0;
		if(v==1 || v==0){
			gpio_data->current_fun = 1;
			for(i=0; i<gpio_data->gpios_port_num; i++){
				gpio_direction_output(gpio_data->gpio[i], v);
				
			printk("%s gpio %d out %d\n", gpio_data->name,gpio_data->gpio[i],v);
			}
		}else if(gpio_data->gpios_mode >= 3 && v==2){
			gpio_direction_input(gpio_data->gpio[0]);
			gpio_data->free_irq_flag = true;
			gpio_data->current_fun = 2;
			if(gpio_data->gpios_mode==3){
				ret = request_irq(gpio_data->irq, polyhex_gpio_irq_handler,
				  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, gpio_data->name, gpio_data);
				input_device_register(gpio_data);
				if (ret < 0)
					return ret;
			} else {
				schedule_delayed_work(&gpio_data->delayed_work,HZ/50);
			}
		} else if(v==3){
			gpio_data->current_fun = 0;
			gpio_direction_input(gpio_data->gpio[0]);
		} else {
			printk("%s gpio control failed\n", gpio_data->name);
		}
	}

	mutex_unlock(&gpio_data->mutex); 
	
	return count;
}

static int gpio_register_chrdev(struct polyhex_gpio_data *pdata,struct device *dev, const char *name)
{
	dev_t devno = MKDEV(pdata->major,0);
	sprintf(pdata->class_name, "%s_class", name);
	
	pdata->major = 0;
	pdata->gpio_fops.owner = THIS_MODULE;
	pdata->gpio_fops.open = gpio_open;
	pdata->gpio_fops.write = gpio_write;
	pdata->gpio_fops.read = gpio_read;
	pdata->gpio_fops.release = gpio_release;
#if 0	
 	pdata->major = register_chrdev(0, pdata->class_name, &pdata->gpio_fops);
	if(pdata->major < 0){
        dev_err(dev, "failes register_chrdev %d\n",pdata->major);
		pdata->major = 0;
		return pdata->major;
	}
#else
	if (alloc_chrdev_region(&devno, 0, 1, pdata->class_name) < 0)
	{
		printk(KERN_INFO "..alloc..reg err.\r\n");				
		return -1;
	}
	pdata->major = MAJOR(devno); //get the major after alloc register
	pdata->gpio_cdev.owner = THIS_MODULE;	
	cdev_init(&pdata->gpio_cdev, &pdata->gpio_fops);	
	if(cdev_add(&pdata->gpio_cdev,MKDEV(pdata->major,0),1) < 0)
	{
		printk("..add err.\r\n");
		return -1;
	}	
#endif
	pdata->gpio_drv_class = class_create(THIS_MODULE, pdata->class_name);
	if(IS_ERR(pdata->gpio_drv_class)){
		dev_err(dev,"failes 2 wg_drv register\n");
		goto gpio_class_create_fail;
	}

	//pdata->gpio_drv_class_dev = device_create(pdata->gpio_drv_class, NULL, MKDEV(pdata->major,0), 0, name);
	pdata->gpio_drv_class_dev = device_create(pdata->gpio_drv_class, NULL, devno, 0, name);
	if(IS_ERR(pdata->gpio_drv_class_dev)){
		dev_err(dev,"failes  wg_drv register\n");
		goto gpio_dev_create_fail;
	}
	printk("%s: name=%s, class_name=%s, pdata->major=%d\n",__func__, name, pdata->class_name, pdata->major);
	return 0;

gpio_dev_create_fail:
	device_destroy(pdata->gpio_drv_class,MKDEV(pdata->major,0));
	class_destroy(pdata->gpio_drv_class);	
gpio_class_create_fail:
	unregister_chrdev(pdata->major,pdata->class_name);
	pdata->major = 0;

	return -1;
}

void gpio_unregister_chrdev(struct polyhex_gpio_data *pdata)
{
	printk("%s \n", __func__);
	if(pdata->major){
		device_destroy(pdata->gpio_drv_class,MKDEV(pdata->major,0));
		class_destroy(pdata->gpio_drv_class);	
		unregister_chrdev(pdata->major,pdata->class_name);
		pdata->major = 0;
	}
}


static int polyhex_gpio_probe(struct platform_device *pdev)
{
	//struct gpio_switch_platform_data *pdata = pdev->dev.platform_data;
	struct polyhex_gpio_data *gpio_data;
	struct device * dev = &pdev->dev;
	struct device_node	*of_node = dev->of_node;
	int ret = 0;
	char gpio_name[20];
	int i;
	//enum of_gpio_flags gpio_flag;

	gpio_data = devm_kzalloc(dev, sizeof(struct polyhex_gpio_data), GFP_KERNEL);

	
	if (!gpio_data)
		return -ENOMEM;

	ret = of_property_read_u32(of_node, "gpios_mode",&gpio_data->gpios_mode);
	if(ret) {
		dev_err(dev,"failed to get gpios_mode\n");
		return ret;
	}

	ret = of_property_read_string_index(of_node,"gpios_name",0,&gpio_data->name);
	//printk("%s: name=%s\n",__func__,gpio_data->name);
	if (ret ) {
		dev_err(dev,"failed to get gpio name\n");
		return ret;
	}

	ret = of_property_read_u32(of_node,"gpios_port_num",&gpio_data->gpios_port_num);
	//printk("%s: gpios_port_num=%d\n",__func__,gpio_data->gpios_port_num);
	if (ret ) {
		dev_err(dev,"failed to get gpio gpios_port_num\n");
		return ret;
	}

	for(i=0; i<gpio_data->gpios_port_num; i++){ 
		sprintf(gpio_name, "gpios_pin%d", i);
		gpio_data->gpio[i] = of_get_named_gpio(of_node, gpio_name, 0);
		if (!gpio_is_valid(gpio_data->gpio[i])) {
			dev_err(dev,"gpio(%d) error\n",gpio_data->gpio[i]);
			return gpio_data->gpio[i] ;
		}
		sprintf(gpio_name, "%s%d", gpio_data->name, i);
		ret = devm_gpio_request(dev, gpio_data->gpio[i], gpio_name);
		if (ret < 0)
			goto err_request_gpio;
		//printk("%s: num=%d request gpio_name=%s\n",__func__,i,gpio_name);
	}

	if(gpio_data->gpios_mode==0 || gpio_data->gpios_mode==1 || gpio_data->gpios_mode==2 || gpio_data->gpios_mode==3){
		gpio_register_chrdev(gpio_data, dev, gpio_data->name);
	}

	gpio_data->free_irq_flag = true;
	platform_set_drvdata(pdev,gpio_data);

	switch (gpio_data->gpios_mode)
	{
		case 0:
		case 5:
				gpio_data->current_fun = 0;
				for(i=0; i<gpio_data->gpios_port_num; i++)
					gpio_direction_input(gpio_data->gpio[i]);
				break;
		case 1:
				gpio_data->current_fun = 1;
				for(i=0; i<gpio_data->gpios_port_num; i++)
					gpio_direction_output(gpio_data->gpio[i], 0);

				if(strstr(gpio_data->name,"reset")){
					if(gpio_data->gpios_port_num==2){
						msleep(200);
						gpio_direction_output(gpio_data->gpio[1], 1);
						msleep(200);
						gpio_direction_output(gpio_data->gpio[1], 0);
					} else {
						msleep(100);
						gpio_direction_output(gpio_data->gpio[0], 1);
						msleep(100);
						gpio_direction_output(gpio_data->gpio[0], 0);
					}
				
				}
				break;
		case 2:
				gpio_data->current_fun = 1;
				for(i=0; i<gpio_data->gpios_port_num; i++){
					gpio_direction_output(gpio_data->gpio[i], 1);
				}

				if(strstr(gpio_data->name,"reset")){
					if(gpio_data->gpios_port_num==2){
						msleep(200);
						gpio_direction_output(gpio_data->gpio[1], 0);
						msleep(200);
						gpio_direction_output(gpio_data->gpio[1], 1);
					} else {
						msleep(100);
						gpio_direction_output(gpio_data->gpio[0], 0);
						msleep(100);
						gpio_direction_output(gpio_data->gpio[0], 1);
					}
				
				}
				break;
		case 3:
				gpio_data->current_fun = 2;
				ret = of_property_read_u32(of_node, "gpios_key",&gpio_data->input_key);
				if(ret) {
					dev_err(dev,"failed to get gpios_key\n");
					return ret;
				}
				//gpio_direction_output(gpio_data->gpio[0], 0);
				ret = gpio_direction_input(gpio_data->gpio[0]);
				if (ret < 0)
					goto err_set_gpio_input;
				INIT_DELAYED_WORK(&gpio_data->delayed_work, polyhex_gpio_work);
				gpio_data->irq = gpio_to_irq(gpio_data->gpio[0]);
				if (gpio_data->irq < 0) {
					ret = gpio_data->irq;
					goto err_detect_irq_num_failed; 
				}
				ret = request_irq(gpio_data->irq, polyhex_gpio_irq_handler,
						  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, gpio_data->name, gpio_data);
				if (ret < 0)
					goto err_request_irq;
				input_device_register(gpio_data);
				/* Perform initial detection */
				polyhex_gpio_work(&gpio_data->delayed_work.work);
				break;
		case 4:
				gpio_data->current_fun = 2;
				ret = of_property_read_u32(of_node, "gpios_key",&gpio_data->input_key);
				if(ret) {
					dev_err(dev,"failed to get gpios_key\n");
					return ret;
				}
				ret = gpio_direction_input(gpio_data->gpio[0]);
				if (ret < 0)
					goto err_set_gpio_input;
				INIT_DELAYED_WORK(&gpio_data->delayed_work, polyhex_gpio_poll_work);
				ret = input_device_register(gpio_data);
				if(ret!=0){
					dev_err(dev,"failed to input_device_register\n");
					goto err_request_irq;
				}
				gpio_data->key_flag = true;
				schedule_delayed_work(&gpio_data->delayed_work,HZ*5);
				break;
		default:
				break;
	}

	mutex_init(&gpio_data->mutex);
	
	//printk("%s: end (HZ=%d)\n",__func__,HZ);

	return 0;

err_request_irq:
err_detect_irq_num_failed:
err_set_gpio_input:
	for(i=0; i<gpio_data->gpios_port_num; i++){
		gpio_free(gpio_data->gpio[i]);
	}
err_request_gpio:

	//kfree(gpio_data);
	
	return ret;
}

static int polyhex_gpio_remove(struct platform_device *pdev)
{
	int i = 0;
	struct polyhex_gpio_data *gpio_data = platform_get_drvdata(pdev);

	gpio_unregister_chrdev(gpio_data);
	
	if(gpio_data->gpios_mode >= 2){
		input_free_device(gpio_data->key_dev);
	}
	cancel_delayed_work_sync(&gpio_data->delayed_work);
	for(i=0; i<gpio_data->gpios_port_num; i++){
		gpio_free(gpio_data->gpio[i]);
	}
	//kfree(gpio_data);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int polyhex_gpio_suspend(struct device *dev)
{
     printk("%s \n",__func__);
     return 0;
}
static int polyhex_gpio_resume(struct device *dev)
{
	printk("%s \n",__func__);
	return 0;
}

static const struct dev_pm_ops polyhex_gpio_pm_ops = {
	.suspend        = polyhex_gpio_suspend,
	.resume         = polyhex_gpio_resume,
};
#endif

#ifdef CONFIG_OF
static struct of_device_id polyhex_gpio_of_match[] = {
	{ .compatible = "polyhex-gpio" },
	{ }
};
MODULE_DEVICE_TABLE(of, polyhex_gpio_of_match);
#endif

static struct platform_driver polyhex_gpio_driver = {
	.probe		= polyhex_gpio_probe,
	.remove		= polyhex_gpio_remove,
	.driver		= {
		.name	= "polyhex_gpio",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm		= &polyhex_gpio_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(polyhex_gpio_of_match),
#endif
	},
};

static int __init polyhex_gpio_init(void)
{
	return platform_driver_register(&polyhex_gpio_driver);
}

static void __exit polyhex_gpio_exit(void)
{
	platform_driver_unregister(&polyhex_gpio_driver);
}

module_init(polyhex_gpio_init);
module_exit(polyhex_gpio_exit);

MODULE_AUTHOR("624157905@qq.com");
MODULE_DESCRIPTION("Polyhex GPIO driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:polyhex gpio");
