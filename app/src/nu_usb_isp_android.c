#include <stdio.h>
#include <unistd.h> // 為了 usleep
#include <time.h>
#include <string.h>
#include <errno.h>
#include <libusb/libusb.h> // 使用 libusb-1.0

#define CMD_UPDATE_APROM    0x000000A0
#define CMD_UPDATE_CONFIG   0x000000A1
#define CMD_READ_CONFIG     0x000000A2
#define CMD_ERASE_ALL       0x000000A3
#define CMD_SYNC_PACKNO     0x000000A4
#define CMD_GET_FWVER       0x000000A6
#define CMD_APROM_SIZE      0x000000AA
#define CMD_RUN_APROM       0x000000AB
#define CMD_RUN_LDROM       0x000000AC
#define CMD_RESET           0x000000AD

#define CMD_GET_DEVICEID    0x000000B1

#define CMD_PROGRAM_WOERASE   0x000000C2
#define CMD_PROGRAM_WERASE    0x000000C3
#define CMD_READ_CHECKSUM     0x000000C8
#define CMD_WRITE_CHECKSUM    0x000000C9
#define CMD_GET_FLASHMODE     0x000000CA

#define APROM_MODE  1
#define LDROM_MODE  2

#define BOOL    unsigned char
//#define PAGE_SIZE           0x00000200      /* Page size */

#define PACKET_SIZE 64
#define FILE_BUFFER 128
#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif
unsigned char rcvbuf[PACKET_SIZE];
unsigned char sendbuf[PACKET_SIZE];
unsigned char aprom_buf[512];
unsigned int send_flag = FALSE;
unsigned int recv_flag = FALSE;
unsigned int g_packno = 1;
unsigned short gcksum;

// 函式原型宣告
unsigned short Checksum(unsigned char *buf, unsigned int len);
void WordsCpy(void *dest, void *src, unsigned int size);
BOOL CmdSyncPackno(int flag);
BOOL CmdGetCheckSum(int flag, int start, int len, unsigned short *cksum);
BOOL CmdGetDeviceID(int flag, unsigned int *devid);
BOOL CmdGetConfig(int flag, unsigned int *config);
BOOL CmdPutApromSize(int flag, unsigned int apsize);
BOOL CmdEraseAllChip(int flag);
// *** MODIFIED: 函式原型包含 filename 參數 ***
BOOL CmdUpdateAprom(int flag, const char *filename); 

#define dbg_printf printf
// 注意：這種直接的類型轉換在某些架構上可能有效能或對齊問題，但暫時保留原始邏輯
#define inpw(addr)          (*(unsigned int *)(addr))


// libusb-1.0 全域變數
libusb_device_handle *udev = NULL;
libusb_context *ctx = NULL;

int main(int argc, char *argv[])
{
    clock_t start_time, end_time;
    float total_time = 0;
    start_time = clock(); /* mircosecond */

    int r = 1, i = 0;

    // *** NEW: 檢查命令列參數 ***
    if (argc < 2) {
        // argv[0] 通常是程式的名稱
        fprintf(stderr, "Usage: %s <firmware_file.bin>\n", argv[0]);
        return -1;
    }
    const char *filepath = argv[1]; // 取得檔案路徑

    // *** NEW: 初始化 libusb-1.0 ***
    r = libusb_init(&ctx);
    if (r < 0) {
        fprintf(stderr, "libusb_init error %d: %s\n", r, libusb_error_name(r));
        return -1;
    }

    // *** NEW: 透過 VID/PID 查找並開啟設備 ***
    // 0x0416:0xa317 是 Nuvoton
    udev = libusb_open_device_with_vid_pid(ctx, 0x0416, 0x5020);
    if (udev == NULL) {
        printf("USB IO Card (0416:a317) not found.\n");
        libusb_exit(ctx);
        return -1;
    }
    printf("Found device 0416:a317, opening...\n");


    // *** NEW: 必要時分離核心驅動程式 ***
    r = libusb_detach_kernel_driver(udev, 0); // 假設介面為 0
    if (r < 0 && r != LIBUSB_ERROR_NOT_SUPPORTED && r != LIBUSB_ERROR_NOT_FOUND) {
        fprintf(stderr, "libusb_detach_kernel_driver error %d: %s\n", r, libusb_error_name(r));
    } else {
        printf("libusb_detach_kernel_driver: ret %d\n", r);
    }

    // *** NEW: 設定 configuration ***
    r = libusb_set_configuration(udev, 1); // 通常是 Config 1
    if (r < 0) {
        fprintf(stderr, "libusb_set_configuration error %d: %s\n", r, libusb_error_name(r));
        libusb_close(udev);
        libusb_exit(ctx);
        return -1;
    }

    // *** NEW: 聲明介面 ***
    r = libusb_claim_interface(udev, 0); // 假設介面為 0
    if (r < 0) {
        fprintf(stderr, "libusb_claim_interface error %d: %s\n", r, libusb_error_name(r));
        fprintf(stderr, "Hint: Run with sudo or set udev rules for this device.\n");
        libusb_close(udev);
        libusb_exit(ctx);
        return -1;
    }
    printf("Successfully claimed interface\n");
    printf("Using firmware file: %s\n", filepath); // 印出要使用的檔案

    //ISP 更新流程
    // *** MODIFIED: 將檔案路徑傳遞給函式 ***
    CmdUpdateAprom(FALSE, filepath);

    // *** NEW: 釋放介面並關閉 USB 設備 ***
    libusb_release_interface(udev, 0);
    libusb_close(udev);
    libusb_exit(ctx); // 清理 libusb session

    end_time = clock();
    /* CLOCKS_PER_SEC is defined at time.h */
    total_time = (float)(end_time - start_time) / CLOCKS_PER_SEC;

    printf("Time : %f sec \n", total_time);
    return 0; // 正常退出
}


//Copies the values of num bytes from the location pointed
//to by source directly to the memory block pointed to by destination.
void WordsCpy(void *dest, void *src, unsigned int size)
{
    unsigned char *pu8Src, *pu8Dest;
    unsigned int i;

    pu8Dest = (unsigned char *)dest;
    pu8Src  = (unsigned char *)src;

    for (i = 0; i < size; i++)
        pu8Dest[i] = pu8Src[i];
}

//Calculate the starting address and length of the incoming indicator, and return total checksum
unsigned short Checksum(unsigned char *buf, unsigned int len)
{
    int i;
    unsigned short c;

    for (c = 0, i = 0; i < len; i++) {
        c += buf[i];
    }
    return (c);
}

//ISP packet is 64 bytes for command packet sent
BOOL SendData(void)
{
    // *** MODIFIED: Use libusb_interrupt_transfer ***
    int r;
    int actual_length;

    gcksum = Checksum(sendbuf, PACKET_SIZE);

    // Endpoint 0x02 (OUT), timeout 10000ms
    r = libusb_interrupt_transfer(udev, 0x02, sendbuf, PACKET_SIZE, &actual_length, 10000);

    if (r == LIBUSB_SUCCESS && actual_length == PACKET_SIZE) {
        return TRUE;
    } else {
        fprintf(stderr, "SendData error %d: %s\n", r, libusb_error_name(r));
        return FALSE;
    }
}

//ISP packet is 64 bytes for fixed ack packet received
BOOL RcvData(unsigned int count)
{
    // *** MODIFIED: Use libusb_interrupt_transfer ***
    int r;
    int actual_length;
    unsigned short lcksum, i;
    unsigned char *pBuf;

    // Endpoint 0x81 (IN), timeout 10000ms
    r = libusb_interrupt_transfer(udev, 0x81, rcvbuf, PACKET_SIZE, &actual_length, 10000);

    if (r != LIBUSB_SUCCESS) {
        fprintf(stderr, "RcvData error %d: %s\n", r, libusb_error_name(r));
        return FALSE;
    }
    
    if (actual_length != PACKET_SIZE) {
        fprintf(stderr, "RcvData: incorrect packet size received (%d)\n", actual_length);
        return FALSE;
    }

    pBuf = rcvbuf;
    WordsCpy(&lcksum, pBuf, 2);
    pBuf += 4;

    if (inpw(pBuf) != g_packno)
    {
        dbg_printf("g_packno=%d rcv %d\n", g_packno, inpw(pBuf));
        return FALSE;
    }
    else
    {
        if (lcksum != gcksum)
        {
            dbg_printf("gcksum=%x lcksum=%x\n", gcksum, lcksum);
            return FALSE;
        }
        g_packno++;
        return TRUE;
    }
}

//This command is used to synchronize packet number with ISP.
//Before sending any command, master/host need send the command to synchronize packet number with ISP.
BOOL CmdSyncPackno(int flag)
{
    BOOL Result;
    unsigned long cmdData;

    //sync send&recv packno
    memset(sendbuf, 0, PACKET_SIZE);
    cmdData = CMD_SYNC_PACKNO;
    WordsCpy(sendbuf + 0, &cmdData, 4);
    WordsCpy(sendbuf + 4, &g_packno, 4);
    WordsCpy(sendbuf + 8, &g_packno, 4);
    g_packno++;

    Result = SendData();
    if (Result == FALSE)
        return Result;

    Result = RcvData(1);

    return Result;
}

//This command is used to get version of ISP
BOOL CmdFWVersion(int flag, unsigned int *fwver)
{
    BOOL Result;
    unsigned long cmdData;
    unsigned int lfwver;

    //sync send&recv packno
    memset(sendbuf, 0, PACKET_SIZE);
    cmdData = CMD_GET_FWVER;
    WordsCpy(sendbuf + 0, &cmdData, 4);
    WordsCpy(sendbuf + 4, &g_packno, 4);
    g_packno++;

    Result = SendData();
    if (Result == FALSE)
        return Result;

    Result = RcvData(1);
    if (Result)
    {
        WordsCpy(&lfwver, rcvbuf + 8, 4);
        *fwver = lfwver;
    }

    return Result;
}

//This command is used to get product ID.
//PC needs this ID to inquire size of APROM size and inform ISP.
BOOL CmdGetDeviceID(int flag, unsigned int *devid)
{
    BOOL Result;
    unsigned long cmdData;
    unsigned int ldevid;

    //sync send&recv packno
    memset(sendbuf, 0, PACKET_SIZE);
    cmdData = CMD_GET_DEVICEID;
    WordsCpy(sendbuf + 0, &cmdData, 4);
    WordsCpy(sendbuf + 4, &g_packno, 4);
    g_packno++;

    Result = SendData();
    if (Result == FALSE)
        return Result;

    Result = RcvData(1);
    if (Result)
    {
        WordsCpy(&ldevid, rcvbuf + 8, 4);
        *devid = ldevid;
    }

    return Result;
}

//This command is used to instruct ISP to read Config0 and Config1 information of flash memory,
//and transmit them to host.
BOOL CmdGetConfig(int flag, unsigned int *config)
{
    BOOL Result;
    unsigned long cmdData;
    unsigned int lconfig[2];

    //sync send&recv packno
    memset(sendbuf, 0, PACKET_SIZE);
    cmdData = CMD_READ_CONFIG;
    WordsCpy(sendbuf + 0, &cmdData, 4);
    WordsCpy(sendbuf + 4, &g_packno, 4);
    g_packno++;

    Result = SendData();
    if (Result == FALSE)
        return Result;

    Result = RcvData(1);
    if (Result)
    {
        WordsCpy(&lconfig[0], rcvbuf + 8, 4);
        WordsCpy(&lconfig[1], rcvbuf + 12, 4);
        config[0] = lconfig[0];
        config[1] = lconfig[1];
    }

    return Result;
}

//This command is used to instruct ISP to update Config0 and Config1.
BOOL CmdUpdateConfig(int flag, unsigned int *conf)
{
    BOOL Result;
    unsigned long cmdData;

    //sync send&recv packno
    memset(sendbuf, 0, PACKET_SIZE);
    cmdData = CMD_UPDATE_CONFIG;
    WordsCpy(sendbuf + 0, &cmdData, 4);
    WordsCpy(sendbuf + 4, &g_packno, 4);
    WordsCpy(sendbuf + 8, conf, 8);
    g_packno++;

    Result = SendData();
    if (Result == FALSE)
        return Result;

    Result = RcvData(2);

    return Result;
}

//for this commands
//CMD_RUN_APROM, CMD_RUN_LDROM, CMD_RESET
//CMD_ERASE_ALL, CMD_GET_FLASHMODE, CMD_WRITE_CHECKSUM
BOOL CmdRunCmd(unsigned int cmd, unsigned int *data)
{
    BOOL Result;
    unsigned int cmdData, i;

    //sync send&recv packno
    memset(sendbuf, 0, PACKET_SIZE);
    cmdData = cmd;
    WordsCpy(sendbuf + 0, &cmdData, 4);
    WordsCpy(sendbuf + 4, &g_packno, 4);
    if (cmd == CMD_WRITE_CHECKSUM)
    {
        WordsCpy(sendbuf + 8, &data[0], 4);
        WordsCpy(sendbuf + 12, &data[1], 4);
    }
    g_packno++;

    Result = SendData();
    if (Result == FALSE)
        return Result;

    if ((cmd == CMD_ERASE_ALL) || (cmd == CMD_GET_FLASHMODE)
            || (cmd == CMD_WRITE_CHECKSUM))
    {
        Result = RcvData(2);
        if (Result)
        {
            if (cmd == CMD_GET_FLASHMODE)
            {
                WordsCpy(&cmdData, rcvbuf + 8, 4);
                *data = cmdData;
            }
        }

    }
    else if ((cmd == CMD_RUN_APROM) || (cmd == CMD_RUN_LDROM)
             || (cmd == CMD_RESET))
    {
        // *** BUG FIX: ***
        // Original 'sleep(500)' was 500 seconds.
        // Changed to usleep(500000) for 500ms (0.5 seconds).
        usleep(500000);
    }
    return Result;
}

unsigned int file_totallen;
unsigned int file_checksum; // 注意：此變數在程式中似乎未被正確賦值和使用

//the ISP flow, show to update the APROM in target chip
// *** MODIFIED: Added const char *filename parameter ***
BOOL CmdUpdateAprom(int flag, const char *filename)
{
    BOOL Result;
    unsigned int devid, config[2], i, mode, j;
    unsigned long cmdData, startaddr;
    unsigned short get_cksum;
    unsigned char Buff[256];
    unsigned int s1;
    FILE *fp = NULL; // 初始化為 NULL

    g_packno = 1;

    //synchronize packet number with ISP.
    Result = CmdSyncPackno(flag);
    if (Result == FALSE)
    {
        dbg_printf("send Sync Packno cmd fail\n");
        goto out1;
    }

    //This command is used to get boot selection (BS) bit.
    Result = CmdRunCmd(CMD_GET_FLASHMODE, &mode);
    if (mode != LDROM_MODE)
    {
        dbg_printf("fail\n");
        goto out1;
    }
    else
    {
        dbg_printf("ok\n");
    }

    //get product ID
    CmdGetDeviceID(flag, &devid);
    printf("DeviceID: 0x%x\n", devid);

    //get config bit
    CmdGetConfig(flag, config);
    dbg_printf("config0: 0x%x\n", config[0]);
    dbg_printf("config1: 0x%x\n", config[1]);

    //open bin file for APROM
    // *** MODIFIED: Use the 'filename' parameter in fopen ***
    if ((fp = fopen(filename, "rb")) == NULL)
    {
        printf("APROM FILE (%s) OPEN FAILED\n\r", filename);
        perror("fopen error"); // 顯示詳細的檔案開啟錯誤
        Result = FALSE;
        goto out1;
    }

    //get BIN file size
    fseek(fp, 0, SEEK_END);
    file_totallen = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    //first isp package
    memset(sendbuf, 0, PACKET_SIZE);
    cmdData = CMD_UPDATE_APROM;      //CMD_UPDATE_APROM Command
    WordsCpy(sendbuf + 0, &cmdData, 4);
    WordsCpy(sendbuf + 4, &g_packno, 4);
    g_packno++;

    //start address
    startaddr = 0;
    WordsCpy(sendbuf + 8, &startaddr, 4);
    WordsCpy(sendbuf + 12, &file_totallen, 4);

    fread(&sendbuf[16], sizeof(char), 48, fp);

    //send CMD_UPDATE_APROM
    Result = SendData();
    if (Result == FALSE)
        goto out1;

    //for erase time delay using, other bus need it.
    sleep(2); // 2 second sleep for erase
    Result = RcvData(20);
    if (Result == FALSE)
        goto out1;

    //Send other BIN file data in ISP package
    for (i = 48; i < file_totallen; i = i + 56)
    {
        dbg_printf("i=%d \n\r", i);

        //clear buffer
        for (j = 0; j < 64; j++)
        {
            sendbuf[j] = 0;
        }

        WordsCpy(sendbuf + 4, &g_packno, 4);
        g_packno++;
        if ((file_totallen - i) > 56)
        {
            fread(&sendbuf[8], sizeof(char), 56, fp);
            Result = SendData();
            if (Result == FALSE)
                goto out1;
            Result = RcvData(2);
            if (Result == FALSE)
                goto out1;
        }
        else
        {

            fread(&sendbuf[8], sizeof(char), file_totallen - i, fp);

            Result = SendData();
            if (Result == FALSE)
                goto out1;
            Result = RcvData(2);
            if (Result == FALSE)
                goto out1;
            #if 0
            // 這段原始碼是註解掉的，看起來是用於檢查checksum
            // 但 file_checksum 變數從未被賦值，所以保持註解
            WordsCpy(&get_cksum, rcvbuf + 8, 2);
            if ((file_checksum & 0xffff) != get_cksum)
            {
                Result = FALSE;
                goto out1;
            }
            #endif
        }
    }

out1:
    if (fp != NULL) {
        fclose(fp); // 確保檔案被開啟了才關閉
    }
    return Result;

}