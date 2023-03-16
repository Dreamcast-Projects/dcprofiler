#include <kos.h>

#include <stdio.h>
#include <stdlib.h> // atexit
#include "profiler.h"

int i = 0;

static void __attribute__((__noreturn__)) wait_exit() {
    maple_device_t *dev;
    cont_state_t *state;

    printf("Press any button to exit.\n");

    for(;;) {
        dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);

        if(dev) {
            state = (cont_state_t *)maple_dev_status(dev);

            if(state)   {
                fflush(stdout);
                if(state->buttons)
                {
                    stopProfiling();
                    shutdownProfiling();
                    arch_exit();
                }
            }
        }
    }
}

void test1()
{
    printf("Testing function 1\n");
}

void test2()
{
    printf("Testing function 2\n");
    test1();
}

void test3()
{
    printf("Testing function 3\n");
    test1();
    test2();
}

void test4()
{
    printf("Testing function 4\n");
    test3();
    test2();
    test1();
}

void test5()
{
    printf("Testing function 5\n");
    test1();
    test2();
    test3();
    test4();
}

int main(int argc, char **argv) {

    vid_set_mode(DM_640x480, PM_RGB555);
    vid_clear(255, 0, 255);

    cont_btn_callback(0, CONT_START | CONT_A | CONT_B | CONT_X | CONT_Y, (cont_btn_callback_t) arch_exit);

    test1();
    test2();
    test3();
    test4();
    test5();

    return 0;
}