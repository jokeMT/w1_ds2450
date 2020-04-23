/*
 * w1_ds2450.c - 1wire DS2450 driver
 *
 * Copyright (c) 2020 Johannes Kemter <kemter.j@googlemail.com>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2. See the file COPYING for more details.
 *
 * Summary of options:
 * - parasite_mode = 0
 * - parasite_mode = 1
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

static int w1_f20_parasite_mode = 0;
module_param_named(parasite_mode, w1_f20_parasite_mode, int, 0);

#define W1_F20_SYSFS_FILES 4

#define W1_DS2450_FAMILY	0x20
#define W1_F20_READ_MEMORY 	0xAA
#define W1_F20_WRITE_MEMORY 	0x55
#define W1_F20_CONVERT 		0x3C

u8 max_tries = 3;

struct w1_slave_mmap_val {
	char name[28];
	u8 size;
	u8 addr;
	u8 bytes;
	u16 mask;
	u8 shift;
	u8 type;
};

struct w1_slave_mmap {
	u8 addr;
	u8 size;
	u16 output_size;
	u8 values_cnt;
	struct w1_slave_mmap_val values[];
};

#define W1F20_CONVERT_OUT (u16)((8*5)+(8*8))
static struct w1_slave_mmap w1f20_convert_map = {
	0x00, 		//addr
	8, 		//size
	W1F20_CONVERT_OUT, 	//output size
	8,		//values cnt
	{		//values
		 { "fmt_a",    5, 0x00, 2, 0xFFFF, 0, 2},
		 { "raw_a",    5, 0x00, 2, 0xFFFF, 0, 1},
		 { "fmt_b",    5, 0x02, 2, 0xFFFF, 0, 2},
		 { "raw_b",    5, 0x02, 2, 0xFFFF, 0, 1},
		 { "fmt_c",    5, 0x04, 2, 0xFFFF, 0, 2},
		 { "raw_c",    5, 0x04, 2, 0xFFFF, 0, 1},
		 { "fmt_d",    5, 0x06, 2, 0xFFFF, 0, 2},
		 { "raw_d",    5, 0x06, 2, 0xFFFF, 0, 1}
	}
};

#define W1F20_CONTROL_OUT (u16)(568+(36*8))
static struct w1_slave_mmap w1f20_control_map={
	0x08, 		//addr
	8, 		//size
	W1F20_CONTROL_OUT, 	//output size
	36,		//values cnt
	{		//values
		{ "output_control_a",    16, 0x08,1, 0x40,0,0},
		{ "output_enable_a",     15, 0x08,1, 0x80,0,0},
		{ "resolution_a",	 12, 0x08,1, 0x0F,0,1},
		{ "input_range_a",       13, 0x08,1, 0x01,0,0},
		{ "alarm_enable_low_a",  18, 0x09,1, 0x04,0,0},
		{ "alarm_enable_high_a", 19, 0x09,1, 0x08,0,0},
		{ "alarm_flag_low_a",    16, 0x09,1, 0x10,0,0},
		{ "alarm_flag_high_a",   17, 0x09,1, 0x20,0,0},
		{ "power_on_reset_a",    16, 0x09,1, 0x80,0,0},
		{ "output_control_b",    16, 0x0A,1, 0x40,0,0},
		{ "output_enable_b",     15, 0x0A,1, 0x80,0,0},
		{ "resolution_b",	 12, 0x0A,1, 0x0F,0,1},
		{ "input_range_b",       13, 0x0B,1, 0x01,0,0},
		{ "alarm_enable_low_b",  18, 0x0B,1, 0x04,0,0},
		{ "alarm_enable_high_b", 19, 0x0B,1, 0x08,0,0},
		{ "alarm_flag_low_b",    16, 0x0B,1, 0x10,0,0},
		{ "alarm_flag_high_b",   17, 0x0B,1, 0x20,0,0},
		{ "power_on_reset_b",    16, 0x0B,1, 0x80,0,0},
		{ "output_control_c",    16, 0x0C,1, 0x40,0,0},
		{ "output_enable_c",     15, 0x0C,1, 0x80,0,0},
		{ "resolution_c",	 12, 0x0C,1, 0x0F,0,1},
		{ "input_range_c",       13, 0x0D,1, 0x01,0,0},
		{ "alarm_enable_low_c",  18, 0x0D,1, 0x04,0,0},
		{ "alarm_enable_high_c", 19, 0x0D,1, 0x08,0,0},
		{ "alarm_flag_low_c",    16, 0x0D,1, 0x10,0,0},
		{ "alarm_flag_high_c",   17, 0x0D,1, 0x20,0,0},
		{ "power_on_reset_c",    16, 0x0D,1, 0x80,0,0},
		{ "output_control_d",    16, 0x0E,1, 0x40,0,0},
		{ "output_enable_d",     15, 0x0E,1, 0x80,0,0},
		{ "resolution_d",	 12, 0x0E,1, 0x0F,0,1},
		{ "input_range_d",       13, 0x0F,1, 0x01,0,0},
		{ "alarm_enable_low_d",  18, 0x0F,1, 0x04,0,0},
		{ "alarm_enable_high_d", 19, 0x0F,1, 0x08,0,0},
		{ "alarm_flag_low_d",    16, 0x0F,1, 0x10,0,0},
		{ "alarm_flag_high_d",   17, 0x0F,1, 0x20,0,0},
		{ "power_on_reset_d",    16, 0x0F,1, 0x80,0,0}
	}
};

#define W1F20_ALARM_OUT (u16)(92+(8*8))
static struct w1_slave_mmap w1f20_alarm_map={
	0x10, 		//addr
	8, 		//size
	W1F20_ALARM_OUT,	//output size
	8,		//values cnt
	{		//values
		{ "alarm_low_a",   11, 0x10,1, 0xFF,0,1},
		{ "alarm_high_a",  12, 0x11,1, 0xFF,0,1},
		{ "alarm_low_b",   11, 0x12,1, 0xFF,0,1},
		{ "alarm_high_b",  12, 0x13,1, 0xFF,0,1},
		{ "alarm_low_c",   11, 0x14,1, 0xFF,0,1},
		{ "alarm_high_c",  12, 0x15,1, 0xFF,0,1},
		{ "alarm_low_d",   11, 0x16,1, 0xFF,0,1},
		{ "alarm_high_d",  12, 0x17,1, 0xFF,0,1}
	}
};

#define W1F20_VCC_OUT (u16)(3+8)
static struct w1_slave_mmap w1f20_vcc_map={
	0x18, 		//addr
	8, 		//size,
	W1F20_VCC_OUT,		//output size
	1,		//values cnt
	{		//values
		{ "vcc",   3, 0x1C, 1, 0x40,0,0}
	}
};




static inline char w1_f20_read(struct kobject *kobj, u8 *buf, u8 cmd_len, u8 out_len,u8 parasite_mode) {
	u16 crc=0;
	u16 slave_crc=0;
	u8 tries = max_tries;
	char rtn = 0;
	int i=0;
	int len=cmd_len+out_len;
	struct w1_slave *slave = kobj_to_w1_slave(kobj);

	mutex_lock(&slave->master->bus_mutex);
	while(tries--) {
		if (w1_reset_select_slave(slave)) {
			rtn=-1;
			continue;
		}

		if(parasite_mode){
			w1_next_pullup(slave->master,4);
			printk(KERN_INFO"DS2450 strong pullup\n"); //###############debug 
		}

		w1_write_block(slave->master, buf, cmd_len);
		w1_read_block(slave->master, buf+cmd_len, out_len);

		//###############debug 
		for(i=0;i<len;i++){
			printk(KERN_INFO"0x%2x ",buf[i]);
		}
		//###############

		crc = ~crc16(0, buf, len-2);
		slave_crc=(buf[len-2]|(buf[len-1]<<8));
		if(crc^slave_crc){
			printk(KERN_INFO"DS2450 CRC ERROR 0x%x!=0x%x\n",crc,slave_crc); //###############debug 
			rtn=-2;
			continue;
		}
		printk(KERN_INFO"DS2450 CRC OK 0x%x==0x%x\n",crc,slave_crc); //###############debug 
		if(parasite_mode){
			mdelay(4);
			rtn = (w1_read_8(slave->master)!=0xff);
			if (rtn || w1_reset_select_slave(slave)) {
				rtn=-1;
				continue;
			}
		}
		rtn=0;
		break;
	} 
	mutex_unlock(&slave->master->bus_mutex);
	return rtn;
}

static inline char w1_f20_convert(struct kobject *kobj, u8 *buf, u8 cmd_len, u8 out_len) {
	u8 state = 0xFF;
	u8 max_check = 5;
	struct w1_slave *slave = kobj_to_w1_slave(kobj);
	char rtn = w1_f20_read(kobj,buf,cmd_len,out_len,w1_f20_parasite_mode);
	if(rtn!=0){
		return -1;
	}
	if(!w1_f20_parasite_mode) {
		mdelay(4);
		max_check=3;
		mutex_lock(&slave->master->bus_mutex);
		while(((state=w1_read_8(slave->master))!=0xFF) || --max_check<0){
			printk(KERN_INFO"DS2450 s 0x%x\n",state); //#################debug
			udelay(160);
		};
		mutex_unlock(&slave->master->bus_mutex);
	}
	return (state!=0xFF);
}

static inline ssize_t w1_f20_fmt_page(struct w1_slave_mmap *map, u8 *w1_buf, u8 *buf) {
	int i = 0;
	u16 out = 0;
	u16 offset = 0;

	for(i=0;i<map->values_cnt;i++){
		struct w1_slave_mmap_val *val = map->values+i;
		u8 byte_pos=val->addr-map->addr;
		if(val->bytes>1){
			out = (u16)(w1_buf[byte_pos]|(w1_buf[byte_pos+1]<<8));
		} else {
			out = (u16)w1_buf[byte_pos];
		}
		out = (out & val->mask)>>val->shift;
		printk(KERN_INFO"DS2450 ? b1 0x%x b2 0x%x\n",w1_buf[byte_pos],w1_buf[byte_pos+1]); 
		switch(val->type){
			case 0:
				out = (out > 0);
			break;
			case 2:
				out=out*2300/65536;
			break;
		}
		
		if(val->type==2){
			snprintf(buf+offset,val->size+8,"%s=%01d.%-3d\n",val->name, out/1000, out-((int)(out/1000))*1000);
		} else {
			snprintf(buf+offset,val->size+8,"%s=%-5d\n",val->name,out);
		}
		offset+=val->size+8;
	}
	return map->output_size;
}

static ssize_t w1_f20_status_control(struct file *filp, struct kobject *kobj,struct bin_attribute *bin_attr,char *buf, loff_t off, size_t count) {
	u8 w1_buf[13];
	
	if (off != 0) {
		return 0;
	}
	if (!buf || w1f20_control_map.output_size!=count) {
		printk(KERN_INFO"DS2450 s %d\n",w1f20_control_map.output_size);
		return -EINVAL;
	}
	
	memset(w1_buf,0,13);
	w1_buf[0]=W1_F20_READ_MEMORY;
	w1_buf[1]=0x08;
	w1_buf[2]=0;	
	if(w1_f20_read(kobj,w1_buf,3, 10,0)!=0){
		return -EIO;
	}

	return w1_f20_fmt_page(&w1f20_control_map,w1_buf+3,buf);
}

static ssize_t w1_f20_alarm_settings(struct file *filp, struct kobject *kobj,struct bin_attribute *bin_attr,char *buf, loff_t off, size_t count) {
	u8 w1_buf[13];
	
	if (off != 0) {
		return 0;
	}
	if (!buf || w1f20_alarm_map.output_size!=count) {
		return -EINVAL;
	}
	
	memset(w1_buf,0,13);
	w1_buf[0]=W1_F20_READ_MEMORY;
	w1_buf[1]=0x10;
	w1_buf[2]=0;	
	if(w1_f20_read(kobj,w1_buf,3, 10, 0)!=0){
		return -EIO;
	}

	return w1_f20_fmt_page(&w1f20_alarm_map,w1_buf+3,buf);
}

static ssize_t w1_f20_vcc(struct file *filp, struct kobject *kobj,struct bin_attribute *bin_attr,char *buf, loff_t off, size_t count) {
	u8 w1_buf[13];
	
	if (off != 0) {
		return 0;
	}
	if (!buf || w1f20_vcc_map.output_size!=count) {
		return -EINVAL;
	}
	
	memset(w1_buf,0,13);
	w1_buf[0]=W1_F20_READ_MEMORY;
	w1_buf[1]=0x18;
	w1_buf[2]=0;	
	if(w1_f20_read(kobj,w1_buf,3, 10, 0)!=0){
		return -EIO;
	}
	return w1_f20_fmt_page(&w1f20_vcc_map,w1_buf+3,buf);
}

static ssize_t w1_f20_write_vcc(struct file *filp, struct kobject *kobj, struct bin_attribute *bin_attr, char *buf, loff_t off, size_t size){
	u8 w1_buf[13];
	int rtn = 0;
	int val = 0;
	u8 state = 0;
struct w1_slave *slave = kobj_to_w1_slave(kobj);
	if (off != 0) {
		return 0;
	}

	if (!buf || size == 0) {
		return -EINVAL;
	}

	rtn = kstrtoint(buf, 0, &val);
	if (rtn) {
		pr_warn("invalid val\n");
		return -EINVAL;
	}

	if (val < 0 || val > 1) {
		pr_warn("Unsupported vcc\n");
		return -EINVAL;
	}

	memset(w1_buf,0,13);
	

	w1_buf[0]=W1_F20_WRITE_MEMORY;
	w1_buf[1]=0x1C;
	w1_buf[2]=0x00;
	w1_buf[3] = val ? 0x40 : 0x00;
	if(w1_f20_read(kobj,w1_buf,4, 2, 0)!=0){
		return -EIO;
	}

	state=w1_read_8(slave->master);
printk(KERN_INFO"DS2450 wstate %d\n",state);
	if (w1_reset_select_slave(slave)) {
			return -EIO;
	}
	return size;
}


static ssize_t w1_f20_convert_all(struct file *filp, struct kobject *kobj,struct bin_attribute *bin_attr,char *buf, loff_t off, size_t count) {
	u8 w1_buf[13];
	
	if (off != 0) {
		return 0;
	}
	if (!buf || w1f20_convert_map.output_size!=count) {
		return -EINVAL;
	}

	memset(w1_buf,0,13);
	w1_buf[0]=W1_F20_CONVERT;
	w1_buf[1]=0x0F;
	w1_buf[2]=0x55;
	if(w1_f20_convert(kobj,w1_buf,3, 2)!=0){
		return -EIO;
	}

	memset(w1_buf,0,13);
	w1_buf[0]=W1_F20_READ_MEMORY;
	w1_buf[1]=0x00;
	w1_buf[2]=0;	
	if(w1_f20_read(kobj,w1_buf,3, 10, 0)!=0){
		return -EIO;
	}

	return w1_f20_fmt_page(&w1f20_convert_map,w1_buf+3,buf);
}

static struct bin_attribute w1_f20_sysfs[W1_F20_SYSFS_FILES] = {
	{
		.attr = {
			.name = "ad_convert",
			.mode = S_IRUGO,
		},
		.size = W1F20_CONVERT_OUT,
		.read = w1_f20_convert_all,
	},
	{
		.attr = {
			.name = "ad_status_control",
			.mode = S_IRUGO,
		},
		.size = W1F20_CONTROL_OUT,
		.read = w1_f20_status_control,
	},
	{
		.attr = {
			.name = "ad_alarm_settings",
			.mode = S_IRUGO,
		},
		.size = W1F20_ALARM_OUT,
		.read = w1_f20_alarm_settings,
	},
	{
		.attr = {
			.name = "ad_vcc",
			.mode = S_IRUGO| S_IWUSR | S_IWGRP,
		},
		.size = W1F20_VCC_OUT,
		.read = w1_f20_vcc,
		.write = w1_f20_write_vcc
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
