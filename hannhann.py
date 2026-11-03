from __future__ import annotations
import os
import cv2
import numpy as np
import pandas as pd
from datetime import datetime, time
from deepface import DeepFace
# ================== CONFIG ==================
DATASET_DIR = "dataset"
ATTENDANCE_FILE = os.path.join(os.getcwd(), "attendance.xlsx")
MODEL_NAME = "ArcFace"
DISTANCE_THRESHOLD = 0.45
# Khung giờ điểm danh
START_TIME = time(11, 00, 0)
END_TIME = time(23, 50, 0)
# Danh sách sinh viên (lấy từ folder dataset)
students = []  # list dict: {"folder":..., "name":..., "mssv":..., "status":...}
checked_list = []


# ================== INIT EXCEL ==================
def init_excel():
    if not os.path.exists(ATTENDANCE_FILE):
        df = pd.DataFrame(columns=["mssv", "name", "folder", "date", "time", "status", "note"])
        df.to_excel(ATTENDANCE_FILE, index=False)


# ================== LOAD STUDENTS FROM DATASET ==================
def load_students():
    global students
    students = []

    if not os.path.exists(DATASET_DIR):
        print("❌ Không tìm thấy folder dataset!")
        return

    for folder_name in os.listdir(DATASET_DIR):
        folder_path = os.path.join(DATASET_DIR, folder_name)

        if not os.path.isdir(folder_path):
            continue

        parts = folder_name.split("_", 1)
        name = parts[0]
        mssv = parts[1] if len(parts) > 1 else folder_name

        students.append({
            "folder": folder_name,
            "name": name,
            "mssv": mssv,
            "status": "Not Checked"
        })


# ================== CHECK TIME IN RANGE ==================
def in_attendance_time() -> bool:
    now = datetime.now().time()
    return START_TIME <= now <= END_TIME


# ================== CHECK ALREADY CHECKED ==================
def already_checked_in(mssv: str, date_str: str) -> bool:
    df = pd.read_excel(ATTENDANCE_FILE)

    existed = (
        (df["mssv"].astype(str) == str(mssv)) &
        (df["date"].astype(str) == date_str)
    ).any()

    return existed


# ================== SAVE ATTENDANCE ==================
def save_attendance(mssv: str, name: str, folder: str, status: str, note: str):
    now = datetime.now()
    date_str = now.strftime("%Y-%m-%d")
    time_str = now.strftime("%H:%M:%S")

    # nếu đã có record hôm nay thì không ghi thêm
    if already_checked_in(mssv, date_str):
        return False, time_str

    df = pd.read_excel(ATTENDANCE_FILE)

    new_row = {
        "mssv": mssv,
        "name": name,
        "folder": folder,
        "date": date_str,
        "time": time_str,
        "status": status,
        "note": note
    }

    df = pd.concat([df, pd.DataFrame([new_row])], ignore_index=True)
    df.to_excel(ATTENDANCE_FILE, index=False)

    return True, time_str


# ================== ADD CHECKED LIST ==================
def add_checked_list(name: str, mssv: str, time_str: str):
    info = f"{name} - {mssv} - {time_str}"

    for item in checked_list:
        if f"- {mssv} -" in item:
            return

    checked_list.append(info)


# ================== UPDATE STUDENT STATUS ==================
def update_student_status(mssv: str, status: str):
    for st in students:
        if str(st["mssv"]) == str(mssv):
            st["status"] = status
            return


# ================== RECOGNIZE FACE ==================
def recognize_face(frame_path: str):
    try:
        results = DeepFace.find(
            img_path=frame_path,
            db_path=DATASET_DIR,
            model_name=MODEL_NAME,
            enforce_detection=True
        )

        if len(results) == 0:
            return None, None

        df = results[0]
        if df.empty:
            return None, None

        best_match_path = df.iloc[0]["identity"]
        best_distance = df.iloc[0]["distance"]

        folder_name = os.path.basename(os.path.dirname(best_match_path))

        if best_distance > DISTANCE_THRESHOLD:
            return None, best_distance

        return folder_name, best_distance

    except Exception:
        return None, None
# ================== MARK ABSENT AFTER END TIME ==================
def mark_absent_students():
    """
    Hết 08:00 tự động ghi absent cho những người chưa điểm danh.
    """
    now = datetime.now()
    date_str = now.strftime("%Y-%m-%d")

    for st in students:
        if st["status"] != "Present":
            st["status"] = "Absent"

            # lưu absent vào excel
            save_attendance(
                mssv=st["mssv"],
                name=st["name"],
                folder=st["folder"],
                status="Absent",
                note="Vắng"
            )


# ================== MAIN ==================
def main():
    init_excel()
    load_students()

    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("❌ Không mở được camera.")
        return

    status_text = "Ready"
    last_name = ""
    last_mssv = ""
    last_time = ""

    print("📌 Nhấn SPACE để quét điểm danh")
    print("📌 Nhấn Q để thoát")
    not_recognized_time = 0
    while True:
        now_time = datetime.now().time()

        # nếu hết giờ -> mark absent + đóng chương trình
        if now_time > END_TIME:
            print("⏰ Hết giờ điểm danh. Đang đánh vắng...")
            mark_absent_students()
            print("✅ Đã ghi absent. Tự động đóng chương trình.")
            break

        ret, frame = cap.read()
        if not ret:
            break
        # ================== PANEL RIGHT ==================
        panel_width = 480
        h, w, _ = frame.shape

        new_frame = np.zeros((h, w + panel_width, 3), dtype=np.uint8)
        new_frame[:, :w] = frame

        # panel background
        cv2.rectangle(new_frame, (w, 0), (w + panel_width, h), (40, 40, 40), -1)

        # title
        cv2.putText(new_frame, "ATTENDANCE LIST", (w + 20, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 0), 2)

        # time range display
        time_range = f"Time: {START_TIME.strftime('%H:%M')} - {END_TIME.strftime('%H:%M')}"
        cv2.putText(new_frame, time_range, (w + 20, 75),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)

        # show students status
        y_start = 120
        line_height = 30

        for i, st in enumerate(students[:20]):  # chỉ show 20 dòng (tránh tràn)
            y = y_start + i * line_height

            text = f"{st['name']} - {st['mssv']} - {st['status']}"
            if st["status"] == "Present":
                color = (0, 255, 0)   # xanh
            elif st["status"] == "Absent":
                color = (0, 0, 255)   # đỏ
            else:
                color = (255, 255, 255)  # trắng

            cv2.putText(new_frame, text, (w + 20, y),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)

        frame = new_frame

        # ================== LEFT SIDE TEXT ==================
        cv2.putText(frame, "SPACE: Scan | Q: Quit", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)

        # show status
        cv2.putText(frame, f"Status: {status_text}", (10, 70),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        # Nếu không nhận diện được -> hiển thị cảnh báo đỏ giữa màn hình
        if status_text == "Not Recognized":
            cv2.putText(frame, "KHONG NHAN DIEN DUOC!", (10, 140),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

        # show current time
        cv2.putText(frame, f"Now: {datetime.now().strftime('%H:%M:%S')}", (10, 110),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)

        if last_name != "":
            cv2.putText(frame, f"Name: {last_name}", (10, 170),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

        if last_mssv != "":
            cv2.putText(frame, f"MSSV: {last_mssv}", (10, 210),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

        if last_time != "":
            cv2.putText(frame, f"Time: {last_time}", (10, 250),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

        cv2.imshow("Face Attendance System", frame)

        key = cv2.waitKey(1) & 0xFF

        # quit
        if key == ord("q"):
            print("⚠️ Thoát sớm - ghi danh sách vắng...")
            mark_absent_students()
            break
        # scan
        if key == 32:
            if not in_attendance_time():
                status_text = "Out of time range!"
                continue

            status_text = "Scanning..."
            cv2.imshow("Face Attendance System", frame)
            cv2.waitKey(1)

            temp_path = "temp_capture.jpg"
            cv2.imwrite(temp_path, frame[:, :w])  # lưu phần camera thôi

            folder_name, distance = recognize_face(temp_path)

            if folder_name is None:
                status_text = "Not Recognized"
                last_name = ""
                last_mssv = ""
                last_time = ""

            else:
                parts = folder_name.split("_", 1)
                name = parts[0]
                mssv = parts[1] if len(parts) > 1 else folder_name

                ok, time_str = save_attendance(
                    mssv=mssv,
                    name=name,
                    folder=folder_name,
                    status="Present",
                    note="Có mặt"
                )

                last_name = name
                last_mssv = mssv
                last_time = time_str

                update_student_status(mssv, "Present")
                add_checked_list(name, mssv, time_str)

                if ok:
                    status_text = "CHECKED-IN SUCCESS"
                else:
                    status_text = "ALREADY CHECKED TODAY"

            if os.path.exists(temp_path):
                os.remove(temp_path)

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
