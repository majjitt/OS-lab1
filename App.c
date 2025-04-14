#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define BUF_SIZE 100

int main(int argc, char *argv[]) {
    if (argc != 2) {  
        printf(1, "Usage: app_name <input_string>\n");
        exit();
    }

    char *input = argv[1];
    int len = strlen(input);

    
    if (len == 2 && input[0] == '{' && input[1] == '}') {
        
        int fd = open("Result.txt", O_CREATE | O_WRONLY);
        write(fd, "Right\n", 6);
        close(fd);
    } else {
        
        int fd = open("Result.txt", O_CREATE | O_WRONLY);
        write(fd, "Wrong\n", 6);
        close(fd);
    }

    exit();
}
