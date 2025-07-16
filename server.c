#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <semaphore.h>
#include <linux/stat.h>
#include <sys/stat.h>

#define PORT 8080
#define MAX 1024

sem_t sem_admin;
sem_t sem_student;
sem_t sem_faculty;
sem_t sem_course;
sem_t sem_enrollment;

void handleClient(int connfd);

void adminMenu(int connfd);
void studentMenu(int connfd, char* username);
void facultyMenu(int connfd, char* username);
int authenticateUser(char *username, char *password);

void addStudentToFile(int connfd);
void addFacultyToFile(int connfd);
void toggleStudent(int connfd);
void updateDetails(int connfd);

void addCourseToFile(int connfd, char* username);
void removeCourse(int connfd, char* username);
void viewEnrollments(int connfd);
void facultyChangePassword(int connfd, char* username);

void enrollCourse(int connfd, char* username);
void unenrollCourse(int connfd, char* username);
void viewCourses(int connfd, char* username);
void changeStudentPassword(int connfd, char* username);

int courseLimit[1000] = {0};

int main() {
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket failed");
        exit(1);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    listen(sockfd, 5);
    socklen_t len = sizeof(cli);

    printf("Server listening on port %d...\n", PORT);

    // Initialize semaphores
    sem_init(&sem_admin, 0, 3);
    sem_init(&sem_student, 0, 3);
    sem_init(&sem_faculty, 0, 3);
    sem_init(&sem_course, 0, 3);
    sem_init(&sem_enrollment, 0, 3);

    while (1) {
        connfd = accept(sockfd, (struct sockaddr*)&cli, &len);
        if (connfd < 0) {
            perror("accept failed");
            exit(1);
        }

        int flag = 1;
        setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

        if (fork() == 0) {
            close(sockfd);
            handleClient(connfd);
            exit(0);
        } else {
            close(connfd);
        }
    }

    // Cleanup semaphores
    sem_destroy(&sem_admin);
    sem_destroy(&sem_student);
    sem_destroy(&sem_faculty);
    sem_destroy(&sem_course);
    sem_destroy(&sem_enrollment);

    return 0;
}

void handleClient(int connfd) {
    char username[50], password[50];

    // Login
    write(connfd, "Username: ", strlen("Username: "));
    read(connfd, username, sizeof(username));
    username[strcspn(username, "\n")] = 0;

    write(connfd, "Password: ", strlen("Password: "));
    read(connfd, password, sizeof(password));
    password[strcspn(password, "\n")] = 0;

    int role = authenticateUser(username, password);
    if (role == 1) {
        adminMenu(connfd);
    } else if (role == 2) {
        studentMenu(connfd, username);
    } else if (role == 3) {
        facultyMenu(connfd, username);
    } else {
        write(connfd, "Invalid Credentials\n", 21);
    }

    close(connfd);
}

int authenticateUser(char *username, char *password) {
    // Check if it's admin
    FILE* adminFile = fopen("admin.txt", "r");
    if (adminFile == NULL) {
        perror("Failed to open admin file");
        return -1;
    }

    char adminUsername[50], adminPassword[50];
    while (fscanf(adminFile, "%s %s", adminUsername, adminPassword) != EOF) {
        if (strcmp(username, adminUsername) == 0 && strcmp(password, adminPassword) == 0) {
            fclose(adminFile);
            return 1; // Admin
        }
    }
    fclose(adminFile);

    // Check if it's a student
    FILE* studentFile = fopen("student.txt", "r");
    if (studentFile == NULL) {
        perror("Failed to open student file");
        return -1;
    }

    int studentId, status;
    char studentFirstName[50], studentPassword[50];
    while (fscanf(studentFile, "%d %s %s %d", &studentId, studentFirstName, studentPassword, &status) != EOF) {
        if (strcmp(username, studentFirstName) == 0 && strcmp(password, studentPassword) == 0 && status == 1) {
            fclose(studentFile);
            return 2; // Active Student
        }
    }
    fclose(studentFile);

    // Check if it's a faculty
    FILE* facultyFile = fopen("faculty.txt", "r");
    if (facultyFile == NULL) {
        perror("Failed to open faculty file");
        return -1;
    }

    int facultyId;
    char facultyFirstName[50], facultyPassword[50];
    while (fscanf(facultyFile, "%d %s %s", &facultyId, facultyFirstName, facultyPassword) != EOF) {
        if (strcmp(username, facultyFirstName) == 0 && strcmp(password, facultyPassword) == 0) {
            fclose(facultyFile);
            return 3; // Faculty
        }
    }
    fclose(facultyFile);

    return 0; // Invalid credentials
}


void adminMenu(int connfd) {
    char option[10];
    char menu[] = "\nAdmin Menu:\n1. Add Student\n2. Add Faculty\n3. Activate/Deactivate Student\n4. Update Details\n5. Exit (type 'exit')\nChoice: \0";
    
    while (1) {
        bzero(option, sizeof(option));
        write(connfd, menu, sizeof(menu));
        bzero(option, sizeof(option));
        read(connfd, option, sizeof(option));
        int choice = atoi(option);

        switch (choice) {
            case 1:
                addStudentToFile(connfd);
                read(connfd, option, sizeof(option));
                break;
            case 2:
                addFacultyToFile(connfd);
                read(connfd, option, sizeof(option));
                break;
            case 3:
                toggleStudent(connfd);
                read(connfd, option, sizeof(option));
                break;
            case 4:
                updateDetails(connfd);
                read(connfd, option, sizeof(option));
                break;
            case 5:
                return;
            default:
                continue;
        }
    }
}

void addStudentToFile(int connfd) {
    sem_wait(&sem_student);

    char idStr[10], fname[50], password[50];
    int id;

    write(connfd, "Enter Student ID: ", 19);
    read(connfd, idStr, sizeof(idStr));
    sscanf(idStr, "%d", &id);

    write(connfd, "Enter Student Name: ", 21);
    read(connfd, fname, sizeof(fname));
    fname[strcspn(fname, "\n")] = 0;

    write(connfd, "Enter Student Password: ", 25);
    read(connfd, password, sizeof(password));
    password[strcspn(password, "\n")] = 0;

    int fd = open("student.txt", O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (fd == -1) {
        write(connfd, "Failed to open student file\n", 29);
        sem_post(&sem_student);
        return;
    }

    struct flock lock = { F_WRLCK, SEEK_SET, 0, 0 };
    fcntl(fd, F_SETLKW, &lock);

    // Append new student with status 0 (inactive)
    dprintf(fd, "%d %s %s 0\n", id, fname, password);

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);

    write(connfd, "Student added with status = 0 (inactive).\n", 42);
    sem_post(&sem_student);
}

void addFacultyToFile(int connfd) {
    sem_wait(&sem_faculty);

    char idStr[10], fname[50], password[50];
    int id;

    // Get Faculty ID
    write(connfd, "Enter Faculty ID: ", 19);
    memset(idStr, 0, sizeof(idStr));
    read(connfd, idStr, sizeof(idStr));
    idStr[strcspn(idStr, "\n")] = 0;
    sscanf(idStr, "%d", &id);

    // Get Faculty First Name
    write(connfd, "Enter Faculty Name: ", 21);
    memset(fname, 0, sizeof(fname));
    read(connfd, fname, sizeof(fname));
    fname[strcspn(fname, "\n")] = 0;

    // Get Faculty Password
    write(connfd, "Enter Faculty Password: ", 25);
    memset(password, 0, sizeof(password));
    read(connfd, password, sizeof(password));
    password[strcspn(password, "\n")] = 0;

    // Open faculty file and append new entry
    int fd = open("faculty.txt", O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (fd == -1) {
        write(connfd, "Failed to open faculty file\n", 29);
        sem_post(&sem_faculty);
        return;
    }

    struct flock lock = { F_WRLCK, SEEK_SET, 0, 0 };
    fcntl(fd, F_SETLKW, &lock);

    // Append line to file
    dprintf(fd, "%d %s %s\n", id, fname, password);

    // Unlock and clean up
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);

    write(connfd, "Faculty added successfully.\n", 29);
    sem_post(&sem_faculty);
}

void toggleStudent(int connfd) {
    sem_wait(&sem_student);

    char idStr[10];
    int id, found = 0;

    // Get Student ID
    write(connfd, "Enter Student ID to toggle: ", 29);
    memset(idStr, 0, sizeof(idStr));
    read(connfd, idStr, sizeof(idStr));
    idStr[strcspn(idStr, "\n")] = 0;
    sscanf(idStr, "%d", &id);

    int fd = open("student.txt", O_RDWR);
    if (fd == -1) {
        write(connfd, "Failed to open student file\n", 29);
        sem_post(&sem_student);
        return;
    }

    struct flock lock = {F_WRLCK, SEEK_SET, 0, 0};
    fcntl(fd, F_SETLKW, &lock);

    char buffer[8192], updated[8192], line[256];
    memset(buffer, 0, sizeof(buffer));
    memset(updated, 0, sizeof(updated));

    lseek(fd, 0, SEEK_SET);
    read(fd, buffer, sizeof(buffer) - 1);

    char *start = buffer;
    while (*start) {
        char *end = strchr(start, '\n');
        if (!end) break;
        *end = '\0';

        int fileId, status;
        char fname[50], password[50];

        sscanf(start, "%d %s %s %d", &fileId, fname, password, &status);

        if (fileId == id) {
            found = 1;
            status = (status == 0) ? 1 : 0;
            snprintf(line, sizeof(line), "%d %s %s %d\n", fileId, fname, password, status);
        } else {
            snprintf(line, sizeof(line), "%s\n", start);
        }

        strcat(updated, line);
        start = end + 1;
    }

    if (found) {
        ftruncate(fd, 0);
        lseek(fd, 0, SEEK_SET);
        write(fd, updated, strlen(updated));
        write(connfd, "Student status toggled successfully.\n", 38);
    } else {
        write(connfd, "Student ID not found.\n", 23);
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    sem_post(&sem_student);
}

void updateDetails(int connfd) {
    char choice[10];
    write(connfd, "Update details for Student/Faculty (s/f): ", 43);
    memset(choice, 0, sizeof(choice));
    read(connfd, choice, sizeof(choice));
    choice[strcspn(choice, "\n")] = 0;

    if (strcmp(choice, "s") == 0) {
        sem_wait(&sem_student);

        char idStr[10], newPassword[50];
        int id, found = 0;

        write(connfd, "Enter Student ID to update: ", 29);
        memset(idStr, 0, sizeof(idStr));
        read(connfd, idStr, sizeof(idStr));
        idStr[strcspn(idStr, "\n")] = 0;
        sscanf(idStr, "%d", &id);

        int fd = open("student.txt", O_RDWR);
        if (fd == -1) {
            write(connfd, "Failed to open student file\n", 29);
            sem_post(&sem_student);
            return;
        }

        struct flock lock = {F_WRLCK, SEEK_SET, 0, 0};
        fcntl(fd, F_SETLKW, &lock);

        char buffer[8192], updated[8192], line[256];
        memset(buffer, 0, sizeof(buffer));
        memset(updated, 0, sizeof(updated));

        lseek(fd, 0, SEEK_SET);
        read(fd, buffer, sizeof(buffer) - 1);

        char *start = buffer;
        while (*start) {
            char *end = strchr(start, '\n');
            if (!end) break;
            *end = '\0';

            int fileId, status;
            char fname[50], password[50];

            sscanf(start, "%d %s %s %d", &fileId, fname, password, &status);

            if (fileId == id) {
                found = 1;

                write(connfd, "Enter new password: ", 21);
                memset(newPassword, 0, sizeof(newPassword));
                read(connfd, newPassword, sizeof(newPassword));
                newPassword[strcspn(newPassword, "\n")] = 0;

                snprintf(line, sizeof(line), "%d %s %s %d\n", fileId, fname, newPassword, status);
            } else {
                snprintf(line, sizeof(line), "%s\n", start);
            }

            strcat(updated, line);
            start = end + 1;
        }

        if (found) {
            ftruncate(fd, 0);
            lseek(fd, 0, SEEK_SET);
            write(fd, updated, strlen(updated));
            write(connfd, "Student password updated successfully.\n", 40);
        } else {
            write(connfd, "Student ID not found.\n", 23);
        }

        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lock);
        close(fd);
        sem_post(&sem_student);
    }

    else if (strcmp(choice, "f") == 0) {
        sem_wait(&sem_faculty); // assuming you have a semaphore for faculty

        char idStr[10], newPassword[50];
        int id, found = 0;

        write(connfd, "Enter Faculty ID to update: ", 29);
        memset(idStr, 0, sizeof(idStr));
        read(connfd, idStr, sizeof(idStr));
        idStr[strcspn(idStr, "\n")] = 0;
        sscanf(idStr, "%d", &id);

        int fd = open("faculty.txt", O_RDWR);
        if (fd == -1) {
            write(connfd, "Failed to open faculty file\n", 29);
            sem_post(&sem_faculty);
            return;
        }

        struct flock lock = {F_WRLCK, SEEK_SET, 0, 0};
        fcntl(fd, F_SETLKW, &lock);

        char buffer[8192], updated[8192], line[256];
        memset(buffer, 0, sizeof(buffer));
        memset(updated, 0, sizeof(updated));

        lseek(fd, 0, SEEK_SET);
        read(fd, buffer, sizeof(buffer) - 1);

        char *start = buffer;
        while (*start) {
            char *end = strchr(start, '\n');
            if (!end) break;
            *end = '\0';

            int fileId;
            char fname[50], password[50];

            sscanf(start, "%d %s %s", &fileId, fname, password);

            if (fileId == id) {
                found = 1;

                write(connfd, "Enter new password: ", 21);
                memset(newPassword, 0, sizeof(newPassword));
                read(connfd, newPassword, sizeof(newPassword));
                newPassword[strcspn(newPassword, "\n")] = 0;

                snprintf(line, sizeof(line), "%d %s %s\n", fileId, fname, newPassword);
            } else {
                snprintf(line, sizeof(line), "%s\n", start);
            }

            strcat(updated, line);
            start = end + 1;
        }

        if (found) {
            ftruncate(fd, 0);
            lseek(fd, 0, SEEK_SET);
            write(fd, updated, strlen(updated));
            write(connfd, "Faculty password updated successfully.\n", 40);
        } else {
            write(connfd, "Faculty ID not found.\n", 23);
        }

        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lock);
        close(fd);
        sem_post(&sem_faculty);
    }

    else {
        write(connfd, "Invalid choice. Use 's' or 'f'.\n", 33);
    }
}


void facultyMenu(int connfd, char* username) {
    char option[10];
    char menu[] = "\nFaculty Menu:\n1. Add Course\n2. Remove Course\n3. View Enrollments\n4. Change Password\n5. Exit (type 'exit')\nEnter Choice: ";
    while (1) {
        bzero(option, sizeof(option));
        write(connfd, menu, sizeof(menu));
        bzero(option, sizeof(option));
        read(connfd, option, sizeof(option));
        int choice = atoi(option);

        switch (choice) {
            case 1:
                addCourseToFile(connfd, username);
                read(connfd, option, sizeof(option));
                break;
            case 2:
                removeCourse(connfd, username);
                read(connfd, option, sizeof(option));
                break;
            case 3:
                viewEnrollments(connfd);
                read(connfd, option, sizeof(option));
                break;
            case 4:
                facultyChangePassword(connfd, username);
                read(connfd, option, sizeof(option));
                break;
            case 5:
                return;
            default:
                continue;
        }
    }
}

void addCourseToFile(int connfd, char *username) {
    sem_wait(&sem_course);

    char courseId[10], courseName[50];
    int id;

    write(connfd, "Enter Course ID: ", 17);
    read(connfd, courseId, sizeof(courseId));
    sscanf(courseId, "%d", &id);

    write(connfd, "Enter Course Name: ", 19);
    read(connfd, courseName, sizeof(courseName));
    courseName[strcspn(courseName, "\n")] = 0;

    int fd = open("course.txt", O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (fd == -1) {
        write(connfd, "Failed to open course file\n", 28);
        sem_post(&sem_course);
        return;
    }

    struct flock lock = { F_WRLCK, SEEK_SET, 0, 0 };
    fcntl(fd, F_SETLKW, &lock);

    // Store course with faculty username
    dprintf(fd, "%d %s %s\n", id, courseName, username);

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);

    write(connfd, "Course added successfully.\n", 27);
    sem_post(&sem_course);
}

void removeCourse(int connfd, char *username) {
    sem_wait(&sem_course);

    char courseIdStr[10];
    int courseId, found = 0;

    // Ask for Course ID to remove
    write(connfd, "Enter Course ID to remove: ", 27);
    memset(courseIdStr, 0, sizeof(courseIdStr));
    read(connfd, courseIdStr, sizeof(courseIdStr));
    sscanf(courseIdStr, "%d", &courseId);

    // Open the original course file for reading
    FILE *courseFile = fopen("course.txt", "r");
    if (courseFile == NULL) {
        write(connfd, "Failed to open course file\n", 26);
        sem_post(&sem_course);
        return;
    }

    // Create a temporary file for writing the updated records
    FILE *tempFile = fopen("course_temp.txt", "w");
    if (tempFile == NULL) {
        write(connfd, "Failed to create temporary file\n", 32);
        fclose(courseFile);
        sem_post(&sem_course);
        return;
    }

    int fd = fileno(courseFile);
    // Lock
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    char line[256];
    int tempId;
    char tempName[50], tempFaculty[50];

    // Read through each line and write it to the temp file unless the course ID matches
    while (fscanf(courseFile, "%d %s %s", &tempId, tempName, tempFaculty) != EOF) {
        if (tempId != courseId || strcmp(tempFaculty, username) != 0) {
            fprintf(tempFile, "%d %s %s\n", tempId, tempName, tempFaculty);
        } else {
            found = 1; // Course ID found, so it will not be copied to the temp file
        }
    }

    // Unlock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);

    fclose(courseFile);
    fclose(tempFile);

    // If the course was not found, notify the user
    if (!found) {
        write(connfd, "Course ID not found.\n", 21);
        remove("course_temp.txt"); // Remove the temp file if no record was found
        sem_post(&sem_course);
        return;
    }

    // Overwrite the original course file with the updated data
    if (rename("course_temp.txt", "course.txt") != 0) {
        write(connfd, "Failed to update course file\n", 29);
        sem_post(&sem_course);
        return;
    }

    write(connfd, "Course removed successfully.\n", 30);
    sem_post(&sem_course);
}

void viewEnrollments(int connfd) {
    sem_wait(&sem_enrollment);

    char courseIdStr[10];
    int courseId;
    int studentIdBuffer[100]; // Buffer to store student IDs
    int studentCount = 0;
    char studentDataBuffer[1024] = ""; // Buffer to store all student names

    // Ask for Course ID
    write(connfd, "Enter Course ID to view enrollment: ", 37);
    memset(courseIdStr, 0, sizeof(courseIdStr));
    read(connfd, courseIdStr, sizeof(courseIdStr));
    sscanf(courseIdStr, "%d", &courseId);

    // Open the enrollment file to get the students enrolled in the course
    FILE *enrollmentFile = fopen("enrollment.txt", "r");
    if (enrollmentFile == NULL) {
        write(connfd, "Failed to open enrollment file\n", 30);
        sem_post(&sem_enrollment);
        return;
    }

    int fd = fileno(enrollmentFile);
    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    // Read through the enrollment file and store student IDs for the given course
    int tempCourseId, tempStudentId;
    while (fscanf(enrollmentFile, "%d %d", &tempCourseId, &tempStudentId) != EOF) {
        if (tempCourseId == courseId) {
            studentIdBuffer[studentCount++] = tempStudentId;
        }
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);  

    fclose(enrollmentFile);

    // If no students are enrolled in the course, notify the user
    if (studentCount == 0) {
        write(connfd, "No students are enrolled in this course.\n", 40);
        sem_post(&sem_enrollment);
        return;
    }

    // Open the student file to get student details
    FILE *studentFile = fopen("student.txt", "r");
    if (studentFile == NULL) {
        write(connfd, "Failed to open student file\n", 26);
        sem_post(&sem_enrollment);
        return;
    }

    // Lock the student file for reading
    fd = fileno(studentFile);
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    // Iterate through the student file and collect details for each student enrolled in the course
    char studentName[50], studentPassword[50], studentStatus[10];
    tempStudentId = 0;
    int found = 0;

    // Loop through the student file and add student names to the buffer
    while (fscanf(studentFile, "%d %s %s %s", &tempStudentId, studentName, studentPassword, studentStatus) != EOF) {
        for (int i = 0; i < studentCount; i++) {
            if (studentIdBuffer[i] == tempStudentId) {
                // Concatenate the student name to the buffer
                if (strlen(studentDataBuffer) + strlen(studentName) + 20 < sizeof(studentDataBuffer)) {
                    // Add student ID and name to the buffer
                    char tempBuffer[256];
                    snprintf(tempBuffer, sizeof(tempBuffer), "Student ID: %d, Name: %s\n", tempStudentId, studentName);
                    strcat(studentDataBuffer, tempBuffer);
                    found = 1;
                } else {
                    // If buffer is full, break out of the loop
                    break;
                }
            }
        }
    }

    // Unlock the student file
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    // Close the student file
    fclose(studentFile);

    // If no students were found (which shouldn't happen unless there was an error), notify the user
    if (!found) {
        write(connfd, "No student details found for enrolled students.\n", 49);
    } else {
        // Write the entire collected student data to the client at once
        write(connfd, studentDataBuffer, strlen(studentDataBuffer));
    }

    sem_post(&sem_enrollment);
}

void facultyChangePassword(int connfd, char* username) {
    sem_wait(&sem_faculty);

    char newPassword[50];
    int found = 0;

    // Ask for new password
    write(connfd, "Enter new password: ", 21);
    memset(newPassword, 0, sizeof(newPassword));
    read(connfd, newPassword, sizeof(newPassword));
    newPassword[strcspn(newPassword, "\n")] = 0;

    // Open the original faculty file for reading
    FILE *facultyFile = fopen("faculty.txt", "r");
    if (facultyFile == NULL) {
        write(connfd, "Failed to open faculty file\n", 29);
        sem_post(&sem_faculty);
        return;
    }

    // Create a temporary file for writing the updated records
    FILE *tempFile = fopen("faculty_temp.txt", "w");
    if (tempFile == NULL) {
        write(connfd, "Failed to create temporary file\n", 32);
        fclose(facultyFile);
        sem_post(&sem_faculty);
        return;
    }

    int fd = fileno(facultyFile);
    // Lock
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    char line[256];
    int tempId;
    char tempFirstName[50], tempPassword[50];

    // Read through each line and write it to the temp file
    while (fscanf(facultyFile, "%d %s %s", &tempId, tempFirstName, tempPassword) != EOF) {
        if (strcmp(username, tempFirstName) == 0) {
            fprintf(tempFile, "%d %s %s\n", tempId, tempFirstName, newPassword);
            found = 1;
        } else {
            fprintf(tempFile, "%d %s %s\n", tempId, tempFirstName, tempPassword);
        }
    }

    // Unlock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);

    fclose(facultyFile);
    fclose(tempFile);

    // If the username was not found, notify the user
    if (!found) {
        write(connfd, "Username not found.\n", 21);
        remove("faculty_temp.txt"); // Remove the temp file if no record was found
        sem_post(&sem_faculty);
        return;
    }

    // Overwrite the original faculty file with the updated data
    if (rename("faculty_temp.txt", "faculty.txt") != 0) {
        write(connfd, "Failed to update faculty file\n", 30);
        sem_post(&sem_faculty);
        return;
    }

    write(connfd, "Password changed successfully.\n", 31);
    sem_post(&sem_faculty);
}


void studentMenu(int connfd, char* username) {
    char option[10];
    char menu[] = "\nStudent Menu:\n1. Enroll in Course\n2. Unenroll\n3. View Courses\n4. Change Password\n5. Exit (type 'exit')\nEnter Choice: ";
    while (1) {
        bzero(option, sizeof(option));
        write(connfd, menu, sizeof(menu));
        bzero(option, sizeof(option));
        read(connfd, option, sizeof(option));
        int choice = atoi(option);

        switch (choice) {
            case 1:
                // Enroll in Course
                enrollCourse(connfd, username);
                read(connfd, option, sizeof(option));
                break;
            case 2:
                // Unenroll
                unenrollCourse(connfd, username);
                read(connfd, option, sizeof(option));
                break;
            case 3:
                // View Courses
                viewCourses(connfd, username);
                read(connfd, option, sizeof(option));
                break;
            case 4:
                // Change Password
                changeStudentPassword(connfd, username);
                read(connfd, option, sizeof(option));
                break;
            case 5:
                return;
            default:
                continue;
        }
    }
}

void enrollCourse(int connfd, char* username) {
    sem_wait(&sem_enrollment);

    char courseIdStr[10];
    int studentId = -1;

    // Get course ID from user
    write(connfd, "Enter Course ID to enroll: ", 28);
    memset(courseIdStr, 0, sizeof(courseIdStr));
    read(connfd, courseIdStr, sizeof(courseIdStr));
    courseIdStr[strcspn(courseIdStr, "\n")] = 0;

    // checking if limit exceeded
    if (courseLimit[atoi(courseIdStr)] >= 50) {
        write(connfd, "Course limit exceeded.\n", 23);
        sem_post(&sem_enrollment);
        return;
    }

    // Find student ID from student.txt using username
    FILE *fp = fopen("student.txt", "r");
    if (!fp) {
        write(connfd, "Unable to open student.txt\n", 28);
        sem_post(&sem_enrollment);
        return;
    }

    // Lock the student file for reading
    int fd = fileno(fp);
    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    int id, status;
    char name[50], pwd[50];

    while (fscanf(fp, "%d %s %s %d", &id, name, pwd, &status) != EOF) {
        if (strcmp(name, username) == 0) {
            studentId = id;
            break;
        }
    }

    // Unlock the student file
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);

    fclose(fp);

    if (studentId == -1) {
        write(connfd, "Student not found.\n", 20);
        sem_post(&sem_student);
        sem_post(&sem_course);
        sem_post(&sem_enrollment);
        return;
    }

    // Write <course_id> <student_id> to enrollment.txt
    fd = open("enrollment.txt", O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (fd == -1) {
        write(connfd, "Unable to open enrollment.txt\n", 31);
        sem_post(&sem_student);
        sem_post(&sem_course);
        sem_post(&sem_enrollment);
        return;
    }

    struct flock nlock = {F_WRLCK, SEEK_SET, 0, 0};
    fcntl(fd, F_SETLKW, &nlock);

    int courseId = atoi(courseIdStr);

    dprintf(fd, "%d %d\n", courseId, studentId);

    nlock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &nlock);
    close(fd);

    write(connfd, "Enrollment successful.\n", 24);

    // Increment the course limit
    courseLimit[courseId]++;

    sem_post(&sem_enrollment);
}

void unenrollCourse(int connfd, char* username) {
    sem_wait(&sem_enrollment);

    char courseIdStr[10];
    int studentId = -1;

    // Get course ID from user
    write(connfd, "Enter Course ID to unenroll: ", 30);
    memset(courseIdStr, 0, sizeof(courseIdStr));
    read(connfd, courseIdStr, sizeof(courseIdStr));
    courseIdStr[strcspn(courseIdStr, "\n")] = 0;

    // Find student ID from student.txt using username
    FILE *fp = fopen("student.txt", "r");
    if (!fp) {
        write(connfd, "Unable to open student.txt\n", 28);
        sem_post(&sem_enrollment);
        return;
    }

    // Lock the student file for reading
    int fd = fileno(fp);
    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    int id, status;
    char name[50], pwd[50];

    while (fscanf(fp, "%d %s %s %d", &id, name, pwd, &status) != EOF) {
        if (strcmp(name, username) == 0) {
            studentId = id;
            break;
        }
    }

    // Unlock the student file
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    fclose(fp); 
    if (studentId == -1) {
        write(connfd, "Student not found.\n", 20);
        sem_post(&sem_enrollment);
        return;
    }
    // Open the original enrollment file for reading
    FILE *enrollmentFile = fopen("enrollment.txt", "r");
    if (enrollmentFile == NULL) {
        write(connfd, "Failed to open enrollment file\n", 30);
        sem_post(&sem_enrollment);
        return;
    }
    // Create a temporary file for writing the updated records
    FILE *tempFile = fopen("enrollment_temp.txt", "w");     
    if (tempFile == NULL) {
        write(connfd, "Failed to create temporary file\n", 32);
        fclose(enrollmentFile);
        sem_post(&sem_enrollment);
        return;
    }
    fd = fileno(enrollmentFile);
    // Lock
    struct flock nlock;
    nlock.l_type = F_WRLCK;
    nlock.l_whence = SEEK_SET;
    nlock.l_start = 0;
    nlock.l_len = 0;
    fcntl(fd, F_SETLKW, &nlock);

    char line[256];
    int tempCourseId, tempStudentId;
    int found = 0;
    // Read through each line and write it to the temp file unless the course ID matches
    while (fscanf(enrollmentFile, "%d %d", &tempCourseId, &tempStudentId) != EOF) {
        if (tempCourseId == atoi(courseIdStr) && tempStudentId == studentId) {
            found = 1; // Course ID and Student ID found
        } else {
            fprintf(tempFile, "%d %d\n", tempCourseId, tempStudentId);
        }
    }
    // Unlock
    nlock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &nlock);
    fclose(enrollmentFile);
    fclose(tempFile);

    // If the course was not found, notify the user
    if (!found) {
        write(connfd, "Enrollment record not found.\n", 30);
        remove("enrollment_temp.txt"); // Remove the temp file if no record was found
        sem_post(&sem_enrollment);
        return;
    }

    // Overwrite the original enrollment file with the updated data
    if (rename("enrollment_temp.txt", "enrollment.txt") != 0) {
        write(connfd, "Failed to update enrollment file\n", 34);
        sem_post(&sem_enrollment);
        return;
    }

    write(connfd, "Unenrollment successful.\n", 25);

    // Decrement the course limit
    int courseId = atoi(courseIdStr);
    courseLimit[courseId]--;
    if (courseLimit[courseId] < 0) {
        courseLimit[courseId] = 0; // Ensure it doesn't go negative
    }

    sem_post(&sem_enrollment);
}

void viewCourses(int connfd, char* username) {
    sem_wait(&sem_enrollment); // Only semaphore used

    int studentId = -1;
    FILE *file;
    struct flock lock;
    char buffer[1024] = "";
    char temp[256];

    // 1. Find student ID from student.txt
    file = fopen("student.txt", "r");
    if (!file) {
        write(connfd, "Error opening student.txt\n", 26);
        sem_post(&sem_enrollment);
        return;
    }

    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock entire file
    fcntl(fileno(file), F_SETLKW, &lock);

    int id, status;
    char name[50], pwd[50];
    while (fscanf(file, "%d %s %s %d", &id, name, pwd, &status) != EOF) {
        if (strcmp(name, username) == 0) {
            studentId = id;
            break;
        }
    }

    lock.l_type = F_UNLCK;
    fcntl(fileno(file), F_SETLK, &lock);
    fclose(file);

    if (studentId == -1) {
        write(connfd, "Student not found\n", 18);
        sem_post(&sem_enrollment);
        return;
    }

    // 2. Find all course IDs for the student from enrollment.txt
    int courseIds[100];
    int courseCount = 0;

    file = fopen("enrollment.txt", "r");
    if (!file) {
        write(connfd, "Error opening enrollment.txt\n", 29);
        sem_post(&sem_enrollment);
        return;
    }

    lock.l_type = F_RDLCK;
    fcntl(fileno(file), F_SETLKW, &lock);

    int courseId, sid;
    while (fscanf(file, "%d %d", &courseId, &sid) != EOF) {
        if (sid == studentId) {
            courseIds[courseCount++] = courseId;
        }
    }

    lock.l_type = F_UNLCK;
    fcntl(fileno(file), F_SETLK, &lock);
    fclose(file);

    if (courseCount == 0) {
        write(connfd, "No courses enrolled.\n", 22);
        sem_post(&sem_enrollment);
        return;
    }

    // 3. Retrieve course info from course.txt
    file = fopen("course.txt", "r");
    if (!file) {
        write(connfd, "Error opening course.txt\n", 25);
        sem_post(&sem_enrollment);
        return;
    }

    lock.l_type = F_RDLCK;
    fcntl(fileno(file), F_SETLKW, &lock);

    int cid;
    char cname[100], faculty[100];
    while (fscanf(file, "%d %s %s", &cid, cname, faculty) != EOF) {
        for (int i = 0; i < courseCount; ++i) {
            if (cid == courseIds[i]) {
                snprintf(temp, sizeof(temp), "Course ID: %d, Name: %s, Faculty: %s\n", cid, cname, faculty);
                if (strlen(buffer) + strlen(temp) < sizeof(buffer)) {
                    strcat(buffer, temp);
                }
            }
        }
    }

    lock.l_type = F_UNLCK;
    fcntl(fileno(file), F_SETLK, &lock);
    fclose(file);

    // 4. Send to client
    write(connfd, buffer, strlen(buffer));

    sem_post(&sem_enrollment);
}

void changeStudentPassword(int connfd, char* username) {
    sem_wait(&sem_student);

    char newPassword[50];
    int found = 0;

    // Ask for new password
    write(connfd, "Enter new password: ", 21);
    memset(newPassword, 0, sizeof(newPassword));
    read(connfd, newPassword, sizeof(newPassword));
    newPassword[strcspn(newPassword, "\n")] = 0;

    // Open the original student file for reading
    FILE *studentFile = fopen("student.txt", "r");
    if (studentFile == NULL) {
        write(connfd, "Failed to open student file\n", 29);
        sem_post(&sem_student);
        return;
    }

    // Create a temporary file for writing the updated records
    FILE *tempFile = fopen("student_temp.txt", "w");
    if (tempFile == NULL) {
        write(connfd, "Failed to create temporary file\n", 32);
        fclose(studentFile);
        sem_post(&sem_student);
        return;
    }

    int fd = fileno(studentFile);
    // Lock
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    char line[256];
    int tempId, status;
    char tempFirstName[50], tempPassword[50];

    // Read through each line and write it to the temp file
    while (fscanf(studentFile, "%d %s %s %d", &tempId, tempFirstName, tempPassword, &status) != EOF) {
        if (strcmp(username, tempFirstName) == 0) {
            fprintf(tempFile, "%d %s %s %d\n", tempId, tempFirstName, newPassword, status);
            found = 1;
        } else {
            fprintf(tempFile, "%d %s %s %d\n", tempId, tempFirstName, tempPassword, status);
        }
    }

    // Unlock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);

    fclose(studentFile);
    fclose(tempFile);

    // If the username was not found, notify the user
    if (!found) {
        write(connfd, "Username not found.\n", 21);
        remove("student_temp.txt"); // Remove the temp file if no record was found
        sem_post(&sem_student);
        return;
    }
    // Overwrite the original student file with the updated data
    if (rename("student_temp.txt", "student.txt") != 0) {
        write(connfd, "Failed to update student file\n", 30);
        sem_post(&sem_student);
        return;
    }
    write(connfd, "Password changed successfully.\n", 31);
    sem_post(&sem_student);
}