#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define MAX_LEN 1000  

int isValidString(const char *str) {
    int balance = 0;
    while (*str) {
        if (*str == '{') {
            balance++;  
        } else if (*str == '}') {
            balance--;  
        }
        if (balance < 0) {  
            return 0;
        }
        str++;
    }
    return balance == 0;  
}

int main(int argc, char *argv[]) {
    int fd;
    
    if (argc != 2) {
        printf(1, "Usage: %s <string>\n", argv[0]);
        exit();
    }

    const char *input = argv[1];
    const char *result = isValidString(input) ? "Right\n" : "Wrong\n";

    fd = open("Result.txt", O_CREATE | O_WRONLY);
    if (fd < 0) {
        printf(1, "NULL\n");
        exit();
    }
    write(fd, result, strlen(result));
    close(fd);

    exit();
}

