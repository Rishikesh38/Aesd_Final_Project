/**
 * @file camera_drivers.c
 * @brief Adapted by Rishikesh Sundaragiri for use with C270 web camera and
 *        Starter code by Sam Siewert(RTES).
 * The original code adapted was open source from V4L2 API and had the
 * following use and incorporation policy:
 *
 * This program can be used and distributed without restrictions.
 *
 * This program is provided with the V4L2 API.
 * See http://linuxtv.org/docs.php for more information.
 *
 * @author Rishikesh Goud Sundaragiri
 * @date  30th Nov 2023
 */




#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <syslog.h>
#include <getopt.h>            
#include <fcntl.h>              
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include "camera_drivers.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define HRES 640
#define VRES 480


static struct v4l2_format fmt;

struct buffer 
{
        void   *start;
        size_t  length;
};






static char            *dev_name;
static int              fd = -1;
struct buffer          *buffers;
static unsigned int     n_buffers;
struct v4l2_buffer buf_service;
unsigned char bigbuffer[(1280*960)];

/**
 * @brief   Handle an error and exit the program.
 *
 * This function prints an error message along with the provided string `s`,
 * the error code stored in `errno`, and the corresponding error description
 * obtained from `strerror(errno)`. Then, it exits the program with a failure
 * status code.
 *
 * @param   s   A string describing the context of the error.
 *
 * @return  This function does not return a value, as it exits the program.
 */
static void errno_exit(const char *s)
{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}

/**
 * @brief   Perform an ioctl operation on a file descriptor.
 *
 * This function performs an ioctl operation on the given file descriptor `fh`
 * using the provided request code `request` and the argument `arg`. It retries
 * the ioctl call if it is interrupted by a signal and the error code is `EINTR`.
 *
 * @param   fh      The file descriptor on which to perform the ioctl operation.
 * @param   request The request code for the desired operation.
 * @param   arg     A pointer to the argument data required for the operation.
 *
 * @return  The return value of the ioctl call. If the operation is successful,
 *          the return value can vary based on the specific ioctl request. In
 *          case of an error, -1 is returned, and the error code can be checked
 *          through the global `errno` variable.
 */
static int xioctl(int fh, int request, void *arg)
{
        int r;

        do 
        {
            r = ioctl(fh, request, arg);

        } while (-1 == r && EINTR == errno);

        return r;
}


/**
 * @brief   Perform color conversion from YUV to RGB.
 *
 * This function performs color conversion from YUV color space to RGB color space.
 * Given the Y, U, and V values, it calculates the corresponding RGB values and
 * stores them in the provided pointers `r`, `g`, and `b`. The conversion is done
 * using integer arithmetic to avoid floating-point operations.
 *
 * @param   y   Y component value.
 * @param   u   U component value.
 * @param   v   V component value.
 * @param   r   Pointer to store the resulting red component value.
 * @param   g   Pointer to store the resulting green component value.
 * @param   b   Pointer to store the resulting blue component value.
 *
 * @return  This function does not return a value.
 */
void transformation_color_conversion(int y, int u, int v, unsigned char *r, unsigned char *g, unsigned char *b)
{
   int r1, g1, b1;

   // replaces floating point coefficients
   int c = y-16, d = u - 128, e = v - 128;       

   // Conversion that avoids floating point
   r1 = (298 * c           + 409 * e + 128) >> 8;
   g1 = (298 * c - 100 * d - 208 * e + 128) >> 8;
   b1 = (298 * c + 516 * d           + 128) >> 8;

   // Computed values may need clipping.
   if (r1 > 255) r1 = 255;
   if (g1 > 255) g1 = 255;
   if (b1 > 255) b1 = 255;

   if (r1 < 0) r1 = 0;
   if (g1 < 0) g1 = 0;
   if (b1 < 0) b1 = 0;

   *r = r1 ;
   *g = g1 ;
   *b = b1 ;
}


/**
 * @brief   Perform continuous color transformation on input data.
 *
 * This function performs a continuous color transformation on the provided input
 * data `p` of size `size`. The input data is processed in blocks of four elements,
 * where each block consists of Y, U, Y2, and V values. For each block, the
 * transformation_color_conversion function is called to convert YUV to RGB values,
 * and the resulting RGB values are stored in the `bigbuffer`.
 *
 * @param   p       Pointer to the input data.
 * @param   size    Size of the input data in bytes.
 *
 * @return  This function does not return a value.
 */
void continuous_transformation(const unsigned char *p, int size)
{
    int i, newi, newsize=0;
    int y_temp, y2_temp, u_temp, v_temp;
    unsigned char *pptr = (unsigned char *)p;
    for(int i=0, newi=0; i<size; i=i+4, newi=newi+6)
    {
        y_temp=(int)pptr[i]; u_temp=(int)pptr[i+1]; y2_temp=(int)pptr[i+2]; v_temp=(int)pptr[i+3];
        transformation_color_conversion(y_temp, u_temp, v_temp, &bigbuffer[newi], &bigbuffer[newi+1], &bigbuffer[newi+2]);
        transformation_color_conversion(y2_temp, u_temp, v_temp, &bigbuffer[newi+3], &bigbuffer[newi+4], &bigbuffer[newi+5]);
    }
}

/**
 * @brief   Stop capturing video frames from the device.
 *
 * This function stops the video capturing stream by calling the VIDIOC_STREAMOFF
 * ioctl operation on the file descriptor `fd` associated with the device. It sets
 * the buffer type to V4L2_BUF_TYPE_VIDEO_CAPTURE before invoking the ioctl call.
 * If the ioctl call returns an error, the errno_exit function is called with an
 * appropriate error message.
 *
 * @return  This function does not return a value.
 */
void stop_capturing(void)
{
        enum v4l2_buf_type type;
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
                errno_exit("VIDIOC_STREAMOFF");
}

/**
 * @brief   Start capturing video frames from the device.
 *
 * This function starts the video capturing stream by performing the following steps:
 * - Allocates buffers for capturing video frames.
 * - Enqueues the allocated buffers using the VIDIOC_QBUF ioctl operation.
 * - Sets the buffer type to V4L2_BUF_TYPE_VIDEO_CAPTURE.
 * - Calls VIDIOC_STREAMON ioctl operation to start capturing.
 * If any ioctl call returns an error, the errno_exit function is used to handle
 * the error with an appropriate error message.
 *
 * @return  This function does not return a value.
 */
void start_capturing(void)
{
        unsigned int i;
        enum v4l2_buf_type type;
        for (i = 0; i < n_buffers; ++i) 
        {
                ("allocated buffer %d\n", i);
                struct v4l2_buffer buf;

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                        errno_exit("VIDIOC_QBUF");
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
                errno_exit("VIDIOC_STREAMON");
}


/**
 * @brief   Deallocate resources and uninitialize device buffers.
 *
 * This function deallocates the memory mapped buffers that were previously
 * allocated for capturing video frames. It iterates over each buffer and uses
 * the munmap function to release the memory. The `buffers` array is also freed
 * to release the memory used to store buffer information.
 * If any munmap call returns an error, the errno_exit function is used to
 * handle the error with an appropriate error message.
 *
 * @return  This function does not return a value.
 */
void uninit_device(void)
{
        unsigned int i;
        for (i = 0; i < n_buffers; ++i)
                if (-1 == munmap(buffers[i].start, buffers[i].length))
                        errno_exit("munmap");
        free(buffers);
}


/**
 * @brief   Initialize memory mapping for capturing video frames.
 *
 * This function initializes memory mapping for capturing video frames by performing the following steps:
 * - Requests buffers from the device using VIDIOC_REQBUFS ioctl operation.
 * - Allocates memory for buffer information in the `buffers` array.
 * - Queries each buffer's information using VIDIOC_QUERYBUF ioctl operation.
 * - Maps the buffer memory using the mmap function.
 * If any ioctl call returns an error, the errno_exit function is used to handle
 * the error with an appropriate error message.
 * If the memory allocation fails, the function prints an error message and exits.
 * If the number of requested buffers is insufficient, the function prints an error
 * message and exits.
 *
 * @return  This function does not return a value.
 */
void init_mmap(void)
{
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count = 6;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) 
        {
                if (EINVAL == errno) 
                {
                        fprintf(stderr, "%s does not support "
                                 "memory mapping\n", dev_name);
                        exit(EXIT_FAILURE);
                } else 
                {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        if (req.count < 2) 
        {
                fprintf(stderr, "Insufficient buffer memory on %s\n", dev_name);
                exit(EXIT_FAILURE);
        }

        buffers = calloc(req.count, sizeof(*buffers));

        if (!buffers) 
        {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
                struct v4l2_buffer buf;

                CLEAR(buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = n_buffers;

                if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
                        errno_exit("VIDIOC_QUERYBUF");

                buffers[n_buffers].length = buf.length;
                buffers[n_buffers].start =
                        mmap(NULL /* start anywhere */,
                              buf.length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              fd, buf.m.offset);

                if (MAP_FAILED == buffers[n_buffers].start)
                        errno_exit("mmap");
        }
}

/**
 * @brief   Initialize the video capture device.
 *
 * This function initializes the video capture device by performing the following steps:
 * - Queries and checks device capabilities using VIDIOC_QUERYCAP ioctl operation.
 * - Checks if the device supports video capture and streaming.
 * - Sets the exposure mode to manual and the exposure time to a specific value.
 * - Selects video input, video standard, and cropping capabilities if available.
 * - Sets the desired video format (width, height, pixel format).
 * - Initializes memory mapping for capturing video frames using init_mmap function.
 * If any ioctl call returns an error, the errno_exit function is used to handle
 * the error with an appropriate error message.
 * If any capability or format check fails, or if exposure settings cannot be modified,
 * the function prints an error message and exits the program.
 *
 * @return  This function does not return a value.
 */
void init_device(void)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    unsigned int min;

    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap))
    {
        if (EINVAL == errno) {
            fprintf(stderr, "%s is no V4L2 device\n",
                     dev_name);
            exit(EXIT_FAILURE);
        }
        else
        {
                errno_exit("VIDIOC_QUERYCAP");
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fprintf(stderr, "%s is no video capture device\n",
                 dev_name);
        exit(EXIT_FAILURE);
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        fprintf(stderr, "%s does not support streaming i/o\n",
                    dev_name);
        exit(EXIT_FAILURE);
    }

    /* Video input and tune */

    //Manual exposure setting
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_EXPOSURE_AUTO;
    ctrl.value = V4L2_EXPOSURE_MANUAL;
    if(xioctl(fd, VIDIOC_S_CTRL, &ctrl) != 0)
    {
        syslog(LOG_CRIT, "Exposure mode could not be modified\n");
    }
    else
    {
        syslog(LOG_CRIT, "Exposure set to manual\n");
    }

    //time to set the exposure time
    ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
    ctrl.value = 250;

    if(xioctl(fd, VIDIOC_S_CTRL, &ctrl)!=0)
    {
        syslog(LOG_CRIT,"Exposure time could not be set\n");
    }
    else
    {
        syslog(LOG_CRIT,"Exposure time set to %i",ctrl.value);
    }
    /* Select video input, video standard and tune here. */
    CLEAR(cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap))
    {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop))
        {
            switch (errno)
            {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                        break;
            }

        }
    }
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = HRES;
    fmt.fmt.pix.height      = VRES;

    // Specify the Pixel Coding Formate here

    // This one works for Logitech C200
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
    {
        errno_exit("VIDIOC_S_FMT");
    }
    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
    {
        fmt.fmt.pix.bytesperline = min;
    }
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
    {
        fmt.fmt.pix.sizeimage = min;
    }
    init_mmap();
}

/**
 * @brief   Close the video capture device.
 *
 * This function closes the video capture device associated with the file descriptor `fd`.
 * If the close operation returns an error, the errno_exit function is used to handle
 * the error with an appropriate error message. After closing the device, the `fd` is set to -1.
 *
 * @return  This function does not return a value.
 */
void close_device(void)
{
        if (-1 == close(fd))
                errno_exit("close");

        fd = -1;
}

/**
 * @brief   Open the video capture device.
 *
 * This function opens the video capture device associated with the device name `dev_name`.
 * It performs the following steps:
 * - Uses the `stat` system call to check if the specified path is a character device file.
 * - Opens the device using the `open` system call with required flags.
 * If any system call returns an error, the function prints an appropriate error message and exits.
 *
 * @return  This function does not return a value.
 */
void open_device(void)
{
        struct stat st;

        if (-1 == stat(dev_name, &st)) {
                fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit(EXIT_FAILURE);
        }

        if (!S_ISCHR(st.st_mode)) {
                fprintf(stderr, "%s is no device\n", dev_name);
                exit(EXIT_FAILURE);
        }

        fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

        if (-1 == fd) {
                fprintf(stderr, "Cannot open '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit(EXIT_FAILURE);
        }
}

static int frames_reading(void)
{
    struct v4l2_buffer buf_service;
    unsigned int i;

    CLEAR(buf_service);

    buf_service.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf_service.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf_service))
    {
        switch (errno)
        {
        case EAGAIN:
            return 0;

        case EIO:
            /* Could ignore EIO, but drivers should only set for serious errors, although some set for
               non-fatal errors too.
             */
            return 0;

        default:
            printf("mmap failure\n");
            errno_exit("VIDIOC_DQBUF");
        }
    }

    assert(buf_service.index < n_buffers);
    continuous_transformation(buffers[buf_service.index].start, buf_service.bytesused);

    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf_service))
        errno_exit("VIDIOC_QBUF");

    return 1;
}

void capture_pic(void)
{

    for (;;)
    {
        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(fd + 1, &fds, NULL, NULL, &tv);

        if (-1 == r)
        {
            if (EINTR == errno)
                continue;
            errno_exit("select");
        }

        if (0 == r)
        {
            fprintf(stderr, "select timeout\n");
            exit(EXIT_FAILURE);
        }

        if (frames_reading())
        {
            break;
        }
    }
}

unsigned char *return_pic_buffer()
{
    capture_pic();
    return bigbuffer;
}