
# Course Registration Portal (Academia)

## Overview
This project is a socket-based, multi-user Course Registration Portal. It provides role-based access for **Admin**, **Faculty**, and **Student** users, enabling functionalities like course management, enrollment, and user management. The system ensures secure login, data consistency, and concurrent client handling using file management, file locking, and semaphores.

---

## Features
### Admin
- Secure login  
- Add Student / Faculty  
- Activate / Deactivate Student  
- Update Student / Faculty details  

### Faculty
- Secure login  
- Add new course (with seat limits)  
- Remove course  
- View enrollments  
- Change password  

### Student
- Secure login  
- Enroll in a course (if seats are available)  
- Unenroll from a course  
- View enrolled courses  
- Change password  

---

## Technical Details
- **Architecture:** Client-Server model  
- **Communication:** TCP Sockets (`bind()`, `listen()`, `accept()`)  
- **Concurrency:** `fork()` for handling multiple clients  
- **File Management:**  
  - `student.txt` → `id name password status`  
  - `faculty.txt` → `id name password`  
  - `course.txt` → `id name faculty`  
  - `enrollment.txt` → `course_id student_id`  
- **File Locking:** `fcntl()` for read/write locks  
- **Synchronization:** Semaphores for critical sections  
- **Language:** C  

---

## How It Works
- **Server:**
  - Maintains all data files
  - Handles multiple client connections
  - Uses semaphores and file locks to avoid data corruption
- **Client:**
  - Connects to the server via TCP
  - Sends credentials and receives role-based menus

---

## Steps to Run
```bash
# 1. Compile the code
make all

# 2. Run the server
./server

# 3. Open another terminal and run the client
./client
```

---

## Requirements
- GCC Compiler  
- Linux Environment  
- Basic C Socket Programming Knowledge  

---
