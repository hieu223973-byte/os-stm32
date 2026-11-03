// main.cpp
#include "os.h"
#include <stdio.h>

// Định nghĩa địa chỉ thanh ghi GPIO Port C của STM32F103
#define RCC_APB2ENR      (*((volatile uint32_t*)0x40021018))

// Giả lập tần số thạch anh STM32F103C8 chạy ở 72 MHz
// 72000000 / 1000 = 72000 tick hệ thống tương đương 1ms
#define SYSTEM_CLOCK_1MS  72000

// Định nghĩa các thanh ghi USART1 chính xác của bạn
#define USART1_SR     (*(volatile uint32_t*)0x40013800) // Status Register
#define USART1_DR     (*(volatile uint32_t*)0x40013804) // Data register
#define USART1_BRR    (*(volatile uint32_t*)0x40013808) // Baudrate
#define USART1_CR1    (*(volatile uint32_t*)0x4001380C) // Control Register


void uart_init(void) 
{
    // 1. Cấp xung Clock cho USART1 (Bit 14 trong thanh ghi RCC_APB2ENR)
    RCC_APB2ENR |= (1 << 14);

    // 2. Cấu hình Baudrate (Ví dụ: Giả lập không quá khắt khe, nhưng nên cho giá trị hợp lệ)
    USART1_BRR = 0x341; // Cấu hình tạm cho 115200 baud ở clock 72MHz

    // 3. BẬT BỘ PHÁT VÀ BẬT USART1 (QUAN TRỌNG NHẤT ĐỂ SỬA LỖI TRÊN)
    // Bit 3 (TE): Transmitter Enable (Bật bộ phát)
    // Bit 13 (UE): USART Enable (Bật toàn bộ khối ngoại vi USART)
    USART1_CR1 = 0;
    USART1_CR1 |= (1 << 3) | (1 << 13);
}

// Hàm gửi 1 ký tự chính chủ của bạn
void uart_send_char(char c) 
{
    while (!(USART1_SR & (1 << 7))); // Đợi Bit 7 (TXE) lên 1 - Bộ đệm trống
    USART1_DR = c;                   // Ghi dữ liệu vào thanh ghi DR
}

// Hàm gửi chuỗi chính chủ của bạn
void uart_send_string(const char* str) 
{
    while (*str) 
    {
        uart_send_char(*str++);
    }
}

void Task1_Handler(void) 
{
    while(1) 
    {
        uart_send_string("-> [Task 1] Đang thực thi và lật chân LED 13...\n");

        OS_Delay(200);//Delay 200 ms
    }
}

void Task2_Handler(void) 
{
    while(1) 
    {
        uart_send_string("<= [Task 2] Đang thực thi và lật chân LED 14...\n");
        
        OS_Delay(500);//Delay 500 ms
    }
}

int main(void) 
{
    uart_init();

    OS_TaskCreate(1, Task1_Handler);
    OS_TaskCreate(2, Task2_Handler);

    // 3. Kích hoạt hạt nhân OS, phân chia thời gian!
    OS_Start(SYSTEM_CLOCK_1MS);

    while(1) 
    {
        // Code ở đây sẽ KHÔNG BAO GIỜ được chạy tới, 
        // vì CPU đã bị OS chiếm quyền điều khiển và luân chuyển giữa Task 1 và Task 2!
    }
}