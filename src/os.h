// os.h
#ifndef OS_H
#define OS_H

#include <stdint.h>

// ========================================================
// 1. THÀNH PHẦN QUẢN LÝ TIẾN TRÌNH (THREAD MANAGER)
// ========================================================

/*Ta tạo tập hơn trạng thái của vòng đời tác vụ
TASK_READY: có thể chạy nhưng đang đợi CPU
TASK_RUNNING: Đang trực tiếp chiếm giữ lõi xử lí
TASK_BLOCKED: Tạm dừng để chờ đợi 1 sự kiện*/

enum TaskState 
{
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED
};

struct TCB_t 
{
    uint32_t *stackPtr;  // BẮT BUỘC: Luôn ở đầu struct để có offset = 0 trong Assembly
    TaskState state;
    uint32_t taskId;
    uint32_t timeout;//Biến đếm ngược thời gian ngủ (ms)
};


/**
 * brief Hàm tạo mới và cấp phát tài nguyên cho một tác vụ
 * param id: ID số nguyên để định danh Task
 * param task_function: Con trỏ hàm trỏ tới logic chạy của Task (vòng lặp while(1))
 */
void OS_TaskCreate(uint32_t id, void (*task_function)(void));


// ========================================================
// 2. THÀNH PHẦN ĐIỀU PHỐI & LẬP LỊCH (SCHEDULER / DISPATCHER)
// ========================================================

/**
 * brief Hàm kích hoạt Hệ điều hành và cấu hình định thời hệ thống
 * param ticks_per_ms: Số chu kỳ xung nhịp tương ứng với 1 mili-giây
 */
void OS_Start(uint32_t ticks_per_ms);

//Hàm Delay
//ms: thời gian delay
void OS_Delay(uint32_t ms);

//Idle Task
void OS_IdleTask(void);
// Chỉ định liên kết C (extern "C") cho các hàm ngắt phần cứng 
// để trình biên dịch C++ không làm thay đổi tên hàm (Name Mangling),
// giúp CPU ARM có thể ánh xạ đúng địa chỉ khi xảy ra ngắt.
extern "C" 
{
    void SysTick_Handler(void);   // Ngắt định thời 1ms của Bộ lập lịch (Scheduler)
    void PendSV_Handler(void);    // Ngắt xử lý Đổi ngữ cảnh vật lý (Dispatcher)
    void OS_UpdateCurrentTCB(void);// Cập nhật Task tiếp theo

    extern TCB_t* current_TCB;
}

#endif // OS_H