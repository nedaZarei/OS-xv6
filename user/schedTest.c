#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "../user/user.h"

int
main(int argc, char *argv[]) {
    int id = 0;
    int index = 0;
    //creating 10 processes
    for (int i = 0; i < 9; ++i) {
        if (id == 0) {
            id = fork();
            if (id != 0) {
                index = i + 1;
            }
        }
    }

    for (int i = 0; i < 1000000000; i++) {}
    printf("DONE: PROCESS %d\n", index);
    exit(0);
}