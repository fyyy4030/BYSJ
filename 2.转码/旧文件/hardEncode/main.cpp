#include<stdio.h>
#include<stdlib.h>
#include"mfc.h"


int main()
{
    char *InputPath="hello.yuv";
    char *OutputPath="world.yuv.h264";
    int width=640;
    int height=480;
    int qb=30;
    FILE *fp_yuv,*fp_strm;
    MFC *mfc=new MFC();
    SSBSIP_MFC_ERROR_CODE ret;
    unsigned char *header;
    void *Y;
    void *UV;
    void *UVBuf;
    void *h264;
    printf("zao shang hao\n");
    fp_yuv = fopen(InputPath, "rb");
    if (fp_yuv == NULL) {
        printf("Cannot open input YUV file.(%s)\n", InputPath);
        ret = MFC_RET_FAIL;
        return 0;
    }
    fp_strm = fopen(OutputPath, "wb");
    if (fp_strm == NULL) {
        printf("Cannot open output stream file.(%s)\n", OutputPath);
        ret = MFC_RET_FAIL;
        return 0;
    }
    printf("before init\n");
    ret=mfc->initMFC(width,height,qb);
    int headerSize=mfc->getHeader(&header);
    printf("before wirte header\n");
    fwrite(header,1,headerSize,fp_strm);   //写入文件头
    mfc->getInputBuf(&Y,&UV);
    int readSize;

    UVBuf=malloc(width * height);
    char *p_nv12,*p_cb,*p_cr;
    int frame=0;
    printf("Y(0x%08x) UV(0x%08x)\n",Y, UVBuf);
    printf("before Y\n");
    do{
        readSize = fread(Y, 1, 1, fp_yuv);
        if(readSize == 0){
            printf("Y readSize == 0\n");
            break;
        }

        readSize = fread(UVBuf, 1, (width * height) >> 1, fp_yuv);
        if(readSize == 0){
            printf("CbCr readSize == 0\n");
            break;
        }
    printf("before convert\n");
        // convert YV12 -> NV12
        p_nv12 = (char *)UV;
        p_cb = (char *)UVBuf;
        p_cr = (char *)UVBuf;
        p_cr += ((width * height) >> 2);

        for(int i = 0; i < (width * height) >> 2; i++){
            *p_nv12 = *p_cb;
            p_nv12++;
            *p_nv12 = *p_cr;
            p_nv12++;
            p_cb++;
            p_cr++;
        }
        readSize=mfc->encode(&h264);
        fwrite(h264, 1, readSize, fp_strm);
        printf("frame:%d\n",frame++);
    }while(1);

    mfc->closeMFC();
    free(UVBuf);
    fclose(fp_yuv);
    fclose(fp_strm);
    return 0;
}
