#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/videodev2.h>
#include <sys/poll.h>
#include <errno.h>

#include "RTSPStream.h" 
#include "mfc_interface.h"
#include "SsbSipMfcApi.h"
#define TEST_H264
//#define TEST_H263

#include "videodev2_samsung.h"

#define CAMERA_DEV_NAME   "/dev/video0"

class TError {
public:
	TError(const char *msg) {
		this->msg = msg;
	}
	TError(const TError bitand e) {
		msg = e.msg;
	}
	void Output() {
		std::cerr << msg << std::endl;
	}
	virtual ~TError() {}
protected:
	TError bitand operator=(const TError bitand);
private:
	const char *msg;
};

// Linear memory based image
class TRect {
public:
	TRect():  Addr(0), Size(0), Width(0), Height(0), LineLen(0), BPP(32) {
	}
	virtual ~TRect() {
	}
	bool DrawRect(const TRect bitand SrcRect, int x, int y) const { //bitand �൱�� & ����ʵ��
		if (BPP not_eq 32 or SrcRect.BPP not_eq 32) {//����ǰ������ù����п�������X������ʹ�ö���ɫ����ȣ���8bpp�� 16bpp��24bpp��32bpp��һ������ɫ�����Խ�����ܱ��ֵ�ɫ��Խ�ḻ���� 24bpp�ͱ���Ϊ���ɫ������ʵ�ı���ͼ���ɫ�ʣ�32bppʵ��Ҳֻ��24bpp�� ����Ϊ����ÿ�����ض�ռ�ݶ�����32λ˫�֣��Զ������ر߽磬���ٴ����ٶȣ� 
			// don't support that yet
			throw TError("does not support other than 32 BPP yet");
		}

		// clip
		int x0, y0, x1, y1;
		x0 = x;
		y0 = y;
		x1 = x0 + SrcRect.Width - 1;
		y1 = y0 + SrcRect.Height - 1;
		if (x0 < 0) {
			x0 = 0;
		}
		if (x0 > Width - 1) {
			return true;
		}
		if( x1 < 0) {
			return true;
		}
		if (x1 > Width - 1) {
			x1 = Width - 1;
		}
		if (y0 < 0) {
			y0 = 0;
		}
		if (y0 > Height - 1) {
			return true;
		}
		if (y1 < 0) {
			return true;
		}
		if (y1 > Height - 1) {
			y1 = Height -1;
		}

		//copy
		int copyLineLen = (x1 + 1 - x0) * BPP / 8;
		unsigned char *DstPtr = Addr + LineLen * y0 + x0 * BPP / 8;
		const unsigned char *SrcPtr = SrcRect.Addr + SrcRect.LineLen *(y0 - y) + (x0 - x) * SrcRect.BPP / 8;

		for (int i = y0; i <= y1; i++) {//FrameByffer��Addr������Ļ��ӳ���ַ
			memcpy(DstPtr, SrcPtr, copyLineLen); //memcpy�����Ĺ����Ǵ�Դsrc��ָ���ڴ��ַ����ʼλ�ÿ�ʼ����n���ֽڵ�Ŀ��dest��ָ���ڴ��ַ����ʼλ���С�
			DstPtr += LineLen;
			SrcPtr += SrcRect.LineLen;
		}
		
		
		return true;
	}

	bool DrawRect(const TRect bitand rect) const { // default is Center
		return DrawRect(rect, (Width - rect.Width) / 2, (Height - rect.Height) / 2);//����һ������
	}

	bool Clear() const {
		int i;
		unsigned char *ptr;
		for (i = 0, ptr = Addr; i < Height; i++, ptr += LineLen) {
			memset(ptr, 0, Width * BPP / 8);
		}
		return true;
	}

protected:
	TRect(const TRect bitand);
	TRect bitand operator=( const TRect bitand);

protected:
	unsigned char *Addr;
	int Size;
	int Width, Height, LineLen;
	unsigned BPP;
};



class TFrameBuffer: public TRect {
public:
	TFrameBuffer(const char *DeviceName = "/dev/fb0"): TRect(), fd(-1) {// ���Ƕ���Ļ�Ķ�д�Ϳ���ת���ɶ�/dev/fb0,�����Ƕ���Ļ�Ĳ�����
		Addr = (unsigned char *)MAP_FAILED;

        fd = open(DeviceName, O_RDWR);
		if (fd < 0) {
			throw TError("cannot open frame buffer");
		}
	

        struct fb_fix_screeninfo Fix;
        struct fb_var_screeninfo Var;
		if (ioctl(fd, FBIOGET_FSCREENINFO, bitand Fix) < 0 or ioctl(fd, FBIOGET_VSCREENINFO, bitand Var) < 0) {
			throw TError("cannot get frame buffer information");
		}

		BPP = Var.bits_per_pixel;
	    if (BPP not_eq 32) {
			throw TError("support 32 BPP frame buffer only");
		}

        	Width  = Var.xres;
        	Height = Var.yres;
        	LineLen = Fix.line_length;
      		Size = LineLen * Height;

		int PageSize = getpagesize();    //ʹ��getpagesize�������һҳ�ڴ��С
		Size = (Size + PageSize - 1) / PageSize * PageSize ;
	        Addr = (unsigned char *)mmap(NULL, Size, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);//ӳ����Ļ������ַ
		if (Addr == (unsigned char *)MAP_FAILED) {//������ޱ仯��Ȼ����ʧ����
			throw TError("map frame buffer failed");
			return;
		}
		::close(fd);
		fd = -1;

		Clear();
	}

	virtual ~TFrameBuffer() {
		::munmap(Addr, Size);
		Addr = (unsigned char *)MAP_FAILED;

		::close(fd);
		fd = -1;
	}

protected:
	TFrameBuffer(const TFrameBuffer bitand);
	TFrameBuffer bitand operator=( const TFrameBuffer bitand);
private:
	int fd;
};

class TVideo : public TRect {
public:
	TVideo(const char *DeviceName = CAMERA_DEV_NAME): TRect(), fd(-1) {
		Width = 640;
		Height = 480;
		BPP = 32;
		LineLen = Width * BPP / 8;
		Size = LineLen * Height;
		fd = -1;
        Valid = true;
		Addr = new unsigned char[Size];
		Clear();

        OpenDevice();
        StartStream();
	}
    
	void initLive555();
    bool IsValid() const { return Valid; }
    bool WaitPic();
	bool FetchPicture();
	int toH264(unsigned char* yuv420sp);

	virtual ~TVideo() {
		::close(fd);
		fd = -1;
		delete[] Addr;
		Addr = 0;
	}

protected:
	TVideo(const TVideo bitand);
	TVideo bitand operator=(const TVideo bitand);
    void OpenDevice();
    void StartStream();
    void StopStream();
	
	int fd;
    bool Valid;
    struct pollfd  m_events_c;
	static const int CAPTURE_BUFFER_NUMBER = 1;
	struct { void * data; int len; } captureBuffer[CAPTURE_BUFFER_NUMBER];
	//live555
	CRTSPStream rtspSender;
	
};

void TVideo::initLive555(){
	bool bRet = rtspSender.Init();  
}

//��yuv420ת��h264 ��ʽ
int TVideo::toH264(unsigned char* yuv420sp)
{
	unsigned char *headbuffer;
	unsigned char *buffer;
		unsigned int buf_type = NO_CACHE;
		void *openHandle;
	#if defined TEST_H264
		SSBSIP_MFC_ENC_H264_PARAM *param;
	#elif defined TEST_H263
		SSBSIP_MFC_ENC_H263_PARAM *param;
	#else
		SSBSIP_MFC_ENC_MPEG4_PARAM *param;
	#endif
		
		SSBSIP_MFC_ERROR_CODE err;
		SSBSIP_MFC_ENC_INPUT_INFO iinfo;
		SSBSIP_MFC_ENC_OUTPUT_INFO oinfo;
		
		FILE *fp_nv12, *fp_strm;
		
		int retv = 0;
		//test.nv12
		/*
		fp_nv12 = fopen("hello.yuv","rb");  //���ܴ��ļ����ˣ�����Ҫ��һ�¿ɲ����ԣ������������������NV12
		if(fp_nv12==NULL) {
			fprintf(stderr,"Error: open test.nv12\n");
			retv = 1;
			goto exit_end;
		}
		*/	
	#if defined TEST_H264
		fp_strm = fopen("test2.h264","a+");
	#elif defined TEST_H263
		fp_strm = fopen("test.h263","wb");
	#else
		fp_strm = fopen("test.mpeg4","wb");
	#endif
		if(fp_strm==NULL) {
			fprintf(stderr,"Error: open output file\n");
			retv = 1;
			return 0;
		}
		
		
		openHandle = SsbSipMfcEncOpen(&buf_type);
		if(openHandle == NULL) {
			fprintf(stderr,"Error: SsbSipMfcEncOpen\n");
			retv = 1;
			return -1;
		}else {
		//	printf("MfcEncOpen succeeded\n");
		}
		
		param=(SSBSIP_MFC_ENC_H264_PARAM*)malloc(sizeof(SSBSIP_MFC_ENC_H264_PARAM));
		if(param==NULL) {
			fprintf(stderr,"Error: malloc param\n");
			retv = 1;
					err = SsbSipMfcEncClose(openHandle);
					if(err<0) {
					fprintf(stderr,"Error: SsbSipMfcEncClose. Code %d\n",err);		
			}
			return -1;
		}
		memset(param,0,sizeof(*param));
		
		//common parameters
	#if defined TEST_H264
		param->codecType = H264_ENC;
	#elif defined TEST_H263
		param->codecType = H263_ENC;
	#else
		param->codecType = MPEG4_ENC;
	#endif
		param->SourceWidth = 640;
		param->SourceHeight = 480;
		param->IDRPeriod = 100;
		param->SliceMode = 0; // 0,1,2,4
		param->RandomIntraMBRefresh = 0;
		param->EnableFRMRateControl = 1; // this must be 1 otherwise init error
		param->Bitrate = 128000;
		param->FrameQp = 20; //<=51, the bigger the lower quality
		param->FrameQp_P = 20;
		param->QSCodeMin = 10; // <=51
		param->QSCodeMax = 51; // <=51
		param->CBRPeriodRf = 120;
		param->PadControlOn = 0;
		param->LumaPadVal = 0; // <=255
		param->CbPadVal = 0; //<=255
		param->CrPadVal = 0; //<=255
		param->FrameMap = 0; // encoding input mode (0=linear, 1=tiled) 
		
	#if defined TEST_H264
		// H264 specific
		param->ProfileIDC = 1; // 0=main,1=high,2=baseline
		param->LevelIDC = 40; // level 4.0
		param->FrameQp_B = 20;
		param->FrameRate = 30000; // real frame rate = FrameRate/1000 (refer to S5PV210 datasheet Section 6.3.4.2.2)
		param->SliceArgument = 0;
		param->NumberBFrames = 0; //<=2
		param->NumberReferenceFrames = 2; // <=2
		param->NumberRefForPframes = 2; // <=2
		param->LoopFilterDisable = 1; // 0=enable, 1=disable
		param->LoopFilterAlphaC0Offset = 0; // <=6
		param->LoopFilterBetaOffset = 0; // <=6
		param->SymbolMode = 1; // 0=CAVLC, 1=CABAC
		param->PictureInterlace = 0; // Picture AFF 0=frame coding, 1=field coding, 2=adaptive
		param->Transform8x8Mode = 1; // 0=only 4x4 transform, 1=allow 8x8 trans, 2=only 8x8 trans
		param->EnableMBRateControl = 0;
		param->DarkDisable = 0;
		param->SmoothDisable = 0;
		param->StaticDisable = 0;
		param->ActivityDisable = 0;
	#elif defined TEST_H263
		// H263 specific
		param->FrameRate = 30000;
	#else
		// MPEG4 specific
		param->ProfileIDC = 0; // 0=main,1=high,2=baseline
		param->LevelIDC = 40; // level 4.0
		param->FrameQp_B = 20;
		param->TimeIncreamentRes = 0;
		param->VopTimeIncreament = 0;
		param->SliceArgument = 0;
		param->NumberBFrames = 0; //<=2
		param->DisableQpelME = 0;
	#endif
		
		err = SsbSipMfcEncInit(openHandle,param);
		if(err<0) {
			fprintf(stderr,"Error: SsbSipMfcEncInit. Code %d\n",err);
			retv = 1;
			return retv;
		}else {
			//printf("SsbSipMfcEncInit succeeded\n");
		}
		
	#ifndef TEST_H263
		err = SsbSipMfcEncGetOutBuf(openHandle,&oinfo);
		if(err<0) {
			fprintf(stderr,"Error: SsbSipMfcEncGetOutBuf. Code %d\n",err);
			retv =1;
			return retv;
		}else {
			//printf("SsbSipMfcEncGetOutBuf suceeded\n");
//			printf("�������˸�noth263ͷ\n");
			headbuffer  = new unsigned char[oinfo.headerSize];
			memcpy(headbuffer,oinfo.StrmVirAddr,oinfo.headerSize);
			fwrite(oinfo.StrmVirAddr,1,oinfo.headerSize,fp_strm);
			//addHead(oinfo);
		}
	#endif
		err = SsbSipMfcEncGetOutBuf(openHandle,&oinfo);
		if(err<0) {
			fprintf(stderr,"Error: SsbSipMfcEncGetOutBuf. Code %d\n",err);
			retv =1;
			return retv;
		}else {
			//printf("SsbSipMfcEncGetOutBuf suceeded\n");
			fwrite(oinfo.StrmVirAddr,1,oinfo.headerSize,fp_strm);
//			fprintf(stderr,"headerSize %d\n",oinfo.headerSize);
			
		}
		memset(&iinfo,0,sizeof(iinfo));
		err = SsbSipMfcEncGetInBuf(openHandle,&iinfo);
		if(err<0) {
			fprintf(stderr,"Error: SsbSipMfcEncGetInBuf. Code %d\n",err);
			retv = 1;
			return retv;
		}else {
			//printf("SsbSipMfcEncGetInBuf succeeded\n");
		}

		
		int w=param->SourceWidth;
		int h=param->SourceHeight;
		//int frmcnt = 0;
		//size_t fread ( void *buffer, size_t size, size_t count, FILE *stream) ;
		//�� ��
		//buffer
		//���ڽ������ݵ��ڴ��ַ
		//size
		//Ҫ����ÿ����������ֽ�������λ���ֽ�
		//count
		//Ҫ��count�������ÿ��������size���ֽ�.
		//stream
		//������
		//��fp_nv12����д��linfo.YVirAddr�У�2����
		//����ֵ
		//ʵ�ʶ�ȡ��Ԫ�ظ������������ֵ��count����ͬ��������ļ���β�������󡣴�ferror��feof��ȡ������Ϣ�����Ƿ񵽴��ļ���β����
		//if(fread(iinfo.YVirAddr,1,w*h,fp_nv12)==w*h && fread(iinfo.CVirAddr,1,w*h/2,fp_nv12)==w*h/2) {//��ȡ�ļ��е�NV12���ݣ�Ҫ�޸�Ϊ�ոյõ��ģ���Ϊÿ�ζ�Ҫ�Լ�wait��Ӧ�ò���ѭ��
		memcpy(iinfo.YVirAddr,yuv420sp,w*h);
		memcpy(iinfo.CVirAddr,yuv420sp+w*h,w*h/2);
			err = SsbSipMfcEncSetInBuf(openHandle,&iinfo);
			if(err<0) {
				fprintf(stderr,"Error: SsbSipMfcEncSetInBuf. Code %d\n",err);
				retv = 1;
				return retv;
			}
	#if 0
			err = SsbSipMfcEncSetOutBuf(openHandle,phyOutbuf,virOutbuf,outbufSize);
			if(err<0) {
				fprintf(stderr,"Error: SsbSipMfcEncSetOutBuf. Code %d\n",err);
				retv = 1;
				return retv;
			}
	#endif
			err = SsbSipMfcEncExe(openHandle);
			if(err<0) {
				fprintf(stderr,"Error: SsbSipMfcEncExe. Code %d\n",err);
				retv = 1;
				return retv;
			}
			
			memset(&oinfo,0,sizeof(oinfo));
			err = SsbSipMfcEncGetOutBuf(openHandle,&oinfo);
			if(err<0) {
				fprintf(stderr,"Error: SsbSipMfcEncGetOutBuf. Code %d\n",err);
				retv = 1;
				return retv;
			}
			
			buffer  = new unsigned char[oinfo.headerSize+oinfo.dataSize];
			memcpy(buffer,headbuffer,oinfo.headerSize);
			memcpy(buffer+oinfo.headerSize,oinfo.StrmVirAddr,oinfo.dataSize);
			rtspSender.SendH264Data(buffer,oinfo.dataSize);  
			//fwrite(oinfo.StrmVirAddr,1,oinfo.dataSize,fp_strm);
			delete[] buffer;
			delete[] headbuffer;
			//toRTP(oinfo);  
			//printf("oinfo.StrmVirAddr=0x%x, oinfo.dataSize=%d.\n",(unsigned)oinfo.StrmVirAddr,oinfo.dataSize);
			//printf("Frame # %d encoded\n", frmcnt++);
		
		// clear up
	exit_param:
		free(param);
		err = SsbSipMfcEncClose(openHandle);
		if(err<0) {
			fprintf(stderr,"Error: SsbSipMfcEncClose. Code %d\n",err);
		}
		
	fclose(fp_strm);
		
	exit_end:
		return retv;

}

void TVideo::OpenDevice()
{
	// Open Device
	const char *device = CAMERA_DEV_NAME;
	fd = ::open(device, O_RDWR|O_NONBLOCK);
	if (fd < 0) {
        Valid = false;
		fprintf(stderr, "cannot open device %s\n", device);
		return;
	}
    
	// Check capability
	struct v4l2_capability cap;
	if( ::ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        Valid = false;
		fprintf(stderr, "cannot query capability\n");
		return;
	}
	fprintf(stderr, "start test\n");
			//����֧�ֵķֱ���,�������ͷ���񲢲����������
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		struct v4l2_fmtdesc fmt2;
		struct v4l2_frmsizeenum frmsize;
		struct v4l2_frmivalenum frmival;
		
		fmt2.index = 0;
		fmt2.type = type;
		if(::ioctl(fd, VIDIOC_ENUM_FMT, &fmt2) < 0){
			fprintf(stderr, "enum < 0\n");
		}
		while (::ioctl(fd, VIDIOC_ENUM_FMT, &fmt2) >= 0) {
			frmsize.pixel_format = fmt2.pixelformat;
			frmsize.index = 0;
			while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
				if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
					printf("%dx%d\n", 
									  frmsize.discrete.width,
									  frmsize.discrete.height);
				} else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
					printf("%dx%d\n", 
									  frmsize.stepwise.max_width,
									  frmsize.stepwise.max_height);
				}
					frmsize.index++;
				}
				fmt2.index++;
		}
	//

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        Valid = false;
		fprintf(stderr, "not a video capture device\n");
		return;
	}
    
	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        Valid = false;
		fprintf(stderr, "does not support streaming i/o\n");
		return;
	}
    
    static struct v4l2_input input;
    input.index = 0; 
    if (ioctl(fd, VIDIOC_ENUMINPUT, &input) != 0) {
        Valid = false;
        fprintf(stderr, "No matching index found\n");
        return;
    }
    if (!input.name) {
        Valid = false;
        fprintf(stderr, "No matching index found\n");
        return;
    }
    if (ioctl(fd, VIDIOC_S_INPUT, &input) < 0) {
        Valid = false;
        fprintf(stderr, "VIDIOC_S_INPUT failed\n");
        return;
    }
    
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof fmt);
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = Width;
    fmt.fmt.pix.height      = Height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix.sizeimage = (fmt.fmt.pix.width * fmt.fmt.pix.height * 16) / 8;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (::ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        Valid = false;
        fprintf(stderr, "VIDIOC_S_FMT failed\n");
        return;
    }
    
	bool CouldSetFrameRate = false;
	struct v4l2_streamparm StreamParam;
	memset(&StreamParam, 0, sizeof StreamParam);
	StreamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_PARM, &StreamParam) < 0)  {
        fprintf(stderr, "could not set frame rate\n");
	} else {
		CouldSetFrameRate = StreamParam.parm.capture.capability & V4L2_CAP_TIMEPERFRAME;
	}

    // map the capture buffer...
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof req);
    req.count  = CAPTURE_BUFFER_NUMBER;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if(ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        Valid = false;
    	fprintf(stderr, "request capture buffer failed\n");
    	return;
    }
    
    if (int(req.count) != CAPTURE_BUFFER_NUMBER) {
    	fprintf(stderr, "capture buffer number is wrong\n");
        Valid = false;
        return;
    }

    for (int i = 0; i < CAPTURE_BUFFER_NUMBER; i++) {
    	struct v4l2_buffer b;
    	memset(&b, 0, sizeof b);
    	b.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    	b.memory = V4L2_MEMORY_MMAP;
    	b.index = i;
    	if (ioctl(fd, VIDIOC_QUERYBUF, &b) < 0) {
            Valid = false;
    		fprintf(stderr, "query capture buffer failed\n");
    		return;
    	}
        
    	captureBuffer[i].data = mmap(0, b.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, b.m.offset);
    	captureBuffer[i].len = b.length;
        
    	if (captureBuffer[i].data == MAP_FAILED) {
            Valid = false;
    		fprintf(stderr, "unable to map capture buffer\n");
    		return;
    	}
        //ֱ�Ӵ����������ͼƬ��һ����ʲô���ӣ�ÿ��ִ��ָ�����һ��ImageSize.
        fprintf(stderr, "ImageSize[%d] = %ld\n", i, b.length);
    }

    if (Valid) {
        fprintf(stderr, "Open Device OK!\n");
    }

    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_CAMERA_CHECK_DATALINE;
    ctrl.value = 0;
    if(ioctl(fd, VIDIOC_S_CTRL,&ctrl)) {
        fprintf(stderr, "VIDIOC_S_CTRL V4L2_CID_CAMERA_CHECK_DATALINE failed\n");
        Valid = false;
        return;
    }

    memset(&m_events_c, 0, sizeof(m_events_c));
    m_events_c.fd = fd;
    m_events_c.events = POLLIN | POLLERR;  //���� �� �� �쳣�¼�
    
	return;
}


void TVideo::StartStream()
{
    for (int i = 0; i < CAPTURE_BUFFER_NUMBER; i++) {
        struct v4l2_buffer b;
        memset(&b, 0, sizeof b);
        b.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;

        if (ioctl (fd, VIDIOC_QBUF, &b) < 0) {
            Valid = false;
            fprintf(stderr, "queue capture failed\n");
            return;
        }
    }
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl (fd, VIDIOC_STREAMON, &type) < 0) {
        Valid = false;
        fprintf(stderr, "cannot start stream\n");
        return;
    }

    if (Valid) {
        fprintf(stderr, "StartStream OK!\n");
    }
}

void TVideo::StopStream()
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl (fd, VIDIOC_STREAMOFF, &type) < 0) {
        Valid = false;
        fprintf(stderr, "cannot stop stream\n");
        return;
    }
}

bool TVideo::WaitPic()
{//int poll(struct pollfd fds[], nfds_t nfds, int timeout)��
//����˵��:
//fds����һ��struct pollfd�ṹ���͵����飬���ڴ����Ҫ�����״̬��Socket��������ÿ�������������֮��ϵͳ�������������飬���������ȽϷ��㣻�ر��Ƕ��� socket���ӱȽ϶������£���һ���̶��Ͽ�����ߴ����Ч�ʣ���һ����select()������ͬ������select()����֮��select() �����������������socket���������ϣ�����ÿ�ε���select()֮ǰ�������socket���������¼��뵽�����ļ����У��� �ˣ�select()�����ʺ���ֻ���һ��socket���������������poll()�����ʺ��ڴ���socket�������������
//nfds��nfds_t���͵Ĳ��������ڱ������fds�еĽṹ��Ԫ�ص���������
//timeout����poll��������������ʱ�䣬��λ�����룻

    int ret = poll(&m_events_c,  1, 10000);  //�ɹ�ʱ��poll()���ؽṹ����revents��Ϊ0���ļ�����������������ڳ�ʱǰû���κ��¼�������poll()����0��ʧ��ʱ��poll()����-1��������errnoΪ����ֵ֮һ��
    if (ret > 0) {
        return true;
    }
    return false;
}
//yuvתrgb
static void decodeYUV420SP(unsigned int* rgbBuf, unsigned char* yuv420sp, int width, int height) {  
    int frameSize = width * height;  

    int i = 0, y = 0;  
    int uvp = 0, u = 0, v = 0;  
    int y1192 = 0, r = 0, g = 0, b = 0;  
    unsigned int xrgb8888;
    int xrgb8888Index = 0;

    for (int j = 0, yp = 0; j < height; j++) {  
        uvp = frameSize + (j >> 1) * width;  
        u = 0;  
        v = 0;  
        for (i = 0; i < width; i++, yp++) {  
            y = (0xff & ((int) yuv420sp[yp])) - 16;  
            if (y < 0) y = 0;  
            if ((i & 1) == 0) {  
                v = (0xff & yuv420sp[uvp++]) - 128;  
                u = (0xff & yuv420sp[uvp++]) - 128;  
            }  

            y1192 = 1192 * y;  
            r = (y1192 + 1634 * v);  
            g = (y1192 - 833 * v - 400 * u);  
            b = (y1192 + 2066 * u);  

            if (r < 0) r = 0; else if (r > 262143) r = 262143;  
            if (g < 0) g = 0; else if (g > 262143) g = 262143;  
            if (b < 0) b = 0; else if (b > 262143) b = 262143; 


            r = (unsigned char)(r >> 10);  
            g = (unsigned char)(g >> 10);  
            b = (unsigned char)(b >> 10); 

            xrgb8888 = (unsigned int)((r << 16) | (g << 8) | b);
            rgbBuf[xrgb8888Index++] = xrgb8888;
        }  
    }  
}

bool TVideo::FetchPicture()
{
	struct v4l2_buffer b;//b ������
	memset(&b, 0, sizeof b);
	b.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	b.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_DQBUF, &b) < 0) {
        Valid = false;
		fprintf(stderr, "cannot fetch picture(VIDIOC_DQBUF failed)\n");
		return false;
	}

    
    void *data_ = captureBuffer[b.index].data; //captuerBufferֻ��������Ҳ����˵һ������һ�����������л���frameBuffer�ã������ڴ�����ô���Ӧ�û�û��
    unsigned int len = b.bytesused;
    unsigned int index = b.index;
    //wo zi ji shi yi fa
    //FILE *file_fd;//yeshiwoxiede 
    //file_fd = fopen("test-mmap.yuv", "a+");//ͼƬ�ļ���
    unsigned char* data = (unsigned char*) data_; //������Ӧ���ǰ�ԭʼ���ݷŵ���data����
	//fwrite(data, Width*Height*3/2, 1, file_fd); //����д���ļ���,��ת��֮ǰд��ȥ
    decodeYUV420SP((unsigned int*)Addr, data, Width, Height);//������ת���� 
	toH264(data);//������ת��h264
    //fclose(file_fd);//wo xie de 
   // fprintf(stderr, "save yuyv file ok\n");

	if (ioctl (fd, VIDIOC_QBUF, &b) < 0) {
        Valid = false;
		fprintf(stderr, "cannot fetch picture(VIDIOC_QBUF failed)\n");
		return false;
	}
    
    return true;
}

int main(int argc, char **argv)
{
	try {
		TFrameBuffer FrameBuffer;
		TVideo Video;
		Video.initLive555();
        while(Video.IsValid()) {//������ã�����ѭ��n
            if (Video.WaitPic()) {//�ȴ�ȡͼƬ
                if (Video.FetchPicture()) {//���ȡ��
                    FrameBuffer.DrawRect(Video);//��ʾ�ڵ�Ƭ����
                }
            }
        }
	} catch (TError bitand e) {
		e.Output();
		return 1;
	}

	return 0;
}
