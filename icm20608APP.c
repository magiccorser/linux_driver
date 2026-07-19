#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

// 定义数据结构，与内核驱动中的顺序一致
struct icm20608_data_t {
    int gyro_x;
    int gyro_y;
    int gyro_z;
    int accel_x;
    int accel_y;
    int accel_z;
    int temp;
};

static int running = 1;

void sig_handler(int sig)
{
    if (sig == SIGINT)
        running = 0;
}

int main(int argc, char *argv[])
{
    int fd;
    int ret;
    struct icm20608_data_t data;
    int count = 0;

    // 1. 注册 Ctrl+C 信号处理
    signal(SIGINT, sig_handler);

    // 2. 打开设备文件
    fd = open("/dev/icm20608", O_RDWR);
    if (fd < 0) {
        perror("open /dev/icm20608 failed");
        return -1;
    }
    printf("Open /dev/icm20608 success.\n");

    // 3. 循环读取并打印数据
    while (running) {
        // 一次性读取 28 字节（7 个 int）
        ret = read(fd, &data, sizeof(data));
        if (ret < 0) {
            perror("read error");
            break;
        } else if (ret == 0) {
            printf("EOF\n");
            break;
        } else if (ret != sizeof(data)) {
            printf("Read %d bytes, expected %ld\n", ret, sizeof(data));
            // 实际读到的字节数不足，可能驱动返回了部分数据
            // 如果只想读部分数据，可以自行处理
        }

        // 4. 打印数据（每隔 100 次清一次屏，方便观察变化）
        if (count % 100 == 0) {
            printf("\033[2J\033[H");  // 清屏
            printf("===== ICM20608 Sensor Data (count: %d) =====\n", count);
            printf("  Gyro  : X=%8d  Y=%8d  Z=%8d\n", 
                   data.gyro_x, data.gyro_y, data.gyro_z);
            printf("  Accel : X=%8d  Y=%8d  Z=%8d\n", 
                   data.accel_x, data.accel_y, data.accel_z);
            printf("  Temp  : %d\n", data.temp);
            printf("==========================================\n");
        } else {
            printf("  Gyro  : X=%8d  Y=%8d  Z=%8d\n", 
                   data.gyro_x, data.gyro_y, data.gyro_z);
        }

        count++;
        usleep(100000);  // 100ms 读取一次（10Hz）
    }

    // 5. 关闭设备
    close(fd);
    printf("\nExit.\n");
    return 0;
}