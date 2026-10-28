# 🏦 OOPBank Banking System

Hệ thống ngân hàng quản lý khách hàng, tài khoản, giao dịch, vay vốn, tín dụng, tiết kiệm và báo cáo hoạt động, được xây dựng bằng **C++** và **Qt 6**. Ứng dụng mang mục tiêu hỗ trợ quản lý ngân hàng theo mô hình khách hàng, nhân viên quầy giao dịch và quản trị viên.

## 📌 1. Tổng quan

OOPBank là một dự án phần mềm ngân hàng dạng desktop, tích hợp giao diện người dùng bằng Qt WebEngine và xử lý nghiệp vụ ở tầng backend C++/Qt. Hệ thống tập trung vào các chức năng chính như:

- 👥 Quản lý người dùng và phân quyền theo vai trò
- 💳 Quản lý tài khoản ngân hàng và số dư
- 💸 Giao dịch chuyển khoản, nạp/rút, tra cứu giao dịch
- 🏠 Vay vốn, tín dụng, tiết kiệm có kỳ hạn
- 🧾 Hóa đơn thanh toán và tính toán phí
- 🛡️ Ghi log kiểm toán và theo dõi hoạt động hệ thống

## ✨ 2. Tính năng chính

### 👤 2.1. Quản lý người dùng
- 🔐 Đăng nhập, xác thực PIN, khóa tài khoản sau nhiều lần sai
- 🎭 Phân quyền theo vai trò: `CUSTOMER`, `TELLER`, `ADMIN`
- 🔎 Tra cứu khách hàng, quản lý thông tin cá nhân và định danh

### 💳 2.2. Quản lý tài khoản
- ➕ Tạo và theo dõi tài khoản ngân hàng
- 🗂️ Hỗ trợ nhiều loại tài khoản: `CHECKING`, `SAVINGS`, `LOAN`, `CREDIT`
- 📊 Cập nhật số dư, trạng thái tài khoản, lịch sử giao dịch

### 💸 2.3. Giao dịch ngân hàng
- 🔁 Chuyển khoản nội bộ / ngoại vi
- ✅ Kiểm tra số dư trước khi thực hiện giao dịch
- 📅 Giới hạn giao dịch theo ngày
- 🧩 Xử lý giao dịch theo kiểu idempotent để tránh trùng lặp

### 🏦 2.4. Tín dụng và vay vốn
- 📝 Đăng ký hạn mức tín dụng
- 🔍 Xét duyệt hồ sơ tín dụng
- ⚙️ Hỗ trợ cấu hình lãi suất, kỳ hạn và phương thức thanh toán
- 📈 Theo dõi khoản vay và trạng thái giải ngân

### 🐷 2.5. Tiết kiệm và tài chính cá nhân
- 📖 Mở sổ tiết kiệm có kỳ hạn
- 💹 Kỳ hạn từ 1–3 tuần đến 60 tháng, lãi suất theo từng kỳ hạn từ 0,5% đến 6,5%/năm
- 📅 Quản lý ngày đáo hạn và tùy chọn tái tục
- 💰 Hỗ trợ rút tiền theo điều kiện hợp lệ

### 🛡️ 2.6. Hệ thống audit và bảo mật
- 📜 Ghi log mọi thao tác quan trọng
- 🗄️ Lưu trữ dữ liệu nhật ký hoạt động người dùng
- 🔒 Bảo mật bằng SHA-256, salt, PIN và kiểm soát quyền truy cập

## 🏗️ 3. Kiến trúc hệ thống

Dự án được tổ chức theo mô hình phân lớp rõ ràng:

- 🖥️ **UI layer**: giao diện ứng dụng desktop và web UI được tích hợp trong Qt WebEngine
- 🌉 **Bridge layer**: kết nối giữa tầng UI và nghiệp vụ
- ⚙️ **Service layer**: xử lý logic nghiệp vụ
- 🗃️ **Repository layer**: truy vấn và thao tác với database
- 📦 **Model layer**: các đối tượng nghiệp vụ và entity quan trọng
- 🧰 **Core layer**: cấu hình database, bảo mật, session, hỗ trợ kỹ thuật

## 🧰 4. Công nghệ sử dụng

- 💻 Ngôn ngữ: C++17
- 🎨 Framework UI: Qt 6
- 🧩 Components: Qt Widgets, Qt WebEngine, Qt WebChannel, Qt Network, Qt Concurrent
- 🗄️ Database: libSQL / Turso
- 🔨 Quản lý build: CMake
- 🌐 Giao diện web: HTML/JavaScript trong thư mục `docs/`
- 🔐 Bảo mật: SHA-256, salt, session token, kiểm soát vai trò

## 📂 5. Cấu trúc thư mục

```text
OOPBANK/
├── CMakeLists.txt
├── README.md
├── schema.sql
├── build/
├── docs/
│   └── app/
├── include/
│   ├── bridge/
│   ├── core/
│   ├── dtos/
│   ├── model/
│   ├── repository/
│   └── services/
├── src/
│   ├── bridge/
│   ├── core/
│   ├── repository/
│   ├── service/
│   └── main.cpp
└── schema.sql
```

## ⚙️ 6. Yêu cầu môi trường

### 📦 6.1. Phần mềm cần thiết
- CMake 3.21+
- Qt 6.5+
- MSVC 64-bit trên Windows (khuyến nghị cho build Qt WebEngine)
- Visual Studio 2022 hoặc môi trường tương thích

### 🔧 6.2. Biến môi trường
- `QT_ROOT_DIR` hoặc `Qt6_DIR`
- CMake cần biết đúng đường dẫn tới bộ Qt đã cài đặt

Ví dụ:

```powershell
cmake -S . -B build -DOOPBANK_QT_ROOT=C:/Qt/6.12.0/msvc2022_64
```

## 🚀 7. Hướng dẫn build

### 📥 Bước 1: Cài đặt dependency
Đảm bảo môi trường đã có:
- Qt 6 với WebEngineWidgets và WebChannel
- CMake
- Compiler C++ tương thích (MSVC trên Windows)

### 🛠️ Bước 2: Generate build files

```powershell
cmake -S . -B build -DOOPBANK_QT_ROOT="C:/Qt/6.x.x/msvc2022_64"
```

### 🔨 Bước 3: Build dự án

```powershell
cmake --build build --config Release
```

### ▶️ Bước 4: Chạy ứng dụng
Sau khi build xong, chạy file thực thi tương ứng:

```powershell
./build/Release/OOPBank-Desktop.exe
```

Hoặc trên Windows nếu build trong môi trường Visual Studio, chạy file exe đã được sinh ra trong thư mục build/Release hoặc build/Debug.

## 🗄️ 8. Thiết lập database

Dự án sử dụng file `schema.sql` để khởi tạo cấu trúc cơ sở dữ liệu. File này chứa các bảng chính như:

- `users`
- `accounts`
- `transactions`
- `audit_logs`
- `system_config`
- `credit_applications`
- `credit_lines`
- `bills`
- `savings_accounts`
- `loans`
- `notifications`
- `idempotency_keys`

> ⚠️ **Lưu ý:** file này phù hợp cho database mới hoặc môi trường phát triển ban đầu. Đối với môi trường production nên sử dụng migration quản lý phiên bản rõ ràng.

Với database đã khởi tạo trước khi hỗ trợ kỳ hạn tiết kiệm theo tuần, áp dụng một lần `migrations/20261026_savings_term_days.sql` trước khi chạy phiên bản mới.

## 🔄 9. Quy trình sử dụng hệ thống

### 🎭 9.1. Vai trò người dùng
- 🙋 **Customer**: đăng nhập, xem tài khoản, chuyển khoản, gửi yêu cầu vay / tín dụng, quản lý tiết kiệm
- 🧑‍💼 **Teller**: hỗ trợ khách hàng, tra cứu thông tin, thực hiện giao dịch theo quy định
- 👨‍💻 **Admin**: quản trị tài khoản, cấu hình hệ thống, duyệt tín dụng và khoản vay lớn

### 🔁 9.2. Luồng nghiệp vụ điển hình
1. 🔑 Khách hàng đăng nhập
2. 🪪 Hệ thống xác thực thông tin và cấp session
3. 👆 Người dùng chọn chức năng cần thực hiện
4. 🧮 Hệ thống kiểm tra quyền, ràng buộc nghiệp vụ và số dư
5. ✅ Giao dịch được thực hiện và ghi log vào audit

## 🔐 10. Bảo mật và định nghĩa hệ thống

Hệ thống được thiết kế với một số biện pháp an ninh cơ bản:

- 🔑 Mật khẩu được hash theo thuật toán SHA-256 kèm salt
- 🔢 PIN riêng tư được lưu trữ với cơ chế phân tách và kiểm soát
- 🧩 Giao dịch có kiểm tra idempotency để tránh lặp hành vi
- 📜 Ghi nhật ký hành động audit cho mọi thao tác quan trọng
- 📅 Hạn mức giao dịch theo ngày để kiểm soát rủi ro

## 💡 11. Mẹo phát triển

- 🔄 Luôn giữ `schema.sql` đồng bộ với cấu trúc dữ liệu khi bổ sung bảng mới
- 🧱 Khi thêm module nghiệp vụ mới, cần cập nhật đồng bộ ở 3 lớp: model, repository, service
- 🔧 Nếu cần build lại với Qt mới, kiểm tra lại `QT_ROOT_DIR` hoặc `Qt6_DIR`
- 🪟 Nên chạy app trong môi trường Windows với toolchain MSVC để tránh lỗi WebEngine trên MinGW

## 🗺️ 12. Roadmap đề xuất

- 🔐 Tích hợp xác thực OAuth / JWT
- 📊 Thêm báo cáo thống kê tài chính theo ngày/tháng
- 🏢 Mở rộng module khách hàng doanh nghiệp
- 📧 Tích hợp email/SMS notification
- 🗃️ Thêm quản lý migration database chuyên nghiệp
- 📱 Chuyển một phần giao diện sang web responsive cho trải nghiệm đa nền tảng

## 📜 13. License

Dự án hiện chưa khai báo giấy phép cụ thể. Nếu cần publish hoặc chia sẻ công khai, nên bổ sung file LICENSE rõ ràng phù hợp với quy định của tổ chức.

## 🤝 14. Liên hệ / đóng góp

Nếu bạn đang phát triển hoặc nâng cấp hệ thống, hãy cập nhật phần này bằng thông tin của team, địa chỉ repo, email hỗ trợ hoặc hướng dẫn triển khai production.

## 👨‍💻 15. Tác giả

Dự án được thực hiện bởi nhóm sinh viên:

| STT | 🧑‍🎓 Họ và tên | 🆔 MSSV | 🏫 Lớp |
|:---:|---|:---:|:---:|
| 1 | Huỳnh Văn Lĩnh | 6651071039 | CQ.66.CNTT |
| 2 | Phạm Đức Cường | 6651071007 | CQ.66.CNTT |
| 3 | Trần Ninh Thuận | 6651071073 | CQ.66.CNTT |
| 4 | Phan Thế Hiệp | 6651071023 | CQ.66.CNTT |
| 5 | Nguyễn Đào Anh Hào | 6651071018 | CQ.66.CNTT |