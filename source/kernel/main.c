#include "butterfly.h"


/**
 * Okay okay okay... there shouldn't be a shell inside a kernel...
 * this isn't userspace yet... but it's very useful, right? :)
 */
void main(void) {

    /* I have a natural revulsion to any operating system that shows so little
     * planning as to have to named all of its commands after digestive noises
     * (awk, grep, fsck, nroff)
    */

    while (true) {
        printf("\n");

        if (BFS_CURRENT_DIR != BFS_PRIMARY_DIR) {
            printf("[/%s]", BFS_CURRENT_DIR->name);
        }

        printl("\033[32;40m[@]\033[0m", " ");

        char *input = gets();

        // Trim whitespaces for avoid errors
        strtrim(input);

        if (!strlen(input)) {
            continue;
        }


        if (strcmp(input, "HALT") == 0) {
            printl(INFO, "The CPU has HALTED NOW!");
            powerControl(POWER_HALT);
        }

        else if (strcmp(input, "SHUTDOWN") == 0) {
            printl(INFO, "Shutting down the system ...");
            shutdownSound();

            tty_clear(NULL);
            tty_cursor(0x3F, -1, -1);

            initializeVGA(video_mode);
            gfx_clear(PX_BLACK);
            gfx_draw(candle_640, 0, 0, 640, 480);

            powerControl(POWER_SHUTDOWN);

            /* Just in case */
            FOREVER NOTHING
        }

        else if (strcmp(input, "REBOOT") == 0) {
            printl(INFO, "Rebooting the system ...");
            shutdownSound();
            powerControl(POWER_REBOOT);
        }

        else if (strcmp(input, "CLS") == 0 || strcmp(input, "CLEAR") == 0) {
            tty_clear(NULL);
        }

        else if (strcmp(input, "CLOCK") == 0) {
            getDateTime();
            printf("\n");
            getCalendar();
        }

        else if (strcmp(input, "BUG") == 0) {
            triggerPanic("Controlled Kernel Panic", 0, 0xDEADDEAD, NULL);

        } else if (substrcmp("CALC", input) == 0) {
            printl(LINE, "The result is %f\n", getMath(input, 5));

        } else if (substrcmp("BEEP", input) == 0) {
            playChord(input, 5);

        } else if (strcmp("TIMER", input) == 0) {
            printl(LINE, "Counter: %d\n", timerGetTicks());
            printl(LINE, "Elapsed: %d:%d:%d\n", timerGetHours(), timerGetMinutes(), timerGetSeconds());

        } else if (strcmp("INFO", input) == 0) {
            printl(INFO, "Project development:\n");
            printf(" * Developed by %s\n", "Nahuel Senek (TheBigEye)");
            printf(" * Tested by %s\n", "Sergio Vargas");
            printf(" * Inspired from Temple OS, by %s\n\n", "Terry Davis");

            printl(INFO, "Kernel build info:\n");
            printf(" * Kernel version %s\n", __kernel_version);
            printf(" * Kernel project %s\n", __kernel_name);
            printf(" * Kernel codename %s\n", __kernel_version_codename);
            printf(" * Kernel CPU arch %s\n\n", __kernel_arch);

            printl(INFO, "GCC version %s\n", __VERSION__);
            printf(" * Compiled at %s\n\n", __TIMESTAMP__);

            tty_printl(INFO "This project is dedicated to:\n");
            printf(
                " * My teachers [\n\r\t%s,\n\r\t%s,\n\r\t%s,\n\r\t%s\n\r   ]\n",
                "Hugo Luna", "Pablo Zelaya", "Sergio Scoleri", "Daniel Palacios"
            );
            tty_printl("\033[96;40m * And my school friends :D \033[0m \n\r");

        } else if (strcmp("CHARS", input) == 0) {
            tty_probe();

        } else if (strcmp("HEAP", input) == 0) {
            memoryGetStatus();

        } else if (strcmp("BOOT", input) == 0) {
            dumpMultiboot();

        } else if (strcmp("VIDEO", input) == 0) {
            printl(INFO, "Switching to video mode...");
            timerSleep(150);
            initializeVGA(video_mode);

            gfx_clear(PX_BLACK); timerSleep(20);
            gfx_clear(PX_GREEN); timerSleep(20);
            gfx_clear(PX_CYAN); timerSleep(20);
            gfx_clear(PX_RED); timerSleep(20);
            gfx_clear(PX_MAGENTA); timerSleep(20);
            gfx_clear(PX_BROWN); timerSleep(20);
            gfx_clear(PX_BLUE); timerSleep(20);

            gfx_probe();
            gfx_plots("- Two of the most famous products of Berkeley are LSD and Unix.\n\rI don't think that this is a coincidence ...", 8, 72, 0x10 | 0x0F);
            gfx_plots("- Installing software on Windows is like opening a surprise box:\n\rYou never know what else comes bundled with the program you're installing", 8, 96, 0x10 | 0x0F);
            gfx_plots("- MacOS users: convinced that paying a premium price for a computer\n\rmeans never having to admit that right-clicking might actually be useful", 8, 120, 0x10 | 0x0F);

            gfx_plotr(PX_LTGREEN, 16, 152, 128, 32, true); // rectangle
            gfx_plotl(PX_LTGREEN, 16, 216, 144, 216); // horizontal line
            gfx_plotl(PX_LTGREEN, 16, 248, 144, 280); // diagonal line

            gfx_ploto(PX_LTRED, 80, 328, 32, false);
            gfx_ploto(PX_LTRED, 80, 408, 32, true);

            // vertical line
            gfx_plotl(PX_LTMAGENTA, 176, 152, 176, 440);


        } else if (strcmp(input, "NOISE") == 0) {
            printl(INFO, "Switching to video mode...");
            timerSleep(200);
            initializeVGA(video_mode);
            gfx_clear(PX_WHITE);

            uint32_t counter = 0;
            while ((counter++) < 42949600) {
                uint16_t x = (rng_next() & 0x7FFFFFFF) % GRAPHMODE_WIDTH;
                uint16_t y = (rng_next() & 0x7FFFFFFF) % GRAPHMODE_HEIGHT;

                uint8_t c = (rng_next() & 0x7FFFFFFF) % 15;
                gfx_plotp(c, x, y);
                gfx_plotp(c, x + 1, y + 1);
                gfx_plotp(c, x + 1, y);
                gfx_plotp(c, x, y + 1);
            }


        } else if (strcmp(input, "GRAPHICS") == 0) {
            printl(INFO, "Starting graphics demo...");
            timerSleep(100);
            initializeVGA(video_mode);
            gfx_clear(PX_BLACK);

            bglPlayDemo();

            timerSleep(100);
            gfx_clear(PX_BLACK);

            bglPlayDemoEx();

            timerSleep(100);
            gfx_clear(PX_BLACK);

            bglPlayTextDemo();

            timerSleep(100);
            initializeVGA(text_mode);
            tty_clear(NULL);


        } else if (strcmp(input, "WALLPAPER") == 0) {
            printl(INFO, "Switching to video mode...");
            timerSleep(200);
            initializeVGA(video_mode);
            gfx_clear(PX_BLACK);
            timerSleep(200);

            gfx_clear(PX_BLACK);
            gfx_draw(bigeye_480, 80, 0, 480, 480);
            timerSleep(1024);

            gfx_clear(PX_BLACK);
            gfx_draw(myfall_480, 80, 0, 480, 480);
            timerSleep(1024);

            gfx_clear(PX_BLACK);
            gfx_draw(myfood_480, 80, 0, 480, 480);
            timerSleep(1024);

            gfx_clear(PX_BLACK);
            gfx_draw(mylamb_480, 80, 0, 480, 480);
            timerSleep(1024);

            gfx_clear(PX_BLACK);
            gfx_draw(mylife_480, 80, 0, 480, 480);
            timerSleep(1024);

            gfx_clear(PX_BLACK);
            gfx_draw(mymind_480, 80, 0, 480, 480);
            timerSleep(1024);

            gfx_clear(PX_BLACK);
            gfx_draw(theman_480, 80, 0, 480, 480);
            timerSleep(1024);


            gfx_clear(PX_BLACK);
            gfx_draw(candle_640, 0, 0, 640, 480);
            timerSleep(1024);

            gfx_clear(PX_BLACK);
            gfx_draw(choice_640, 0, 0, 640, 480);
            timerSleep(1024);

            gfx_clear(PX_BLACK);
            gfx_draw(clouds_640, 0, 0, 640, 480);
            timerSleep(1024);

            gfx_clear(PX_BLACK);
            gfx_draw(myhill_640, 0, 0, 640, 480);
            timerSleep(1024);

            gfx_clear(PX_BLACK);
            gfx_draw(mypain_640, 0, 0, 640, 480);
            timerSleep(1024);

            gfx_clear(PX_BLACK);
            gfx_draw(mypath_640, 0, 0, 640, 480);
            timerSleep(1024);

            gfx_clear(PX_BLACK);
            gfx_draw(myroad_640, 0, 0, 640, 480);
            timerSleep(1024);

            initializeVGA(text_mode);
            tty_clear(NULL);


        } else if (strcmp(input, "CPUID") == 0) {
            processorGetStatus();


        } else if (strcmp(input, "TEST") == 0) {
            printl(LINE, "%-11s -> (-0.114784) = %f\n",  "[sin(69) rad ]", SIN(69));
            printl(LINE, "%-11s -> ( 0.993390) = %f\n",  "[cos(69) rad ]", COS(69));
            printl(LINE, "%-11s -> (-0.115548) = %f\n\n", "[tan(69) rad ]", TAN(69));

            printl(LINE, "%-11s -> ( 0.933580) = %f\n",   "[sin(69) deg ]", SIN(69 * (PI / 180.0)));
            printl(LINE, "%-11s -> ( 0.358367) = %f\n",   "[cos(69) deg ]", COS(69 * (PI / 180.0)));
            printl(LINE, "%-11s -> ( 2.605089) = %f\n\n", "[tan(69) deg ]", TAN(69 * (PI / 180.0)));

            printl(LINE, "%-11s -> ( 484416  ) = %d\n",   "[sqr(696)    ]", SQR(696));
            printl(LINE, "%-11s -> ( 264.001 ) = %f\n\n", "[sqrt(69697) ]", sqrt(69697));

            printl(LINE, "%-11s -> ( 2.00000 ) = %f\n",   "[ceil(1.3)   ]", CEIL(1.3));
            printl(LINE, "%-11s -> (-4.00000 ) = %f\n\n", "[floor(-3.14)]", FLOOR(-3.14));

            printl(LINE, "%-11s -> ( 150.792 ) = %f\n\n", "[3.1415*2*4*6]", getMath("3.1415*2*4*6", 0));

            // Simple console output
            printf("Hello, world!\n");

            puts("Bye, world!");

            // Formatted output with values
            printf("Integer: %d, String: %s\n", 42, "test");

            // Using serial output
            fprintf(serial, "Serial test: %s\n", "This is a TEST!");

            // Storing formatted output in a buffer
            char mybuffer[100];
            sprintf(mybuffer, "Value: %d\n", 42);

            // Safe buffer formatting with size limit
            snprintf(mybuffer, sizeof(mybuffer), "A very long string that might overflow...\n");


        } else if (strcmp(input, "BUGBUG") == 0) {
            printl(FAIL, "*** OH HOLY SHIT! ***");
            timerSleep(100);
            for (uint8_t i = 0; i < 7; i++) {
                printf("%s", itoa(2 % i));
            }


        } else if (strcmp(input, "RAND") == 0 || strcmp(input, "URAND") == 0) {
            for (uint8_t i = 0; i < 7; i++) {
                printf(" %s\n",
                strcmp(input, "RAND") == 0 ?
                    itoa(rng_next()):              // For RAND: use full unsigned range
                    itoa(rng_next() & 0x7FFFFFFF)  // For URAND: mask to positive range
                );
            }


        } else if (strcmp(input, "HELP") == 0) {
            tty_printl(INFO "Availible commands:\n");
            printf(" * %-15s -> %s\n", "HALT",          "Halt the CPU and stop the system");
            printf(" * %-15s -> %s\n", "SHUTDOWN",      "Perform a system shutdown process");
            printf(" * %-15s -> %s\n", "REBOOT",        "Perform a system reboot process");
            printf(" * %-15s -> %s\n", "CLS or CLEAR",  "Clean up the current screen content");
            printf(" * %-15s -> %s\n", "CLOCK",         "Get and display the current time");
            printf(" * %-15s -> %s\n", "TIMER",         "Get and display the internal counter");
            printf(" * %-15s -> %s\n", "RAND or URAND", "Generate signed or unsigned random values");
            printf(" * %-15s -> %s\n", "CALC",          "Perform mathematical operations (+ - * /)");
            printf(" * %-15s -> %s\n", "BEEP",          "Reads and play a set of sound frequencies");
            printf(" * %-15s -> %s\n", "INFO",          "Get and display kernel version information");
            printf(" * %-15s -> %s\n", "CHARS",         "Get and print all the available characters");
            printf(" * %-15s -> %s\n", "HEAP",          "Query and display the heap information");
            printf(" * %-15s -> %s\n", "CPUID",         "Query and display the CPU information");
            printf(" * %-15s -> %s\n", "BUG",           "Throw a handled kernel exception");
            printf(" * %-15s -> %s\n", "BUGBUG",        "Throw a fatal handled kernel exception");


        } else if (strncmp(input, "MKDIR ", 6) == 0) {
            const char *path = input + 6;
            char *slash = strrchr(path, '/');

            if (slash) {
                char parentPath[MAX_NAME_LEN];
                strncpy(parentPath, path, slash - path + 1);
                parentPath[slash - path] = '\0';

                Directory *parent = bfsFindDirectory(parentPath);

                if (parent) {
                    if (bfsCheckName(slash + 1)) {
                        bfsCreateDirectory(parent, slash + 1);
                    } else {
                        printl(FAIL, "Invalid directory name\n\r");
                    }
                } else {
                    printl(FAIL, "Cannot find the parent directory\n\r");
                }

            } else {

                if (bfsCheckName(path)) {
                    bfsCreateDirectory(BFS_CURRENT_DIR, path);
                } else {
                    printl(FAIL, "Invalid directory name\n\r");
                }
            }


        } else if (strncmp(input, "TOUCH ", 6) == 0) {
            const char *path = input + 6;
            char *slash = strrchr(path, '/');

            if (slash) {
                char parentPath[MAX_NAME_LEN];
                strncpy(parentPath, path, slash - path + 1);
                parentPath[slash - path] = '\0';

                Directory *parent = bfsFindDirectory(parentPath);

                if (parent) {
                    if (bfsCheckName(slash + 1)) {
                        bfsCreateFile(parent, slash + 1);
                    } else {
                        printl(FAIL, "Invalid file name\n\r");
                    }
                } else {
                    printl(FAIL, "Cannot find the parent directory\n\r");
                }

            } else {
                if (bfsCheckName(path)) {
                    bfsCreateFile(BFS_CURRENT_DIR, path);
                } else {
                    printl(FAIL, "Invalid file name\n\r");
                }
            }


        } else if (strncmp(input, "RM ", 3) == 0) {
            const char *path = input + 3;
            char *slash = strrchr(path, '/');
            Directory *directory;

            if (slash) {
                char dirPath[MAX_NAME_LEN];
                strncpy(dirPath, path, (slash - path + 1));
                dirPath[slash - path] = '\0';
                directory = bfsFindDirectory(dirPath);
                path = slash + 1;
            } else {
                directory = BFS_CURRENT_DIR;
            }

            if (directory) {
                File *file = directory->files;
                while (file && strcmp(file->name, path) != 0) {
                    file = file->next;
                }

                if (file) {
                    bfsRemoveFile(directory, file->name);
                    printl(INFO, "File removed successfully\n\r");
                } else {
                    printl(FAIL, "Cannot find the file specified\n\r");
                }
            } else {
                printl(FAIL, "Cannot find the directory specified\n\r");
            }


        } else if (strncmp(input, "RMDIR ", 6) == 0) {
            Directory *directory = bfsFindDirectory(input + 6);

            if (directory) {

                /* Just in case ...*/
                if (directory == BFS_PRIMARY_DIR) {
                    printl(FAIL, "Cannot remove root directory\n\r");
                    return;
                }

                if (directory->parent) {
                    Directory *parent = directory->parent;
                    Directory *prev = NULL;
                    Directory *subdir = parent->subdirs;

                    // Search in the parent subdirectory
                    while (subdir && subdir != directory) {
                        prev = subdir;
                        subdir = subdir->next;
                    }

                    // If found ...
                    if (subdir) {
                        if (prev) {
                            prev->next = subdir->next;
                        } else {
                            parent->subdirs = subdir->next;
                        }

                        // we remove it
                        bfsRemoveDirectory(subdir);
                        printl(INFO, "Directory removed successfully\n\r");

                    } else {
                        printl(FAIL, "Cannot find the directory in parent\n\r");
                    }
                } else {
                    printl(FAIL, "The directory has no parent\n\r");
                }
            } else {
                printl(FAIL, "Cannot find the directory specified\n\r");
            }


        } else if (strncmp(input, "WRITE ", 6) == 0) {
            const char *filename = input + 6;
            char *content = strchr(filename, ' ');

            if (content) {
                *content = '\0';
                content++;

                File *file = bfsFindFile(filename);
                if (file) {
                    bfsWriteFile(file, content);
                    printl(INFO, "File written successfully\n\r");
                } else {
                    printl(FAIL, "Cannot find the file specified\n\r");
                }
            } else {
                printl(FAIL, "Invalid command format\n\r");
            }


        } else if (strncmp(input, "CAT ", 4) == 0) {
            File *file = bfsFindFile(input + 4);
            if (file) {
                bfsReadFile(file);
            } else {
                printl(FAIL, "Cannot find the file specified\n\r");
            }


        } else if (strncmp(input, "LS", 2) == 0) {
            const char *path = input[2] == '\0' ? NULL : input + 4;
            Directory *directory = path ? bfsFindDirectory(path) : BFS_CURRENT_DIR;

            if (!directory) {
                printl(FAIL, "Cannot find the directory specified\n\r");
                return;
            }

            printl(INFO, "Contents of directory <%s>\r\n", directory->name);

            Directory *subdir = directory->subdirs;
            while (subdir) {
                printl("\033[93;40m\t[/] \033[0m", "/%s\r\n", subdir->name);
                subdir = subdir->next;
            }

            File *file = directory->files;
            while (file) {
                printl("\033[33;40m\t *  \033[0m", "%s {%d bytes}\r\n", file->name, file->size);
                file = file->next;
            }


        } else if (strncmp(input, "CD ", 3) == 0) {
            const char *targetPath = input + 3;

            if (strcmp(targetPath, "..") == 0) {
                if (BFS_CURRENT_DIR->parent) {
                    BFS_CURRENT_DIR = BFS_CURRENT_DIR->parent;
                }

            } else {
                Directory *directory = bfsFindDirectory(targetPath);
                if (directory) {
                    BFS_CURRENT_DIR = directory;
                } else {
                    printl(FAIL, "Cannot find the directory specified\n\r");
                }
            }


        } else if (strcmp(input, "TREE") == 0) {
            bfsPrintTree(BFS_PRIMARY_DIR, 0);


        } else if (strcmp(input, "PWD") == 0) {
            printl(INFO, "Current working directory is <%s>\r\n", BFS_CURRENT_DIR->name);


        } else if (strncmp(input, "CP ", 3) == 0) {
            const char *sourcePath = input + 3;
            const char *targetPath = strchr(sourcePath, ' ');

            if (targetPath) {
                char source[MAX_NAME_LEN];
                char target[MAX_NAME_LEN];

                strncpy(source, sourcePath, (targetPath - sourcePath + 1));
                source[targetPath - sourcePath] = '\0';
                targetPath++;
                strcpy(target, targetPath);

                File *file = bfsFindFile(source);
                Directory *directory = bfsFindDirectory(target);

                if (file && directory) {
                    bfsCopyFile(file, directory, NULL);
                    tty_printl(INFO "File copied successfully\n\r");
                } else {
                    tty_printl(FAIL "Cannot find the source file or destination directory\n\r");
                }
            } else {
                tty_printl(FAIL "Invalid command format\n\r");
            }

        } else {
            // We dont validate empty buffers :)
            if (strlen(input) != 0) {
                playBeep(600, 32);
                THROW("Command not found!");
            }
        }

    }
}
