#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include "mfc.h"
#include "SsbSipMfcApi.h"
#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "mfc_interface.h"
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/videodev2.h>
#include <sys/poll.h>
MFC::MFC()
{    
}

SSBSIP_MFC_ERROR_CODE MFC::initMFC(int w, int h, int qb)
{
    SSBSIP_MFC_ERROR_CODE ret;

    width=w;
    height=h;
    param = (SSBSIP_MFC_ENC_H264_PARAM*)malloc(sizeof(SSBSIP_MFC_ENC_H264_PARAM));
    memset(param, 0 , sizeof(SSBSIP_MFC_ENC_H264_PARAM));
    param->SourceWidth=width;
    param->SourceHeight=height;
    param->codecType = H264_ENC;
    param->SourceWidth = 640;
    param->SourceHeight = 480;
    param->IDRPeriod = 100;
    param->SliceMode = 0; // 0,1,2,4
    param->RandomIntraMBRefresh = 0;
    param->EnableFRMRateControl = 1; // this must be 1 otherwise init error
    param->Bitrate = 10 * 1000 * 1000;
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

    int hMFCOpen;
    unsigned int mapped_addr;
    int mapped_size;
    mfc_common_args EncArg;
    hMFCOpen = open("/dev/s3c-mfc", O_RDWR | O_NDELAY);
    if (hMFCOpen < 0) {
        printf("SsbSipMfcEncOpen: MFC Open failure\n");
     
    }
    printf("hmfcOpen:%d\n",hMFCOpen);

    
    mapped_size = ioctl(hMFCOpen, IOCTL_MFC_GET_MMAP_SIZE, &EncArg);
    if (EncArg.ret_code != MFC_RET_OK) {
        printf("SsbSipMfcEncOpen: IOCTL_MFC_GET_MMAP_SIZE failed");
        mapped_size = MMAP_BUFFER_SIZE_MMAP;
    }
    unsigned char *Addr;
    int fd;
    fd = open("/dev/video0", O_RDWR);
		if (fd < 0) {
			printf("cannot open frame buffer");
		}
    Addr=(unsigned char *)mmap(NULL, 640*480, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);

    printf("mapped_size:%d\n",Addr);
    printf("mapped_size:%d\n",mapped_size);
    mapped_addr = (unsigned int)mmap(NULL, 0, PROT_READ|PROT_WRITE, MAP_SHARED, hMFCOpen, 0);
    if (!mapped_addr) {
        printf("SsbSipMfcEncOpen: FIMV5.0 driver address mapping failed\n");
  
    }
    printf("mapped_addr:%p\n",mapped_addr);



    if(hOpen == NULL)
    {
        printf("SsbSipMfcEncOpen Failed\n");
        ret = MFC_RET_FAIL;
        return ret;
    }

    if(SsbSipMfcEncInit(hOpen, param) != MFC_RET_OK)
    {
        printf("SsbSipMfcEncInit Failed\n");
        ret = MFC_RET_FAIL;
        goto out;
    }

    if(SsbSipMfcEncGetInBuf(hOpen, &input_info) != MFC_RET_OK)
    {
        printf("SsbSipMfcEncGetInBuf Failed\n");
        ret = MFC_RET_FAIL;
        goto out;
    }

    ret=SsbSipMfcEncGetOutBuf(hOpen, &output_info);
    if(output_info.headerSize <= 0)
    {
        printf("Header Encoding Failed\n");
        ret = MFC_RET_FAIL;
        goto out;
    }
    printf("MFC Header Encoding Success\n");
    headerSize=output_info.headerSize;
    printf("headerSize%d\n",headerSize);
    printf("StrmVirAddr:%p\n",output_info.StrmVirAddr);
    //memcpy(header,output_info.StrmVirAddr,headerSize);//ying gai shi ta de wen ti 
    printf("MFC init success:: Yphy(0x%08x) Cphy(0x%08x)\n",input_info.YPhyAddr, input_info.CPhyAddr);
    printf("neng nu neng guo?\n");
    return ret;
out:
    SsbSipMfcEncClose(hOpen);
    return ret;
}

int MFC::getHeader(unsigned char **p)
{
    //memcpy(*p,header,headerSize);
    *p=header;
    return headerSize;
}

void MFC::getInputBuf(void **Y,void **UV)
{
    *Y=input_info.YVirAddr;
    *UV=input_info.CVirAddr;
}

int MFC::encode(void **h264)
{

    if(SsbSipMfcEncExe(hOpen) != MFC_RET_OK){
        printf("Encoding Failed\n");
        return 0;
    }
    SsbSipMfcEncGetOutBuf(hOpen, &output_info);
    if(output_info.StrmVirAddr == NULL)
    {
        printf("SsbSipMfcEncGetOutBuf Failed\n");
        return 0;
    }
    *h264=output_info.StrmVirAddr;
    return output_info.dataSize;
}

void MFC::closeMFC()
{
    SsbSipMfcEncClose(hOpen);
}
