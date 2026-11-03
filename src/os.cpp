// os.cpp
#include "os.h"
#include <stdio.h>

#define MAX_TASKS   3
#define STACK_SIZE  128  // 128 * 4 byte = 512 bytes

// Các thanh ghi SysTick
#define SYS_TICK_CTRL    (*((volatile uint32_t*)0xE000E010)) // SysTick->Ctrl; Điều khiển SysTick Timer
#define SYS_TICK_LOAD    (*((volatile uint32_t*)0xE000E014)) // SysTick->Load; để điền giá trị của Timer
#define INT_CTRL         (*((volatile uint32_t*)0xE000ED04)) // ICSR

//Giá trị cho thanh ghi SysTick
#define PENDSVSET        (1 << 28)  // Kích hoạt PendSV
#define SYSTICK_ENABLE   (1 << 0)   // Bật SysTick
#define SYSTICK_INT      (1 << 1)   // Bật ngắt SysTick
#define SYSTICK_CLKSOURCE (1 << 2)  // Dùng xung lõi CPU

#define NVIC_SHPR3       (*((volatile uint32_t*)0xE000ED20))//System Handler Priority 3


//Tài nguyên của CPU
TCB_t TCB_Table[MAX_TASKS];      // Ready queue xếp Task theo thứ tự
TCB_t* current_TCB = nullptr;    // Ptr cho Ready queue

uint32_t Task_Stacks[MAX_TASKS][STACK_SIZE]; // Stack RAM cho các task
uint32_t task_count = 0;                     // Index cho Task Stacks

int currentTaskIndex = 0; // Biến quản lý chỉ số Task

volatile uint32_t ms_ticks = 0;//Biến đếm mili-giây toàn cục của hệ thống

// Hàm tạo Task để xếp vào Ready queue
void OS_TaskCreate(uint32_t id, void (*task_function)(void)) 
{
    if (task_count >= MAX_TASKS) return;

    // Khởi tạo TCB
    TCB_t* tcb = &TCB_Table[task_count];
    tcb->taskId = id;
    tcb->state = TASK_READY;
    tcb->timeout = 0;
    printf("Init Task Done in OS_TaskCreate()\n");

    // Đỉnh stack xuất phát từ biên mảng (Cortex-M3 Full Descending)
    uint32_t* top_of_stack = &Task_Stacks[task_count][STACK_SIZE];

    // Hardware Stack Frame
    top_of_stack--; *top_of_stack = 0x01000000; // xPSR
    top_of_stack--; *top_of_stack = (reinterpret_cast<uint32_t>(task_function) | (0x01)); // PC
    top_of_stack--; *top_of_stack = 0xFFFFFFFD; // LR

    top_of_stack--; *top_of_stack = 0x12121212; // R12
    top_of_stack--; *top_of_stack = 0x03030303; // R3
    top_of_stack--; *top_of_stack = 0x02020202; // R2
    top_of_stack--; *top_of_stack = 0x01010101; // R1
    top_of_stack--; *top_of_stack = 0x00000000; // R0

    // Software Stack Frame
    top_of_stack--; *top_of_stack = 0x11111111; // R11
    top_of_stack--; *top_of_stack = 0x10101010; // R10
    top_of_stack--; *top_of_stack = 0x09090909; // R9
    top_of_stack--; *top_of_stack = 0x08080808; // R8
    top_of_stack--; *top_of_stack = 0x07070707; // R7
    top_of_stack--; *top_of_stack = 0x06060606; // R6
    top_of_stack--; *top_of_stack = 0x05050505; // R5
    top_of_stack--; *top_of_stack = 0x04040404; // R4

    tcb->stackPtr = top_of_stack;
    task_count++;
}

//Bộ lập lịch được cập nhật để giải phóng CPU khi tất cả cùng ngủ
extern "C" void OS_UpdateCurrentTCB() 
{
    //Chuyển Task cũ về READY nếu nó thực sự đang chạy bình thường
    if (current_TCB->state == TASK_RUNNING) 
    {
        current_TCB->state = TASK_READY;
    }

    int previousTaskIndex = currentTaskIndex;
    int searchCount = 0;
    bool foundReadyTask = false;

    //Tìm Task READY Task tiếp theo
    do {
        currentTaskIndex = (currentTaskIndex + 1) % task_count;//Chuyển sang index tiếp theo
        searchCount++;//
        if (TCB_Table[currentTaskIndex].state == TASK_READY)//Nếu thấy có READY Task
        {
            foundReadyTask = true;//Đã tìm thấy
            break;//Hủy vòng lặp
        }
    } while (searchCount < task_count);//Chừng nào search < task_count

    //Trao quyền CPU
    if (foundReadyTask)//Nếu tìm thấy
    {
        current_TCB = &TCB_Table[currentTaskIndex];
        current_TCB->state = TASK_RUNNING;
    }
}

void OS_Start(uint32_t ticks_per_ms) 
{

    OS_TaskCreate(99, OS_IdleTask);//Đặt Idle Task ở vị trí cuối cùng

    NVIC_SHPR3 |= 0xFFFF0000; // Hạ quyền ưu tiên của PendSV xuống thấp nhất

    SYS_TICK_LOAD = ticks_per_ms - 1; //Giá trị cho SysTick 
    SYS_TICK_CTRL = SYSTICK_ENABLE | SYSTICK_INT | SYSTICK_CLKSOURCE;//Kích hoạt SysTick

    currentTaskIndex = 0;
    current_TCB = &TCB_Table[0];
    current_TCB->state = TASK_RUNNING; 
    printf("Start Task 0 in OS_Start()\n");

    // Mồi kích hoạt Task đầu tiên
    __asm volatile 
    (
        "LDR R0, =current_TCB \n"
        "LDR R0, [R0]         \n"
        "LDR R0, [R0]         \n" 
        "LDMIA R0!, {R4-R11}  \n" 
        "MSR PSP, R0          \n"
        "MOV R1, #2           \n" 
        "MSR CONTROL, R1      \n" 
        "ISB                  \n" 
        "MOV LR, #0xFFFFFFFD  \n" 
        "BX LR                \n" 
    );
}

//Hàm OS_Delay chặn dựa trên nhịp Timers chuẩn và an toàn
void OS_Delay(uint32_t ms)
{
    __asm volatile("CPSID i"); // Khóa ngắt bảo vệ vùng găng
    if (current_TCB != nullptr) 
    {
        current_TCB->timeout = ms_ticks + ms; 
        current_TCB->state = TASK_BLOCKED; 
    }
    __asm volatile("CPSIE i"); // Mở lại ngắt

    // Vòng lặp chặn an toàn dựa vào trạng thái do SysTick giải phóng
    while (*((volatile uint32_t*)&current_TCB->state) == TASK_BLOCKED)
    {
        __asm volatile("NOP"); //Không làm gì
    }
}

void OS_IdleTask(void)
{
    while(1)
    {
        __asm volatile("NOP");
    }
}

//Short-time Scheduler
//Định thời hệ thống tự động kiểm tra và đánh thức các Task ngủ
extern "C" void SysTick_Handler(void) 
{
    ms_ticks++; // Tăng nhịp thời gian hệ thống
    
    // Duyệt mảng để đánh thức Task
    for (uint32_t i = 0; i < task_count; i++) 
    {
        if (TCB_Table[i].state == TASK_BLOCKED) 
        {
            if (ms_ticks >= TCB_Table[i].timeout) 
            {
                TCB_Table[i].state = TASK_READY; // Đánh thức dậy hợp lệ
            }
        }
    }
    
    INT_CTRL = PENDSVSET; // Kích hoạt đổi Task luân phiên
}

extern void uart_send_char(char c);

//Tạo hàm printf() để debugging
extern "C" 
{
    int _write(int file, char *ptr, int len) 
    {
        for (int i = 0; i < len; i++) 
        {
            uart_send_char(ptr[i]);
        }
        return len;
    }
}