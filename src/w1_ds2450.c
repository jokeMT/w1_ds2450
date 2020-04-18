/*
 * w1_ds2450.c - 1wire DS2450 driver
 *
 * Copyright (c) 2020 Johannes Kemter <kemter.j@googlemail.com>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2. See the file COPYING for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/crc16.h>
#include <linux/uaccess.h>
#include <linux/w1.h>

#define W1_DS2450_FAMILY	0x20

#define W1_F20_READ_MEMORY 	0xAA
#define W1_F20_WRITE_MEMORY 	0x55
#define W1_F20_CONVERT 		0x3C

#define W1_F20_SYSFS_FILES 1
static struct bin_attribute w1_f20_sysfs[W1_F20_SYSFS_FILES] = {
	{
		.attr = {
			.name = "convert_all",
			.mode = S_IRUGO,
		},
		.size = 1,
		.read = w1_f12_read_state,
	}
};

static char w1_f20_read(struct kobject *kobj, u8 *buf, u8 cmd_len, u8 out_len) __attribute__((always_inline)) {
	u16 crc=0;
	u16 slave_crc=0;
	struct w1_slave *slave = kobj_to_w1_slave(kobj);
	mutex_lock(&sl->master->bus_mutex);
	if (w1_reset_select_slave(slave)) {
		mutex_unlock(&slave->master->bus_mutex);
		return -1;
	}
	w1_write_block(slave->master, buf, cmd_len);
	w1_read_block(slave->master, buf+cmd_len, out_len);
	crc16(crc, buf, cmd_len+out_len-3);
	slave_crc=ret[cmd_len+out_len-2]|(buf[cmd_len+out_len-1]<<8);
	printk(KERN_INFO "0x%x == 0x%x\n",crc,slave_crc);
	//todo check crc
	return 0;
}


static ssize_t w1_f20_convert_all(struct file *filp, struct kobject *kobj,struct bin_attribute *bin_attr,char *buf, loff_t off, size_t count) {
	if (off != 0)
		return 0;
	if (!buf)
		return -EINVAL;
	u8 buf[13];
	
	memset(buf,0,13);
	buf[0]=W1_F20_CONVERT;
	buf[1]=0x0F;
	W1_f20_read(kobj,buf,3, 10);
	mdelay(4); //(ch * res * 80us) + 160 us


	memset(buf,0,3);
	buf[0]=W1_f20_READ_MEMORY;
	buf[1]=0;
	buf[2]=0;	
	W1_f20_read(kobj,buf,3, 10);

	u16 ch_a =ret[3]|(buf[4]<<8);
	u16 ch_b =ret[5]|(buf[6]<<8);
	u16 ch_c =ret[7]|(buf[8]<<8);
	u16 ch_d =ret[9]|(buf[10]<<8);
	return -EIO;

}


static int w1_f20_add_slave(struct w1_slave *slave){
	int err = 0;
	int i = 0;

	for(i = 0; i < W1_f20_SYSFS_FILES && !err; ++i){
		err = sysfs_create_bin_file(&sl->dev.kobj,&(w1_f20_sysfs[i]));
	}
		
	if(err){
		while (--i >= 0){
			sysfs_remove_bin_file(&sl->dev.kobj,&(w1_f20_sysfs[i]));
		}
	}
	return err;
}

static void w1_f20_remove_slave(struct w1_slave *slave){
	int i=0;
	for (i = W1_f20_SYSFS_FILES - 1; i >= 0; --i){
		sysfs_remove_bin_file(&sl->dev.kobj,&(w1_f20_sysfs[i]));
	}
}

static struct w1_family_ops w1_f20_fops = {
	.add_slave      = w1_f20_add_slave,
	.remove_slave   = w1_f20_remove_slave,

};

static struct w1_family w1_f20 = {
	.fid = W1_f20_FAMILY,
	.fops = &w1_f20_fops,
};

module_w1_family(w1_f20);

MODULE_AUTHOR("Johannes Kemter <kemter.j@googlemail.com>");
MODULE_DESCRIPTION("1wire DS2450 driver");
MODULE_LICENSE("GPL");
