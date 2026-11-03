.syntax unified
.thumb

.global PendSV_Handler
.extern current_TCB
.extern OS_UpdateCurrentTCB

.section .text

.thumb_func
PendSV_Handler:
    CPSID   I               @ 1. Khóa tất cả các ngắt để đảm bảo an toàn (Khu vực găng)
    
    @ --- LƯU NGỮ CẢNH TASK CŨ ---
    MRS     R0, PSP         @ Đọc con trỏ Stack của Task đang chạy (PSP) vào thanh ghi R0
    STMDB   R0!, {R4-R11}   @ Đẩy các thanh ghi R4 đến R11 vào Stack, R0 tự động giảm địa chỉ (Store Multiple Decrement Before)
    
    LDR     R1, =current_TCB @ Lấy địa chỉ của con trỏ current_TCB cất vào R1
    LDR     R1, [R1]        @ R1 = *current_TCB (Bây giờ R1 chứa địa chỉ của struct TCB)
    STR     R0, [R1]        @ Lưu giá trị R0 (đỉnh Stack mới sau khi cất R4-R11) vào ô nhớ đầu tiên của TCB (chính là biến stackPtr)

    @ --- GỌI BỘ LẬP LỊCH C++ ĐỂ ĐỔI TASK ---
    PUSH    {LR}            @ Lưu thanh ghi Link Register (đang giữ mã thoát ngắt) vào Main Stack
    BL      OS_UpdateCurrentTCB @ Gọi hàm C++ để đổi current_TCB sang Task mới
    POP     {LR}            @ Khôi phục lại mã thoát ngắt trả về cho LR

    @ --- KHÔI PHỤC NGỮ CẢNH TASK MỚI ---
    LDR     R1, =current_TCB @ Lấy địa chỉ của con trỏ current_TCB (lúc này đã trỏ sang Task mới)
    LDR     R1, [R1]        
    LDR     R0, [R1]        @ R0 = current_TCB->stackPtr (Lấy đỉnh Stack của Task mới ra)
    
    LDMIA   R0!, {R4-R11}   @ Bốc ngược dữ liệu từ Stack nạp vào các thanh ghi R4-R11 (Load Multiple Increment After)
    MSR     PSP, R0         @ Cập nhật lại con trỏ PSP vật lý của CPU bằng giá trị R0 mới
    
    CPSIE   I               @ 2. Mở lại các ngắt hệ thống
    BX      LR              @ 3. Thoát ngắt! CPU thấy LR = 0xFFFFFFFD sẽ tự động khôi phục nốt phần cứng và chạy Task mới!
    