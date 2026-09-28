#ifndef MONARCH_SYS_SPARK_SPARK_H
#define MONARCH_SYS_SPARK_SPARK_H 1

#include "base/usr/sys.h"
#include "config.h"

typedef struct Spark Spark;

struct Spark {
    long wing_pid;
    long desk_pid;
    long wush_pid;
    long shell_pid;
    int pty[2];
    int status;

    int (*prepare)(Spark *self);
    int (*start)(Spark *self);
    int (*wait)(Spark *self);
    int (*run)(Spark *self);
};

void Spark_init(Spark *self);

#endif /* MONARCH_SYS_SPARK_SPARK_H */
