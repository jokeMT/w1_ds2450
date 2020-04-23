
# w1_ds2450 
1-Wire Quad A/D Converter
  
new Linux Kernel Module for 1wire chip DS2450

## read

### convert
/sys/bus/w1/devices/20-0000000c0f12/ad_convert 
```
fmt_a=0.0
raw_a=0 
fmt_b=0.0
raw_b=0 
fmt_c=1.823
raw_c=51968
fmt_d=0.0
raw_d=0 
```

### VCC
/sys/bus/w1/devices/20-*/ad_vcc 
```
vcc=1   
```

### alarm settings
/sys/bus/w1/devices/20-0000000c0f12/ad_alarm_settings 
```
alarm_low_a=0    
alarm_high_a=255  
alarm_low_b=0    
alarm_high_b=255  
alarm_low_c=0    
alarm_high_c=255  
alarm_low_d=0    
alarm_high_d=255  
```

### status/control
/sys/bus/w1/devices/20-0000000c0f12/ad_status_control 
```
output_control_a=0    
output_enable_a=0    
resolution_a=8    
input_range_a=0    
alarm_enable_low_a=1    
alarm_enable_high_a=1    
alarm_flag_low_a=0    
alarm_flag_high_a=0    
power_on_reset_a=1    
output_control_b=0    
output_enable_b=0    
resolution_b=8    
input_range_b=0    
alarm_enable_low_b=1    
alarm_enable_high_b=1    
alarm_flag_low_b=0    
alarm_flag_high_b=0    
power_on_reset_b=1    
output_control_c=0    
output_enable_c=0    
resolution_c=8    
input_range_c=0    
alarm_enable_low_c=1    
alarm_enable_high_c=1    
alarm_flag_low_c=0    
alarm_flag_high_c=0    
power_on_reset_c=1    
output_control_d=0    
output_enable_d=0    
resolution_d=8    
input_range_d=0    
alarm_enable_low_d=1    
alarm_enable_high_d=1    
alarm_flag_low_d=0    
alarm_flag_high_d=0    
power_on_reset_d=1   
```

