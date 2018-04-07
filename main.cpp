#include "Motion.hpp"

int main(int argc, char *argv[])
{
    Motion objMotion(argc, argv);
    objMotion.gst_loop();

    return 0;
}
