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