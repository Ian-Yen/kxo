#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "game.h"

#define XO_STATUS_FILE "/sys/module/kxo/initstate"
#define XO_DEVICE_FILE "/dev/kxo"
#define XO_DEVICE_ATTR_FILE "/sys/class/kxo/kxo/kxo_state"

#define IOCTL_READ_BOARD 0
#define IOCTL_READ_TURN 1

static bool status_check(void)
{
    FILE *fp = fopen(XO_STATUS_FILE, "r");
    if (!fp) {
        printf("kxo status : not loaded\n");
        return false;
    }

    char read_buf[20];
    fgets(read_buf, 20, fp);
    read_buf[strcspn(read_buf, "\n")] = 0;
    if (strcmp("live", read_buf)) {
        printf("kxo status : %s\n", read_buf);
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

static struct termios orig_termios;

static void raw_mode_disable(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void raw_mode_enable(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(raw_mode_disable);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~IXON;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static bool read_attr, end_attr;

static void change_board(int n)
{
    int row = (n >> 2);
    int column = n % 4;
    printf("%c%d", row + 'a', column);
}

void output_record(int device_fd)
{
    uint64_t board[16] = {0};
    ioctl(device_fd, IOCTL_READ_BOARD, &board);
    int turn[16] = {0};
    ioctl(device_fd, IOCTL_READ_TURN, &turn);
    for (int i = 0; i < 16; i++) {
        if (!turn[i])
            break;
        // printf("%ld", board[i]);
        // printf("%d\n", turn[i]);
        for (int j = 0; j < turn[i]; j++) {
            change_board((int) (board[i] & 15));
            if (j + 1 < turn[i])
                printf("->");
            board[i] = (board[i] >> 4);
        }
        printf("\n");
    }
}

static void listen_keyboard_handler(int device_fd)
{
    int attr_fd = open(XO_DEVICE_ATTR_FILE, O_RDWR);
    char input;

    if (read(STDIN_FILENO, &input, 1) == 1) {
        char buf[20];
        switch (input) {
        case 16: /* Ctrl-P */
            read(attr_fd, buf, 6);
            buf[0] = (buf[0] - '0') ? '0' : '1';
            read_attr ^= 1;
            write(attr_fd, buf, 6);
            if (!read_attr) {
                printf("Stopping to display the chess board...\n");
                output_record(device_fd);
            }
            break;
        case 17: /* Ctrl-Q */
            read(attr_fd, buf, 6);
            buf[4] = '1';
            read_attr = false;
            end_attr = true;
            write(attr_fd, buf, 6);
            printf("Stopping the kernel space tic-tac-toe game...\n");
            break;
        }
    }
    close(attr_fd);
}

void output_time(void)
{
    time_t current_time;
    const struct tm *time_info;
    char buffer[80];

    time(&current_time);
    time_info = localtime(&current_time);

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", time_info);
    printf("Current time: %s\n", buffer);
}

static char *output_board(const char *table)
{
    int i = 4, k = 0;
    puts("\n");

    while (i--) {
        for (int j = 0; j < (BOARD_SIZE << 1) - 1 && k < N_GRIDS; j++) {
            putchar(j & 1 ? '|' : table[k++]);
        }
        putchar('\n');
        for (int j = 0; j < (BOARD_SIZE << 1) - 1; j++)
            putchar('-');
        putchar('\n');
    }


    return 0;
}

int main(int argc, char *argv[])
{
    if (!status_check())
        exit(1);

    raw_mode_enable();
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    char display_buf[N_GRIDS];

    fd_set readset;
    int device_fd = open(XO_DEVICE_FILE, O_RDONLY);
    int max_fd = device_fd > STDIN_FILENO ? device_fd : STDIN_FILENO;
    read_attr = true;
    end_attr = false;

    while (!end_attr) {
        FD_ZERO(&readset);
        FD_SET(STDIN_FILENO, &readset);
        FD_SET(device_fd, &readset);

        int result = select(max_fd + 1, &readset, NULL, NULL, NULL);
        if (result < 0) {
            printf("Error with select system call\n");
            exit(1);
        }

        if (FD_ISSET(STDIN_FILENO, &readset)) {
            FD_CLR(STDIN_FILENO, &readset);
            listen_keyboard_handler(device_fd);
        } else if (read_attr && FD_ISSET(device_fd, &readset)) {
            FD_CLR(device_fd, &readset);
            printf("\033[H\033[J"); /* ASCII escape code to clear the screen */
            read(device_fd, display_buf, N_GRIDS);
            output_board(display_buf);
            output_time();
        }
    }

    raw_mode_disable();

    output_record(device_fd);

    fcntl(STDIN_FILENO, F_SETFL, flags);

    close(device_fd);

    return 0;
}
