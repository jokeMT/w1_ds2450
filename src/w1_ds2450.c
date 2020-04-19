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

#define W1_F20_SYSFS_FILES 2

#define W1_DS2450_FAMILY	0x20
#define W1_F20_READ_MEMORY 	0xAA
#define W1_F20_WRITE_MEMORY 	0x55
#define W1_F20_CONVERT 		0x3C

#define CH_CNT 4
char ch_names[CH_CNT]= { 'a','b','c','d' };

#define CTRL_PAGE_CNT 9
#define CTRL_PAGE_SIZE ((124+4+(7*9))*4)

struct page_def {
	char name[18];
	u8 size;
	u16 mask;
	u8 type;
};

struct page_def ctrl_page[CTRL_PAGE_CNT] = {
	{ "output_control",    14, 0x0040,0},
	{ "output_enable",     13, 0x0080,0},
	{ "resolution",	       10, 0x000F,1},
	{ "input_range",       11, 0x0100,0},
	{ "alarm_enable_low",  16, 0x0400,0},
	{ "alarm_enable_high", 17, 0x0800,0},
	{ "alarm_flag_low",    14, 0x1000,0},
	{ "alarm_flag_high",   15, 0x2000,0},
	{ "power_on_reset",    14, 0x8000,0}
};

static char w1_f20_read(struct kobject *kobj, u8 *buf, u8 cmd_len, u8 out_len) {
	
	u16 crc=0;
	u16 slave_crc=0;

	int i=0;
	int len=cmd_len+out_len;
	
	struct w1_slave *slave = kobj_to_w1_slave(kobj);
	mutex_lock(&slave->master->bus_mutex);

	if (w1_reset_select_slave(slave)) {
		mutex_unlock(&slave->master->bus_mutex);
		return -1;
	}

	w1_next_pullup(slave->master, 750);
	w1_write_block(slave->master, buf, cmd_len);

	w1_read_block(slave->master, buf+cmd_len, out_len);
	crc = crc16(crc, buf, len-2);
	slave_crc=(buf[len-1]|(buf[len-2]<<8));
	for(i=0;i<len;i++){
		printk(KERN_INFO"0x%2x ",buf[i]);
	}
	printk(KERN_INFO"\n0x%x == 0x%x %d\n",crc,slave_crc,len);
	//todo check crc
	mutex_unlock(&slave->master->bus_mutex);
	return 0;
}


static ssize_t w1_f20_status_control(struct file *filp, struct kobject *kobj,struct bin_attribute *bin_attr,char *buf, loff_t off, size_t count) {
	u8 w1_buf[13];
	int i = 0;
	int j = 0;
	int offset = 0;
	u16 val = 0;
	u16 out = 0;
	
	if (off != 0)
		return 0;
	if (!buf)
		return -EINVAL;

	
	memset(w1_buf,0,13);
	w1_buf[0]=W1_F20_READ_MEMORY;
	w1_buf[1]=0x08;
	w1_buf[2]=0;	
	w1_f20_read(kobj,w1_buf,3, 10);//todo return -EIO;
	buf[0]=0x31;
	
	
	for(i=0;i<CH_CNT;i++){
		val = (u16)(w1_buf[(i*2)+3]|(w1_buf[(i*2)+4]<<8));
		for(j=0;j<CTRL_PAGE_CNT;j++){
			out = val & ctrl_page[j].mask;
			if(ctrl_page[j].type==0){
				out = (out > 0);
			}
			snprintf(buf+offset,ctrl_page[j].size+7,"%s_%c=%-2d\n",ctrl_page[j].name,ch_names[i],out);
			offset += ctrl_page[j].size+7;
		}
		snprintf(buf+(offset++),2,"\n");
	}
	return CTRL_PAGE_SIZE;
}


static ssize_t w1_f20_convert_all(struct file *filp, struct kobject *kobj,struct bin_attribute *bin_attr,char *buf, loff_t off, size_t count) {
	u16 ch_a = 0;
	u16 ch_b = 0;
	u16 ch_c = 0;
	u16 ch_d = 0;
	u8 w1_buf[13];

	if (off != 0)
		return 0;
	if (!buf)
		return -EINVAL;

	
	memset(w1_buf,0,13);
	w1_buf[0]=W1_F20_CONVERT;
	w1_buf[1]=0x0f;
	w1_buf[2]=0x00;
	w1_f20_read(kobj,w1_buf,3, 2);
	
	mdelay(4); //(ch * res * 80us) + 160 us

	memset(w1_buf,0,13);
	w1_buf[0]=W1_F20_READ_MEMORY;
	w1_buf[1]=0;
	w1_buf[2]=0;	
	w1_f20_read(kobj,w1_buf,3, 10);

	//todo check 2.56V or 5.12Vt	
	ch_a =(u16)w1_buf[3]|(w1_buf[4]<<8)*256/65536;
	ch_b =(u16)w1_buf[5]|(w1_buf[6]<<8)*256/65536;
	ch_c =(u16)w1_buf[7]|(w1_buf[8]<<8)*195/65536;
	ch_d =(u16)w1_buf[9]|(w1_buf[10]<<8)*256/65536;

	snprintf(buf,37,"A: %5d\nB: %5d\nC: %5d\nD: %5d\n",ch_a,ch_b,ch_c,ch_d);
	return 37;
	//return -EIO;

}

static struct bin_attribute w1_f20_sysfs[W1_F20_SYSFS_FILES] = {
	{
		.attr = {
			.name = "convert_all",
			.mode = S_IRUGO,
		},
		.size = 37,
		.read = w1_f20_convert_all,
	},
	{
		.attr = {
			.name = "status_control",
			.mode = S_IRUGO,
		},
		.size = CTRL_PAGE_SIZE,
		.read = w1_f20_status_control,
	}
};

static int w1_f20_add_slave(struct w1_slave *slave){
	int err = 0;
	int i = 0;

	for(i = 0; i < W1_F20_SYSFS_FILES && !err; ++i){
		err = sysfs_create_bin_file(&slave->dev.kobj,&(w1_f20_sysfs[i]));
	}
		
	if(err){
		while (--i >= 0){
			sysfs_remove_bin_file(&slave->dev.kobj,&(w1_f20_sysfs[i]));
		}
	}
	return err;
}

static void w1_f20_remove_slave(struct w1_slave *slave){
	int i=0;
	for (i = W1_F20_SYSFS_FILES - 1; i >= 0; --i){
		sysfs_remove_bin_file(&slave->dev.kobj,&(w1_f20_sysfs[i]));
	}
}

static struct w1_family_ops w1_f20_fops = {
	.add_slave      = w1_f20_add_slave,
	.remove_slave   = w1_f20_remove_slave,

};

static struct w1_family w1_f20 = {
	.fid = W1_DS2450_FAMILY,
	.fops = &w1_f20_fops,
};

module_w1_family(w1_f20);

MODULE_AUTHOR("Johannes Kemter <kemter.j@googlemail.com>");
MODULE_DESCRIPTION("1wire DS2450 driver");
MODULE_LICENSE("GPL");
