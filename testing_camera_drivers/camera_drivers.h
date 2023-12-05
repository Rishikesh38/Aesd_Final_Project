/**
​ ​*​ ​@file​ ​camera_drivers.h
​ ​*​ ​@brief​ ​This is the header file for the camera module
​ ​*
​ ​*​ ​This​ ​header​ ​file contains function prototypes​.
​ ​*
​ ​*​ ​@author​ ​Rishikesh Goud Sundaragiri
​ ​*​ ​@date​ Dec 1 2023
​ ​*​ ​
​ ​*/

#ifndef __CAMERA_DRIVERS_H__
#define __CAMERA_DRIVERS_H__

void start_capturing(void);
void uninit_device(void);
void init_device(void);
void close_device(void);
void open_device(void);
void capture_pic(void);
unsigned char *return_pic_buffer();
void stop_capturing(void);

#endif /* __CAMERA_DRIVERS_H__ */