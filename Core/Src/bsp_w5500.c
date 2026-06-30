#include "bsp_w5500.h"

// 引入由 CubeMX 生成的 SPI1 句柄
extern SPI_HandleTypeDef hspi1;

/***************----- 网络参数变量定义 (可根据实际情况修改) -----***************/
unsigned char Gateway_IP[4] = {192, 168, 1, 1};     // 网关IP地址
unsigned char Sub_Mask[4]   = {255, 255, 255, 0};   // 子网掩码
unsigned char Phy_Addr[6]   = {0x00, 0x08, 0xDC, 0x11, 0x22, 0x33}; // MAC地址
unsigned char IP_Addr[4]    = {192, 168, 1, 111};   // 本机IP地址

unsigned char S0_Port[2]    = {0x13, 0x88}; // 端口0的本地监听端口号 (5000)
unsigned char S0_DIP[4]     = {192, 168, 1, 100};   // 端口0发送目标的IP地址
unsigned char S0_DPort[2]   = {0x17, 0x70}; // 端口0发送目标的端口号 (6000)

/***************----- 硬件片选控制宏 -----***************/
// 依赖于 CubeMX 中配置的 User Label: W5500_CS
#define W5500_CS_LOW()   HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_RESET)
#define W5500_CS_HIGH()  HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET)

/**
  * @brief  使用 HAL 库实现的 SPI 读写1字节底层函数
  * @param  TxData: 待发送的字节
  * @retval 接收到的字节
  */
unsigned char W5500_SPI_ReadWriteByte(unsigned char TxData)
{
    unsigned char RxData = 0;
    // 超时时间设为 10ms
    HAL_SPI_TransmitReceive(&hspi1, &TxData, &RxData, 1, 10);
    return RxData;
}

/**
  * @brief  发送16位数据 (短整型)
  */
void W5500_SPI_Send_Short(unsigned short dat)
{
    W5500_SPI_ReadWriteByte(dat >> 8);   // 高位在前
    W5500_SPI_ReadWriteByte(dat & 0xFF); // 低位在后
}

/**
  * @brief  硬件复位W5500 (依赖 User Label: W5500_RST)
  */
void W5500_Hardware_Reset(void)
{
    // 复位引脚拉低至少 500us
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(50);
    // 复位引脚拉高释放
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(200);

    // 等待W5500物理层完成初始化(网线连接正常)
    // 如果不希望没插网线时死机，可以去掉这个死循环或加超时判断
//    while((Read_W5500_1Byte(PHYCFGR) & LINK) == 0);
}

/**
  * @brief  向W5500的通用寄存器写1字节
  */
void Write_W5500_1Byte(unsigned short reg, unsigned char dat)
{
    W5500_CS_LOW();
    W5500_SPI_Send_Short(reg); // 地址
    W5500_SPI_ReadWriteByte(FDM1 | RWB_WRITE | COMMON_R); // 控制字节: 1字节+写+通用寄存器
    W5500_SPI_ReadWriteByte(dat); // 数据
    W5500_CS_HIGH();
}

/**
  * @brief  向W5500的通用寄存器写2字节
  */
void Write_W5500_2Byte(unsigned short reg, unsigned short dat)
{
    W5500_CS_LOW();
    W5500_SPI_Send_Short(reg);
    W5500_SPI_ReadWriteByte(FDM2 | RWB_WRITE | COMMON_R);
    W5500_SPI_Send_Short(dat);
    W5500_CS_HIGH();
}


/**
  * @brief  读取指定Socket寄存器的1字节数据 (修复状态机读取的核心函数)
  */
unsigned char Read_W5500_SOCK_1Byte(SOCKET s, unsigned short reg)
{
    unsigned char i;
    W5500_CS_LOW();
    W5500_SPI_Send_Short(reg);
    // (s * 0x20 + 0x08) 动态定位到对应 Socket 的寄存器块
    W5500_SPI_ReadWriteByte(FDM1 | RWB_READ | (s * 0x20 + 0x08));
    i = W5500_SPI_ReadWriteByte(0x00);
    W5500_CS_HIGH();
    return i;
}
/**
  * @brief  向W5500的通用寄存器写连续N字节数据
  */
void Write_W5500_nByte(unsigned short reg, unsigned char *dat_ptr, unsigned short size)
{
    unsigned short i;
    W5500_CS_LOW();
    W5500_SPI_Send_Short(reg);
    W5500_SPI_ReadWriteByte(VDM | RWB_WRITE | COMMON_R); // 可变长度(VDM)
    for(i = 0; i < size; i++)
    {
        W5500_SPI_ReadWriteByte(*dat_ptr++);
    }
    W5500_CS_HIGH();
}

/**
  * @brief  向指定的Socket寄存器写1字节数据
  */
void Write_W5500_SOCK_1Byte(SOCKET s, unsigned short reg, unsigned char dat)
{
    W5500_CS_LOW();
    W5500_SPI_Send_Short(reg);
    W5500_SPI_ReadWriteByte(FDM1 | RWB_WRITE | (s * 0x20 + 0x08));
    W5500_SPI_ReadWriteByte(dat);
    W5500_CS_HIGH();
}

/**
  * @brief  向指定的Socket寄存器写2字节数据
  */
void Write_W5500_SOCK_2Byte(SOCKET s, unsigned short reg, unsigned short dat)
{
    W5500_CS_LOW();
    W5500_SPI_Send_Short(reg);
    W5500_SPI_ReadWriteByte(FDM2 | RWB_WRITE | (s * 0x20 + 0x08));
    W5500_SPI_Send_Short(dat);
    W5500_CS_HIGH();
}

/**
  * @brief  向指定的Socket寄存器写4字节数据(如IP地址)
  */
void Write_W5500_SOCK_4Byte(SOCKET s, unsigned short reg, unsigned char *dat_ptr)
{
    W5500_CS_LOW();
    W5500_SPI_Send_Short(reg);
    W5500_SPI_ReadWriteByte(FDM4 | RWB_WRITE | (s * 0x20 + 0x08));
    W5500_SPI_ReadWriteByte(*dat_ptr++);
    W5500_SPI_ReadWriteByte(*dat_ptr++);
    W5500_SPI_ReadWriteByte(*dat_ptr++);
    W5500_SPI_ReadWriteByte(*dat_ptr++);
    W5500_CS_HIGH();
}

/**
  * @brief  读取W5500通用寄存器的1字节
  */
unsigned char Read_W5500_1Byte(unsigned short reg)
{
    unsigned char i;
    W5500_CS_LOW();
    W5500_SPI_Send_Short(reg);
    W5500_SPI_ReadWriteByte(FDM1 | RWB_READ | COMMON_R);
    i = W5500_SPI_ReadWriteByte(0x00); // 发送哑数据(0x00)来获取返回数据
    W5500_CS_HIGH();
    return i;
}

/**
  * @brief  读取指定Socket寄存器的2字节数据
  */
unsigned short Read_W5500_SOCK_2Byte(SOCKET s, unsigned short reg)
{
    unsigned short i;
    W5500_CS_LOW();
    W5500_SPI_Send_Short(reg);
    W5500_SPI_ReadWriteByte(FDM2 | RWB_READ | (s * 0x20 + 0x08));
    i = W5500_SPI_ReadWriteByte(0x00);
    i *= 256;
    i += W5500_SPI_ReadWriteByte(0x00);
    W5500_CS_HIGH();
    return i;
}

/**
  * @brief  W5500 核心网络参数初始化
  */
void W5500_Init(void)
{
    unsigned char i = 0;

    // 软件复位W5500
    Write_W5500_1Byte(MR, RST);
    HAL_Delay(10);

    // 配置核心网络参数
    Write_W5500_nByte(GAR, Gateway_IP, 4); // 网关
    Write_W5500_nByte(SUBR, Sub_Mask, 4);  // 掩码
    Write_W5500_nByte(SHAR, Phy_Addr, 6);  // MAC
    Write_W5500_nByte(SIPR, IP_Addr, 4);   // IP

    // 配置所有Socket的收发缓存大小 (均等分配为2KB)
    for(i = 0; i < 8; i++)
    {
        Write_W5500_SOCK_1Byte(i, Sn_RXBUF_SIZE, 0x02);
        Write_W5500_SOCK_1Byte(i, Sn_TXBUF_SIZE, 0x02);
    }

    // 设置重试时间: 2000(0x07D0) * 100us = 200ms
    Write_W5500_2Byte(RTR, 0x07d0);
    // 设置最大重发次数: 8次
    Write_W5500_1Byte(RCR, 8);
}

/**
  * @brief  初始化特定Socket的本地端口号
  */
void Socket_Init(SOCKET s)
{
    // 配置最大分片(不影响UDP，TCP用)
    Write_W5500_SOCK_2Byte(s, Sn_MSSR, 1460);

    if (s == 0)
    {
        // 设置Socket0的本地监听端口
        Write_W5500_SOCK_2Byte(0, Sn_PORT, S0_Port[0]*256 + S0_Port[1]);
    }
}

/**
  * @brief  将指定Socket配置为UDP模式并打开
  */
unsigned char Socket_UDP(SOCKET s)
{
    Write_W5500_SOCK_1Byte(s, Sn_MR, MR_UDP); // 模式配置为UDP
    Write_W5500_SOCK_1Byte(s, Sn_CR, OPEN);   // 下达打开指令
    HAL_Delay(5);

    // 验证状态寄存器是否成功进入UDP模式
    if(Read_W5500_1Byte(Sn_SR + (s * 0x100)) != SOCK_UDP)
    {
        Write_W5500_SOCK_1Byte(s, Sn_CR, CLOSE);
        return FALSE;
    }
    return TRUE;
}

/**
  * @brief  将待发送数据写入W5500 TX缓存并触发UDP发送/TCP发送
  * @param  s: 端口号 (0~7)
  * @param  dat_ptr: 数据指针
  * @param  size: 数据长度
  */
void Write_SOCK_Data_Buffer(SOCKET s, unsigned char *dat_ptr, unsigned short size)
{
    unsigned short offset, offset1;
    unsigned short i;

    // 配置UDP目标主机的IP和端口(使用UDP要开启下面两行、使用TCP就要注释掉)
   // Write_W5500_SOCK_4Byte(s, Sn_DIPR, S0_DIP);
    //Write_W5500_SOCK_2Byte(s, Sn_DPORTR, S0_DPort[0]*256 + S0_DPort[1]);

    // 读取TX写指针
    offset = Read_W5500_SOCK_2Byte(s, Sn_TX_WR);
    offset1 = offset;
    offset &= (S_TX_SIZE - 1); // 物理地址计算

    W5500_CS_LOW();
    W5500_SPI_Send_Short(offset);
    W5500_SPI_ReadWriteByte(VDM | RWB_WRITE | (s * 0x20 + 0x10));

    // 处理环形缓冲区回卷问题
    if((offset + size) < S_TX_SIZE)
    {
        for(i = 0; i < size; i++) { W5500_SPI_ReadWriteByte(*dat_ptr++); }
    }
    else
    {
        offset = S_TX_SIZE - offset;
        for(i = 0; i < offset; i++) { W5500_SPI_ReadWriteByte(*dat_ptr++); }

        W5500_CS_HIGH();
        W5500_CS_LOW();

        W5500_SPI_Send_Short(0x00);
        W5500_SPI_ReadWriteByte(VDM | RWB_WRITE | (s * 0x20 + 0x10));
        for(; i < size; i++) { W5500_SPI_ReadWriteByte(*dat_ptr++); }
    }
    W5500_CS_HIGH();

    // 更新指针并下达发送指令
    offset1 += size;
    Write_W5500_SOCK_2Byte(s, Sn_TX_WR, offset1);
    Write_W5500_SOCK_1Byte(s, Sn_CR, SEND); // 触发硬件发送
}

unsigned short Read_SOCK_Data_Buffer(SOCKET s, unsigned char *dat_ptr)
{
    unsigned short rx_size;
    unsigned short offset, offset1;
    unsigned short i;

    rx_size = Read_W5500_SOCK_2Byte(s, Sn_RX_RSR);
    if(rx_size == 0) return 0;
    if(rx_size > 1460) rx_size = 1460;

    offset = Read_W5500_SOCK_2Byte(s, Sn_RX_RD);
    offset1 = offset;
    offset &= (S_RX_SIZE - 1);

    W5500_CS_LOW(); // 使用你工程里的宏

    W5500_SPI_Send_Short(offset);
    W5500_SPI_ReadWriteByte(VDM | RWB_READ | (s * 0x20 + 0x18));

    if((offset + rx_size) < S_RX_SIZE)
    {
        for(i = 0; i < rx_size; i++) {
            *dat_ptr++ = W5500_SPI_ReadWriteByte(0x00);
        }
    }
    else
    {
        offset = S_RX_SIZE - offset;
        for(i = 0; i < offset; i++) { *dat_ptr++ = W5500_SPI_ReadWriteByte(0x00); }
        W5500_CS_HIGH();
        W5500_CS_LOW();
        W5500_SPI_Send_Short(0x00);
        W5500_SPI_ReadWriteByte(VDM | RWB_READ | (s * 0x20 + 0x18));
        for(; i < rx_size; i++) { *dat_ptr++ = W5500_SPI_ReadWriteByte(0x00); }
    }
    W5500_CS_HIGH();

    offset1 += rx_size;
    Write_W5500_SOCK_2Byte(s, Sn_RX_RD, offset1);
    Write_W5500_SOCK_1Byte(s, Sn_CR, RECV);
    return rx_size;
}
