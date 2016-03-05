#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
  
  
#include <assert.h>  
#include <getopt.h>             
#include <fcntl.h>              
#include <unistd.h>  
#include <errno.h>  
#include <malloc.h>  
#include <sys/stat.h>  
#include <sys/types.h>  
#include <sys/time.h>  
#include <sys/mman.h>  
#include <sys/ioctl.h>  
  
  
  
  
#include <asm/types.h>          
#include <linux/videodev2.h>  
  
  
#define CAMERA_DEVICE "/dev/video0"  
  
  
#define CAPTURE_FILE "frame_yuyv_new.jpg"  
#define CAPTURE_RGB_FILE "frame_rgb_new.bmp"  
#define CAPTURE_show_FILE "a.bmp"  
  
  
#define VIDEO_WIDTH 640  
#define VIDEO_HEIGHT 480  
#define VIDEO_FORMAT V4L2_PIX_FMT_YUYV  
#define BUFFER_COUNT 4  
  
  
typedef struct VideoBuffer {  
    void   *start; //��Ƶ����������ʼ��ַ  
    size_t  length;//�������ĳ���  
} VideoBuffer;  
  
  
/*  
void *calloc(unsigned n,unsigned size) �� ��: ���ڴ�Ķ�̬�洢���з���n������Ϊsize�������ռ䣬��������һ��ָ�������ʼ��ַ��ָ�룻������䲻�ɹ�������NULL����malloc������calloc�ڶ�̬�������ڴ���Զ���ʼ�����ڴ�ռ�Ϊ�㣬��malloc����ʼ��������������������������  
*/  
//λͼ�ļ�ͷ���ݽṹ����λͼ�ļ������ͣ���С�ʹ�ӡ��ʽ����Ϣ  
//���������ֽڵĶ���  
#pragma pack(1)  
typedef struct BITMAPFILEHEADER  
{  
  unsigned short bfType;//λͼ�ļ�������,  
  unsigned long bfSize;//λͼ�ļ��Ĵ�С�����ֽ�Ϊ��λ  
  unsigned short bfReserved1;//λͼ�ļ������֣�����Ϊ0  
  unsigned short bfReserved2;//ͬ��  
  unsigned long bfOffBits;//λͼ���е���ʼλ�ã��������λͼ�ļ�   ����˵��ͷ��ƫ������ʾ�����ֽ�Ϊ��λ  
} BITMAPFILEHEADER;  
#pragma pack()  
  
  
typedef struct BITMAPINFOHEADER//λͼ��Ϣͷ���͵����ݽṹ������˵��λͼ�ĳߴ�  
{  
  unsigned long biSize;//λͼ��Ϣͷ�ĳ��ȣ����ֽ�Ϊ��λ  
  unsigned long biWidth;//λͼ�Ŀ�ȣ�������Ϊ��λ  
  unsigned long biHeight;//λͼ�ĸ߶ȣ�������Ϊ��λ  
  unsigned short biPlanes;//Ŀ���豸�ļ���,����Ϊ1  
  unsigned short biBitCount;//ÿ�����������λ����������1(��ɫ),4(16ɫ),8(256ɫ)��24(2^24ɫ)֮һ  
  unsigned long biCompression;//λͼ��ѹ�����ͣ�������0-��ѹ����1-BI_RLE8ѹ�����ͻ�2-BI_RLE4ѹ������֮һ  
  unsigned long biSizeImage;//λͼ��С�����ֽ�Ϊ��λ  
  unsigned long biXPelsPerMeter;//λͼĿ���豸ˮƽ�ֱ��ʣ���ÿ��������Ϊ��λ  
  unsigned long biYPelsPerMeter;//λͼĿ���豸��ֱ�ֱ��ʣ���ÿ��������Ϊ��λ  
  unsigned long biClrUsed;//λͼʵ��ʹ�õ���ɫ���е���ɫ��ַ��  
  unsigned long biClrImportant;//λͼ��ʾ�����б���Ϊ��Ҫ��ɫ�ı�ַ��  
} BITMAPINFOHEADER;  
  
  
  
  
VideoBuffer framebuf[BUFFER_COUNT];   //�޸��˴���2012-5.21  
int fd;  
struct v4l2_capability cap;  
struct v4l2_fmtdesc fmtdesc;  
struct v4l2_format fmt;  
struct v4l2_requestbuffers reqbuf;  
struct v4l2_buffer buf;  
unsigned char *starter;  
unsigned char *newBuf;  
struct BITMAPFILEHEADER bfh;  
struct BITMAPINFOHEADER bih;  
  
  
void create_bmp_header()  
{  
  bfh.bfType = (unsigned short)0x4D42;  
  bfh.bfSize = (unsigned long)(14 + 40 + VIDEO_WIDTH * VIDEO_HEIGHT*3);  
  bfh.bfReserved1 = 0;  
  bfh.bfReserved2 = 0;  
  bfh.bfOffBits = (unsigned long)(14 + 40);  
  
  
  bih.biBitCount = 24;  
  bih.biWidth = VIDEO_WIDTH;  
  bih.biHeight = VIDEO_HEIGHT;  
  bih.biSizeImage = VIDEO_WIDTH * VIDEO_HEIGHT * 3;  
  bih.biClrImportant = 0;  
  bih.biClrUsed = 0;  
  bih.biCompression = 0;  
  bih.biPlanes = 1;  
  bih.biSize = 40;//sizeof(bih);  
  bih.biXPelsPerMeter = 0x00000ec4;  
  bih.biYPelsPerMeter = 0x00000ec4;  
}  
  
  
int open_device()  
{  
/*  
��linux���豸�������ļ�����ʽ���й����  
ioctl���豸���������ж��豸��I/Oͨ�����й���ĺ���int ioctl(int fd,int cmd,...)?  
�ɹ�����0��������-1  
����fd--�����û�������豸ʹ��open�������ص��ļ���ʶ��  
    cmd--�����û�������豸�Ŀ���������ں��涼ʡ�Ժţ��л�û�к�cmd���������  
*/  
    int fd;  
    fd = open(CAMERA_DEVICE, O_RDWR, 0);//  
    if (fd < 0) {  
        printf("Open %s failed\n", CAMERA_DEVICE);  
        return -1;  
    }  
    return fd;  
}  
  
  
void get_capability()  
{// ��ȡ������Ϣ  
/*  
��������VIDIOC_QUERYCAP  
���ܣ���ѯ�豸�����Ĺ���;  
����˵������������ΪV4L2��������������struct v4l2_capability;  
struct v4l2_capability {  
        __u8    driver[16];     //i.e. "bttv"            //��������,  
        __u8    card[32];       // i.e. "Hauppauge WinTV"         //  
        __u8    bus_info[32];   // "PCI:" + pci_name(pci_dev)     //PCI������Ϣ  
        __u32   version;        // should use KERNEL_VERSION()   
        __u32   capabilities;   // Device capabilities         //�豸����  
        __u32   reserved[4];  
};  
����ֵ˵���� ִ�гɹ�ʱ����������ֵΪ 0;  
����ִ�гɹ���struct v4l2_capability �ṹ������еķ��ص�ǰ��Ƶ�豸��֧�ֵĹ���  
����֧����Ƶ������V4L2_CAP_VIDEO_CAPTURE��V4L2_CAP_STREAMING  
*/  
    int ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);  
    if (ret < 0) {  
        printf("VIDIOC_QUERYCAP failed (%d)\n", ret);  
        return;  
    }  
    // Print capability infomations  
    printf("------------VIDIOC_QUERYCAP-----------\n");  
    printf("Capability Informations:\n");  
    printf(" driver: %s\n", cap.driver);  
    printf(" card: %s\n", cap.card);  
    printf(" bus_info: %s\n", cap.bus_info);  
    printf(" version: %08X\n", cap.version);  
    printf(" capabilities: %08X\n\n", cap.capabilities);  
    return;  
}  
  
  
void get_format()  
{  
/*��ȡ��ǰ��Ƶ�豸֧�ֵ���Ƶ��ʽ  
�������� VIDIOC_ENUM_FMT  
���ܣ� ��ȡ��ǰ��Ƶ�豸֧�ֵ���Ƶ��ʽ ��  
����˵������������ΪV4L2����Ƶ��ʽ���������� struct v4l2_fmtdesc  
struct v4l2_fmtdesc {  
        __u32               index;             // Format number        
        enum v4l2_buf_type  type;              // buffer type          
        __u32               flags;  
        __u8                description[32];   // Description string   
        __u32               pixelformat;       // Format fourcc        
        __u32               reserved[4];  
};  
����ֵ˵���� ִ�гɹ�ʱ����������ֵΪ 0;  
*/  
	printf("--------GET FORMAT--------\n");  
    int ret;  
    fmtdesc.index=0;  
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    ret=ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);  
    while (ret != 0)  
    {  
        fmtdesc.index++;  
        ret=ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);  
    }  
    printf("--------VIDIOC_ENUM_FMT---------\n");  
    printf("get the format what the device support\n{ pixelformat = ''%c%c%c%c'', description = ''%s'' }\n",fmtdesc.pixelformat & 0xFF, (fmtdesc.pixelformat >> 8) & 0xFF, (fmtdesc.pixelformat >> 16) & 0xFF,(fmtdesc.pixelformat >> 24) & 0xFF, fmtdesc.description);  
      
    return;  
}  
  
  
int set_format()  
{  
/*  
��������VIDIOC_S_FMT  
���ܣ�������Ƶ�豸����Ƶ���ݸ�ʽ������������Ƶͼ�����ݵĳ�����ͼ���ʽJPEG��YUYV��ʽ);  
����˵������������ΪV4L2����Ƶ���ݸ�ʽ����struct v4l2_format;  
struct v4l2_format {  
        enum v4l2_buf_type type;    //���������ͣ�������Զ��V4L2_BUF_TYPE_VIDEO_CAPTURE  
        union {  
                struct v4l2_pix_format          pix;     // V4L2_BUF_TYPE_VIDEO_CAPTURE   
                struct v4l2_window              win;     // V4L2_BUF_TYPE_VIDEO_OVERLAY   
                struct v4l2_vbi_format          vbi;     // V4L2_BUF_TYPE_VBI_CAPTURE   
                struct v4l2_sliced_vbi_format   sliced;  // V4L2_BUF_TYPE_SLICED_VBI_CAPTURE   
                __u8    raw_data[200];                   // user-defined   
        } fmt;  
};  
struct v4l2_pix_format {  
        __u32                   width;         // ��������16�ı���  
        __u32                   height;        // �ߣ�������16�ı���  
        __u32                   pixelformat;   // ��Ƶ���ݴ洢���ͣ�������YUV4:2:2����RGB  
        enum v4l2_field       field;  
        __u32                   bytesperline;  
        __u32                   sizeimage;  
        enum v4l2_colorspace colorspace;  
        __u32                   priv;  
};  
����ֵ˵���� ִ�гɹ�ʱ����������ֵΪ 0;  
  
  
  
  
ע�⣺�������Ƶ�豸������֧�������趨��ͼ���ʽ����Ƶ�����������޸�struct v4l2_format�ṹ�������ֵΪ����Ƶ�豸��֧�ֵ�ͼ���ʽ�������ڳ�������У��趨�����е���Ƶ��ʽ��Ҫ��ȡʵ�ʵ���Ƶ��ʽ��Ҫ���¶�ȡ struct v4l2_format�ṹ�������  
ʹ��VIDIOC_G_FMT������Ƶ�豸����Ƶ���ݸ�ʽ��VIDIOC_TRY_FMT��֤��Ƶ�豸����Ƶ���ݸ�ʽ  
*/  
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    fmt.fmt.pix.width       = VIDEO_WIDTH;  
    fmt.fmt.pix.height      = VIDEO_HEIGHT;  
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;//V4L2_PIX_FMT_YUYV;  
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;  
    int ret = ioctl(fd, VIDIOC_S_FMT, &fmt);  
    if (ret < 0) {  
        printf("VIDIOC_S_FMT failed (%d)\n", ret);  
        return -1;  
    }  
  
  
  /*  // ������Ƶ��ʽVIDIOC_G_FMT��VIDIOC_S_FMT��ͬ  
    ret = ioctl(fd, VIDIOC_G_FMT, &fmt);  
    if (ret < 0) {  
        printf("VIDIOC_G_FMT failed (%d)\n", ret);  
        return ret;  
    }*/  
    // Print Stream Format  
    printf("------------VIDIOC_S_FMT---------------\n");  
    printf("Stream Format Informations:\n");  
    printf(" type: %d\n", fmt.type);  
    printf(" width: %d\n", fmt.fmt.pix.width);  
    printf(" height: %d\n", fmt.fmt.pix.height);  
  
  
    char fmtstr[8];  
    memset(fmtstr, 0, 8);  
/*  
void *memcpy(void *dest, const void *src, size_t n);  
��Դsrc��ָ���ڴ��ַ����ʼλ�ÿ�ʼ����n���ֽڵ�Ŀ��dest��ָ���ڴ��ַ����ʼλ����  
����ͷ�ļ�include <string.h>  
*/  
    memcpy(fmtstr, &fmt.fmt.pix.pixelformat, 4);  
    printf(" pixelformat: %s\n", fmtstr);  
    printf(" field: %d\n", fmt.fmt.pix.field);  
    printf(" bytesperline: %d\n", fmt.fmt.pix.bytesperline);  
    printf(" sizeimage: %d\n", fmt.fmt.pix.sizeimage);  
    printf(" colorspace: %d\n", fmt.fmt.pix.colorspace);  
    printf(" priv: %d\n", fmt.fmt.pix.priv);  
    printf(" raw_date: %s\n", fmt.fmt.raw_data);  
    return 0;  
}  
  
  
void request_buf()  
{  
/*  
��������VIDIOC_REQBUFS  
���ܣ� ����V4L2����������Ƶ������(����V4L2��Ƶ���������ڴ�)��V4L2����Ƶ�豸�������㣬λ���ں˿ռ䣬����ͨ��VIDIOC_REQBUFS����������������ڴ�λ���ں˿ռ䣬Ӧ�ó�����ֱ�ӷ��ʣ���Ҫͨ������mmap�ڴ�ӳ�亯�����ں˿ռ��ڴ�ӳ�䵽�û��ռ��Ӧ�ó���ͨ�������û��ռ��ַ�������ں˿ռ䡣  
����˵������������ΪV4L2�����뻺�������ݽṹ������struct v4l2_requestbuffers;  
struct v4l2_requestbuffers {  
        u32                   count;        //��������,Ҳ����˵�ڻ�������ﱣ�ֶ�������Ƭ  
        enum v4l2_buf_type    type;         //����������,������Զ��V4L2_BUF_TYPE_VIDEO_CAPTURE  
        enum v4l2_memory      memory;       //V4L2_MEMORY_MMAP��V4L2_MEMORY_USERPTR  
        u32                   reserved[2];  
};  
����ֵ˵���� ִ�гɹ�ʱ����������ֵΪ 0��V4L2��������������Ƶ������;  
  
  
ע�⣺VIDIOC_REQBUFS���޸�tV4L2_reqbuf��countֵ��tV4L2_reqbuf��countֵ����ʵ������ɹ�����Ƶ��������Ŀ;  
*/  
    reqbuf.count = BUFFER_COUNT;  
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    reqbuf.memory = V4L2_MEMORY_MMAP;  
      
    int ret = ioctl(fd , VIDIOC_REQBUFS, &reqbuf);  
    if(ret < 0) {  
        printf("VIDIOC_REQBUFS failed (%d)\n", ret);  
        return;  
    }  
    printf("the buffer has been assigned successfully!\n");  
    return;  
}  
  
  
void query_map_qbuf()  
{  
/*  
��������VIDIOC_QUERYBUF  
���ܣ���ѯ�Ѿ������V4L2����Ƶ�������������Ϣ��������Ƶ��������ʹ��״̬�����ں˿ռ��ƫ�Ƶ�ַ�����������ȵȡ���Ӧ�ó��������ͨ����VIDIOC_QUERYBUF����ȡ�ں˿ռ����Ƶ��������Ϣ��Ȼ����ú���mmap���ں˿ռ��ַӳ�䵽�û��ռ䣬����Ӧ�ó�����ܹ�����λ���ں˿ռ����Ƶ������  
����˵������������ΪV4L2���������ݽṹ����struct v4l2_buffer;  
struct v4l2_buffer {  
        __u32                   index;  
        enum v4l2_buf_type      type;  
        __u32                   bytesused;  
        __u32                   flags;  
        enum v4l2_field         field;  
        struct timeval          timestamp;  
        struct v4l2_timecode    timecode;  
        __u32                   sequence;  
        ////////// memory location ////////  
        enum v4l2_memory        memory;  
        union {  
                __u32           offset;  
                unsigned long   userptr;  
        } m;  
        __u32                   length;  
        __u32                   input;  
        __u32                   reserved;  
};  
����ֵ˵���� ִ�гɹ�ʱ����������ֵΪ 0;  
struct v4l2_buffer�ṹ������б�����ָ��Ļ������������Ϣ;һ������£�Ӧ�ó����е���VIDIOC_QUERYBUFȡ�����ں˻�������Ϣ�󣬽����ŵ���mmap�������ں˿ռ��ַӳ�䵽�û��ռ䷽���û��ռ�Ӧ�ó���ķ���  
*/  
    int i,ret;  
    for (i = 0; i < reqbuf.count; i++)  
    {  
        buf.index = i;  
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
        buf.memory = V4L2_MEMORY_MMAP;  
        ret = ioctl(fd , VIDIOC_QUERYBUF, &buf);//bufȡ���ڴ滺��������Ϣ  
        if(ret < 0) {  
            printf("VIDIOC_QUERYBUF (%d) failed (%d)\n", i, ret);  
            return;  
        }  
  
  
        // mmap buffer  
        framebuf[i].length = buf.length;//framebuf�ǳ�����ǰ�涨���һ���ṹ�����͵�����  
/*  
#include <sys/mman.h>  
void *mmap(void *start, size_t length, int prot, int flags,int fd, off_t offset);  
int munmap(void *start, size_t length);  
mmap��һ���ļ�������������ӳ����ڴ档�ļ���ӳ�䵽���ҳ�ϣ�����ļ��Ĵ�С��������ҳ�Ĵ�С֮�ͣ����һ��ҳ����ʹ�õĿռ佫������  
start��ӳ�����Ŀ�ʼ��ַ������Ϊ0ʱ��ʾ��ϵͳ����ӳ��������ʼ��ַ  
length��ӳ�����ĳ���  
prot���������ڴ汣����־���������ļ��Ĵ�ģʽ��ͻ�������µ�ĳ��ֵ������ͨ��or�������������һ��    PROT_EXEC //ҳ���ݿ��Ա�ִ��  
    PROT_READ //ҳ���ݿ��Ա���ȡ  
    PROT_WRITE //ҳ���Ա�д��  
    PROT_NONE //ҳ���ɷ���  
flags��ָ��ӳ���������ͣ�ӳ��ѡ���ӳ��ҳ�Ƿ���Թ���  
    MAP_SHARED //����������ӳ���������Ľ��̹���ӳ��ռ䡣�Թ�������д�룬�൱��������ļ���ֱ��msync()����munmap()�����ã��ļ�ʵ���ϲ��ᱻ����  
fd����Ч���ļ������ʡ�һ������open()�������أ���ֵҲ��������Ϊ-1����ʱ��Ҫָ��flags�����е�MAP_ANON,�������е�������ӳ��  
offset����ӳ��������ݵ����  
  
  
�ɹ�ִ��ʱ��mmap()���ر�ӳ������ָ�룬munmap()����0��ʧ��ʱ��mmap()����MAP_FAILED[��ֵΪ(void *)-1]��munmap����-1  
*/  
        framebuf[i].start = (char *) mmap(0, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, buf.m.offset);  
        if (framebuf[i].start == MAP_FAILED) {  
            printf("mmap (%d) failed: %s\n", i, strerror(errno));  
            return;  
        }  
      
        // Queen buffer  
/*  
��������VIDIOC_QBUF  
���ܣ�Ͷ��һ���յ���Ƶ����������Ƶ���������������  
����˵������������ΪV4L2���������ݽṹ����struct v4l2_buffer;  
����ֵ˵���� ִ�гɹ�ʱ����������ֵΪ 0;  
����ִ�гɹ���ָ��(ָ��)����Ƶ������������Ƶ������У���������Ƶ�豸����ͼ��ʱ����Ӧ����Ƶ���ݱ����浽��Ƶ���������Ӧ����Ƶ��������  
*/  
        ret = ioctl(fd , VIDIOC_QBUF, &buf);  
        if (ret < 0) {  
            printf("VIDIOC_QBUF (%d) failed (%d)\n", i, ret);  
            return;  
        }  
  
  
        printf("Frame buffer %d: address=0x%x, length=%d\n", i, (unsigned int)framebuf[i].start, framebuf[i].length);  
    }//�յ���Ƶ���������Ѿ�����Ƶ����������������  
    return;  
}  
  
  
  
  
void yuyv2rgb()  
{  
    unsigned char YUYV[4],RGB[6];  
    int j,k,i;     
    unsigned int location=0;  
    j=0;  
    for(i=0;i < framebuf[buf.index].length;i+=4)  
    {  
        YUYV[0]=starter[i];//Y0  
        YUYV[1]=starter[i+1];//U  
        YUYV[2]=starter[i+2];//Y1  
        YUYV[3]=starter[i+3];//V  
        if(YUYV[0]<1)  
        {  
            RGB[0]=0;  
            RGB[1]=0;  
            RGB[2]=0;  
        }  
        else  
        {  
            RGB[0]=YUYV[0]+1.772*(YUYV[1]-128);//b  
            RGB[1]=YUYV[0]-0.34413*(YUYV[1]-128)-0.71414*(YUYV[3]-128);//g  
            RGB[2]=YUYV[0]+1.402*(YUYV[3]-128);//r  
        }  
        if(YUYV[2]<0)  
        {  
            RGB[3]=0;  
            RGB[4]=0;  
            RGB[5]=0;  
        }  
        else  
        {  
            RGB[3]=YUYV[2]+1.772*(YUYV[1]-128);//b  
            RGB[4]=YUYV[2]-0.34413*(YUYV[1]-128)-0.71414*(YUYV[3]-128);//g  
            RGB[5]=YUYV[2]+1.402*(YUYV[3]-128);//r  
  
  
        }  
  
  
        for(k=0;k<6;k++)  
        {  
            if(RGB[k]<0)  
                RGB[k]=0;  
            if(RGB[k]>255)  
                RGB[k]=255;  
        }  
  
  
        //���ס��ɨ������λͼ�ļ����Ƿ���洢�ģ�  
        if(j%(VIDEO_WIDTH*3)==0)//��λ�洢λ��  
        {  
            location=(VIDEO_HEIGHT-j/(VIDEO_WIDTH*3))*(VIDEO_WIDTH*3);  
        }  
        bcopy(RGB,newBuf+location+(j%(VIDEO_WIDTH*3)),sizeof(RGB));  
  
  
        j+=6;         
    }  
    return;  
}  
  
  
void move_noise()  
{//˫�˲���  
    int i,j,k,temp[3],temp1[3];  
    unsigned char BGR[13*3];  
    unsigned int sq,sq1,loc,loc1;  
    int h=VIDEO_HEIGHT,w=VIDEO_WIDTH;  
    for(i=2;i<h-2;i++)  
    {  
        for(j=2;j<w-2;j++)  
        {  
            memcpy(BGR,newBuf+(i-1)*w*3+3*(j-1),9);  
            memcpy(BGR+9,newBuf+i*w*3+3*(j-1),9);  
            memcpy(BGR+18,newBuf+(i+1)*w*3+3*(j-1),9);  
            memcpy(BGR+27,newBuf+(i-2)*w*3+3*j,3);  
            memcpy(BGR+30,newBuf+(i+2)*w*3+3*j,3);  
            memcpy(BGR+33,newBuf+i*w*3+3*(j-2),3);  
            memcpy(BGR+36,newBuf+i*w*3+3*(j+2),3);  
  
  
            memset(temp,0,4*3);  
              
            for(k=0;k<9;k++)  
            {  
                temp[0]+=BGR[k*3];  
                temp[1]+=BGR[k*3+1];  
                temp[2]+=BGR[k*3+2];  
            }  
            temp1[0]=temp[0];  
            temp1[1]=temp[1];  
            temp1[2]=temp[2];  
            for(k=9;k<13;k++)  
            {  
                temp1[0]+=BGR[k*3];  
                temp1[1]+=BGR[k*3+1];  
                temp1[2]+=BGR[k*3+2];  
            }  
            for(k=0;k<3;k++)  
            {  
                temp[k]/=9;  
                temp1[k]/=13;  
            }  
            sq=0xffffffff;loc=0;  
            sq1=0xffffffff;loc1=0;  
            unsigned int a;           
            for(k=0;k<9;k++)  
            {  
                a=abs(temp[0]-BGR[k*3])+abs(temp[1]-BGR[k*3+1])+abs(temp[2]-BGR[k*3+2]);  
                if(a<sq)  
                {  
                    sq=a;  
                    loc=k;  
                }  
            }  
            for(k=0;k<13;k++)  
            {  
                a=abs(temp1[0]-BGR[k*3])+abs(temp1[1]-BGR[k*3+1])+abs(temp1[2]-BGR[k*3+2]);  
                if(a<sq1)  
                {  
                    sq1=a;  
                    loc1=k;  
                }  
            }  
              
            newBuf[i*w*3+3*j]=(unsigned char)((BGR[3*loc]+BGR[3*loc1])/2);  
            newBuf[i*w*3+3*j+1]=(unsigned char)((BGR[3*loc+1]+BGR[3*loc1+1])/2);  
            newBuf[i*w*3+3*j+2]=(unsigned char)((BGR[3*loc+2]+BGR[3*loc1+2])/2);  
            /*������Щ������  
            temp[0]=(BGR[3*loc]+BGR[3*loc1])/2;  
            temp[1]=(BGR[3*loc+1]+BGR[3*loc1+1])/2;  
            temp[2]=(BGR[3*loc+2]+BGR[3*loc1+2])/2;  
            sq=abs(temp[0]-BGR[loc*3])+abs(temp[1]-BGR[loc*3+1])+abs(temp[2]-BGR[loc*3+2]);  
            sq1=abs(temp[0]-BGR[loc1*3])+abs(temp[1]-BGR[loc1*3+1])+abs(temp[2]-BGR[loc1*3+2]);  
            if(sq1<sq) loc=loc1;  
            newBuf[i*w*3+3*j]=BGR[3*loc];  
            newBuf[i*w*3+3*j+1]=BGR[3*loc+1];  
            newBuf[i*w*3+3*j+2]=BGR[3*loc+2];*/  
        }  
    }  
    return;  
}  
  
  
void yuyv2rgb1()  
{  
    unsigned char YUYV[3],RGB[3];  
    memset(YUYV,0,3);  
    int j,k,i;     
    unsigned int location=0;  
    j=0;  
    for(i=0;i < framebuf[buf.index].length;i+=2)  
    {  
        YUYV[0]=starter[i];//Y0  
        if(i%4==0)  
            YUYV[1]=starter[i+1];//U  
        //YUYV[2]=starter[i+2];//Y1  
        if(i%4==2)  
            YUYV[2]=starter[i+1];//V  
        if(YUYV[0]<1)  
        {  
            RGB[0]=0;  
            RGB[1]=0;  
            RGB[2]=0;  
        }  
        else  
        {  
            RGB[0]=YUYV[0]+1.772*(YUYV[1]-128);//b  
            RGB[1]=YUYV[0]-0.34413*(YUYV[1]-128)-0.71414*(YUYV[2]-128);//g  
            RGB[2]=YUYV[0]+1.402*(YUYV[2]-128);//r  
        }  
  
  
        for(k=0;k<3;k++)  
        {  
            if(RGB[k]<0)  
                RGB[k]=0;  
            if(RGB[k]>255)  
                RGB[k]=255;  
        }  
  
  
        //���ס��ɨ������λͼ�ļ����Ƿ���洢�ģ�  
        if(j%(VIDEO_WIDTH*3)==0)//��λ�洢λ��  
        {  
            location=(VIDEO_HEIGHT-j/(VIDEO_WIDTH*3))*(VIDEO_WIDTH*3);  
        }  
        bcopy(RGB,newBuf+location+(j%(VIDEO_WIDTH*3)),sizeof(RGB));  
  
  
        j+=3;         
    }  
    return;  
}  
  
  
void store_yuyv()  
{  
    FILE *fp = fopen(CAPTURE_FILE, "wb");  
    if (fp < 0) {  
        printf("open frame data file failed\n");  
        return;  
    }  
    fwrite(framebuf[buf.index].start, 1, buf.length, fp);  
    fclose(fp);  
    printf("Capture one frame saved in %s\n", CAPTURE_FILE);  
    return;  
}  
  
  
  
  
void store_bmp(int n_len)  
{  
    FILE *fp1 = fopen(CAPTURE_RGB_FILE, "wb");  
    if (fp1 < 0) {  
        printf("open frame data file failed\n");  
        return;  
    }  
    fwrite(&bfh,sizeof(bfh),1,fp1);  
    fwrite(&bih,sizeof(bih),1,fp1);  
    fwrite(newBuf, 1, n_len, fp1);  
    fclose(fp1);  
    printf("Change one frame saved in %s\n", CAPTURE_RGB_FILE);  
    return;  
}  
  
  
int main()  
{  
    int i, ret;  
  
  
    // ���豸  
    fd=open_device();  
      
    // ��ȡ������Ϣ  
    //struct v4l2_capability cap;  
    get_capability();  
     
    //��ȡ��ǰ��Ƶ�豸֧�ֵ���Ƶ��ʽ  
    //struct v4l2_fmtdesc fmtdesc;  
    memset(&fmtdesc,0,sizeof(fmtdesc));  
    get_format();  
      
    // ������Ƶ��ʽ  
    //struct v4l2_format fmt;  
    //memset��һ���ڴ�������ĳ��������ֵ�����ǶԽϴ�Ľṹ�������������������һ�����ķ���  
    memset(&fmt, 0, sizeof(fmt));//��fmt�е�ǰsizeof(fmt)�ֽ���0�滻������fmt  
    set_format();  
      
    // ��������ڴ�  
    //struct v4l2_requestbuffers reqbuf;  
    request_buf();  
  
  
    // ��ȡ�ռ䣬������ӳ�䵽�û��ռ䣬Ȼ��Ͷ�ŵ���Ƶ�������  
    //struct v4l2_buffer buf;  
    query_map_qbuf();  
  
  
  
  
    // ��ʼ¼��  
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
/*  
��������VIDIOC_STREAMON  
���ܣ�������Ƶ�ɼ����Ӧ�ó������VIDIOC_STREAMON������Ƶ�ɼ��������Ƶ�豸��������ʼ�ɼ���Ƶ���ݣ����Ѳɼ�������Ƶ���ݱ��浽��Ƶ��������Ƶ��������  
����˵������������ΪV4L2����Ƶ���������� enum v4l2_buf_type ;  
enum v4l2_buf_type {  
        V4L2_BUF_TYPE_VIDEO_CAPTURE        = 1,  
        V4L2_BUF_TYPE_VIDEO_OUTPUT         = 2,  
        V4L2_BUF_TYPE_VIDEO_OVERLAY        = 3,  
        V4L2_BUF_TYPE_VBI_CAPTURE          = 4,  
        V4L2_BUF_TYPE_VBI_OUTPUT           = 5,  
        V4L2_BUF_TYPE_SLICED_VBI_CAPTURE   = 6,  
        V4L2_BUF_TYPE_SLICED_VBI_OUTPUT    = 7,  
#if 1  
        //// Experimental ////  
        V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY = 8,  
#endif  
        V4L2_BUF_TYPE_PRIVATE              = 0x80,  
};  
����ֵ˵���� ִ�гɹ�ʱ����������ֵΪ 0;����ִ�гɹ�����Ƶ�豸��������ʼ�ɼ���Ƶ���ݣ���ʱӦ�ó���һ��ͨ������select�������ж�һ֡��Ƶ�����Ƿ�ɼ���ɣ�����Ƶ�豸�������һ֡��Ƶ���ݲɼ������浽��Ƶ��������ʱ��select�������أ�Ӧ�ó�����ſ��Զ�ȡ��Ƶ����;����select��������ֱ����Ƶ���ݲɼ����  
*/  
    ret = ioctl(fd, VIDIOC_STREAMON, &type);  
    if (ret < 0) {  
        printf("VIDIOC_STREAMON failed (%d)\n", ret);  
        return ret;  
    }  
  
  
    // Get frame  
/*  
��������VIDIOC_DQBUF  
���ܣ�����Ƶ�����������������ȡ��һ���Ѿ�������һ֡��Ƶ���ݵ���Ƶ������  
����˵������������ΪV4L2���������ݽṹ����struct v4l2_buffer;  
����ֵ˵���� ִ�гɹ�ʱ����������ֵΪ 0;����ִ�гɹ�����Ӧ���ں���Ƶ�������б����е�ǰ���㵽����Ƶ���ݣ�Ӧ�ó������ͨ�������û��ռ�����ȡ����Ƶ���ݣ�ǰ���Ѿ�ͨ�����ú��� mmap�����û��ռ���ں˿ռ���ڴ�ӳ��).  
  
  
˵��: VIDIOC_DQBUF������, ʹ�Ӷ���ɾ���Ļ���֡��Ϣ�����˴�buf  
V4L2_buffer�ṹ������þ��൱������Ļ���֡�Ĵ����һ���֡�Ķ�Ҫ����������ͨ��������ϵ����֡�������м�����������  
*/  
    ret = ioctl(fd, VIDIOC_DQBUF, &buf);//VIDIOC_DQBUF������, ʹ�Ӷ���ɾ���Ļ���֡��Ϣ�����˴�buf  
    if (ret < 0) {  
        printf("VIDIOC_DQBUF failed (%d)\n", ret);  
        return ret;  
    }  
  
  
    // Process the frame ��ʱ������Ҫ�������ݸ�ʽ�ĸı�  
    store_yuyv();  
      
      
    //�Բɼ������ݽ���ת�䣬�任��RGB24ģʽ��Ȼ����д洢  
/*  
��1�����ٳ���һ���ڴ����������ת���������  
��2��ѭ����ȡbuf�ڴ�ε����ݣ�����ת����ת������뵽�¿��ٵ��ڴ�������  
��3�����¿��ٳ������ڴ��������ݶ����ļ���  
*/  
    printf("********************************************\n");  
    int n_len;  
    n_len=framebuf[buf.index].length*3/2;  
    newBuf=calloc((unsigned int)n_len,sizeof(unsigned char));  
    
    if(!newBuf)  
    {  
        printf("cannot assign the memory !\n");  
        exit(0);  
    }  
  
  
    printf("the information about the new buffer:\n start Address:0x%x,length=%d\n\n",(unsigned int)newBuf,n_len);  
  
  
    printf("----------------------------------\n");  
      
    //YUYV to RGB  
    starter=(unsigned char *)framebuf[buf.index].start;  
    yuyv2rgb();//��������ɼ���ͼƬ��Ч���ȽϺ�  
    move_noise();  
    //yuyv2rgb1();  
    //����bmp�ļ���ͷ��bmp�ļ���һЩ��Ϣ  
    create_bmp_header();  
      
    store_bmp(n_len);  
      
      
    // Re-queen buffer  
    ret = ioctl(fd, VIDIOC_QBUF, &buf);  
    if (ret < 0) {  
        printf("VIDIOC_QBUF failed (%d)\n", ret);  
        return ret;  
    }  
    printf("re-queen buffer end\n");  
    // Release the resource  
/*  
��ͷ�ļ� #include<unistd.h>  
        #include<sys/mman.h>  
        ���庯�� int munmap(void *start,size_t length);  
        ����˵�� munmap()����ȡ������start��ָ��ӳ���ڴ���ʼ��ַ������length������ȡ�����ڴ��С�������̽���������exec��غ�����ִ����������ʱ��ӳ���ڴ���Զ���������رն�Ӧ���ļ�������ʱ������ӳ��  
        ����ֵ ������ӳ��ɹ��򷵻�0�����򷵻أ�1  
*/  
    for (i=0; i< 4; i++)  
    {  
      
        munmap(framebuf[i].start, framebuf[i].length);  
    }  
    //free(starter);  
printf("free starter end\n");  
    //free(newBuf);  
printf("free newBuf end\n");  
    close(fd);  
  
  
      
    printf("Camera test Done.\n");  
    return 0;  
}  