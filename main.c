#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <ncurses.h>

#define MAX_PROCESSES 100
#define MAX_COMMAND_LENGTH 100
#define MAX_STATUS_LINE_LENGTH 256
#define MAX_NAME_LENGTH 256
#define MAX_MEM_LENGTH 128
#define MAX_PID_DIGITS 8

#define SORT_BY_PID 0
#define SORT_BY_NAME 1
#define SORT_BY_MEM 2
#define SORT_BY_CPU 3

typedef struct {
    int pid;
    char name[MAX_NAME_LENGTH];
    char mem_usage[MAX_MEM_LENGTH];
    double cpu_usage;
} ProcessInfo;

int starts_with(const char *prefix, const char *str) {
    return strncmp(prefix, str, strlen(prefix)) == 0;
}

int is_process_dir(const struct dirent *entry) {
    for (size_t i = 0; i < strlen(entry->d_name); i++) {
        if (!isdigit(entry->d_name[i]))
            return 0;
    }
    return 1;
}

char* extract_info_from_status(FILE* fp, const char* label) {
    char line[MAX_STATUS_LINE_LENGTH];
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (starts_with(label, line)) {
            char* value = strdup(line + strlen(label));
            return value;
        }
    }
    return strdup("");
}

void kill_process(int pid) {
    kill(pid, SIGTERM);
}

double get_process_cpu_usage(int pid) {
    char command[MAX_COMMAND_LENGTH];
    snprintf(command, sizeof(command), "ps -p %d -o %%cpu | tail -1", pid);
    FILE *fp = popen(command, "r");
    if (fp != NULL) {
        char value[16];
        if (fgets(value, sizeof(value), fp) != NULL) {
            return atof(value);
        }
        pclose(fp);
    }
    return 0.0;
}

ProcessInfo* get_processes(int* count, int sort_type) {
    *count = 0;
    ProcessInfo* processes = malloc(MAX_PROCESSES * sizeof(ProcessInfo));
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        return NULL;
    }
    struct dirent *entry;
    while ((*count) < MAX_PROCESSES && (entry = readdir(dir)) != NULL) {
        if (is_process_dir(entry)) {
            char status_file_path[MAX_COMMAND_LENGTH];
            snprintf(status_file_path, sizeof(status_file_path), "/proc/%s/status", entry->d_name);
            FILE *status_file = fopen(status_file_path, "r");
            if (status_file != NULL) {
                int pid = atoi(entry->d_name);
                processes[*count].pid = pid;
                char* name = extract_info_from_status(status_file, "Name:");
                strcpy(processes[*count].name, name);
                free(name);
                char* memory = extract_info_from_status(status_file, "VmSize:");
                strcpy(processes[*count].mem_usage, memory);
                free(memory);
                processes[*count].cpu_usage = get_process_cpu_usage(pid);
                (*count)++;
                fclose(status_file);
            }
        }
    }
    closedir(dir);
    if (sort_type == SORT_BY_PID) {
        qsort(processes, *count, sizeof(ProcessInfo), cmp_pid);
    } else if (sort_type == SORT_BY_NAME) {
        qsort(processes, *count, sizeof(ProcessInfo), cmp_name);
    } else if (sort_type == SORT_BY_MEM) {
        qsort(processes, *count, sizeof(ProcessInfo), cmp_mem);
    } else if (sort_type == SORT_BY_CPU) {
        qsort(processes, *count, sizeof(ProcessInfo), cmp_cpu);
    }
    return processes;
}

void draw_ui(ProcessInfo* processes, int count, int cursor) {
    clear();
    mvprintw(0, 0, "PID\tNAME\t\t\tCPU\tMEMORY");
    for (int i = 0; i < count; i++) {
        mvprintw(i + 1, 0, "%d\t%s\t\t%.2f\t%s", processes[i].pid, processes[i].name, processes[i].cpu_usage, processes[i].mem_usage);
    }
    mvprintw(cursor + 1, 0, ">"); // Display cursor
    refresh();
}

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, true);
    int running = 1;
    int cursor = 0;
    int sort_type = SORT_BY_PID;
    int count = 0;
    ProcessInfo* processes = NULL;
    while (running) {
        processes = get_processes(&count, sort_type);
        if (processes != NULL) {
            if (cursor >= count) {
                cursor = count - 1;
            }
            draw_ui(processes, count, cursor);
            int input = getch();
            switch (input) {
                case KEY_UP:
                    if (cursor > 0) cursor--;
                    break;
                case KEY_DOWN:
                    if (cursor < count - 1) cursor++;
                    break;
                case KEY_F(5):
                    sort_type = SORT_BY_PID;
                    break;
                case KEY_F(6):
                    sort_type = SORT_BY_NAME;
                    break;
                case KEY_F(7):
                    sort_type = SORT_BY_CPU;
                    break;
                case KEY_F(8):
                    sort_type = SORT_BY_MEM;
                    break;
                case KEY_F(9):
                    if (count > 0) kill_process(processes[cursor].pid);
                    break;
                case KEY_F(10):
                    running = 0;
                    break;
            }
            free(processes);
        }
    }
    endwin();
    return 0;
}
