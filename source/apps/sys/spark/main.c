/** Entry point for the SPARK service. */
#include "spark.h"

int main(int argc, char **argv) {
    Spark spark;
    unused(argc);
    unused(argv);
    Spark_init(&spark);
    return spark.run(&spark);
}
