#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#define MSG_SLOT_CHANNEL 420 

int main(int argc, char** argv){
    if(argc != 3){
        errno = EINVAL;
        fprintf(stderr, "Invalid number of arguments\n");
        return -1;
    }

    char* message_slot_path = argv[1];
    int channel_id = atoi(argv[2]);

    char* buffer = (char*) malloc(sizeof(char) * 129);

    int fd = open(message_slot_path, O_RDWR);
    if (fd == -1) {
        perror("Failed to open device file");
        return -1;
    }
    int scode;
    scode = ioctl(fd, MSG_SLOT_CHANNEL, channel_id);
    if(scode == -1){
        fprintf(stderr, "an error occured switching channel, errno: %d\n", errno);
        return scode;
    }
    scode = read(fd, buffer, 128);
    if(scode == -1){
        fprintf(stderr, "an error occured writing to channel, errno: %d\n", errno);
        return scode;
    }
    ssize_t character_read = scode;
    buffer[scode] = '\0';
    printf("%s", buffer);
    

    close(fd);
    return 0;
}