#include "lstage.hpp"
#include <getopt.h>

static int stage__main(lua_State *L)
{
    return 0;
}

int main(int argc, char *argv[])
{
    lua__Stage L(NULL);

    static struct option long_options[] = {
            {NULL,      0,                 NULL, 0}
    };

    int r;
    do {
        int option_index = 0;

        switch (r = getopt_long(argc, argv, "c:l:s:vh?", long_options, &option_index))
        {
            case -1: break;
            case 'v': _exit(fprintf(stderr, "%s BUILD [%s %s]\n", argv[0], __DATE__, __TIME__));

            default : _exit(fprintf(stderr, "%s: invalid option -- '%c'\n"
                                            "Try '%s --help' for more information.\n", argv[0], r, argv[0]));
            case 'h':
            {
                fprintf(stderr,
                        "Usage: %s [OPTION]... [arguments] \n"
                        "\n"
                        "Mandatory arguments to long options are mandatory for short options too.\n"
                        "      --help                              display this help and exit\n"
                        "      --version                           output version information and exit\n"
                        "\n", argv[0]);
            } _exit(0);
        }
    } while (r != -1);
    {
        const char *arg = argv[0];

        lua_newtable(*L);
        for (int __i = 0; optind <= argc; arg = argv[++optind])
        {
            lua_pushstring(*L, arg);
            lua_rawseti(*L, -2, ++__i);
        }
    } lua_setglobal(*L, "arg");
    if (lua_cpcall(*L, stage__main, NULL) && (lua_isnil(*L, -1) == false))
    {
        const char *msg = lua_tostring(*L, -1);
        if (msg == NULL)
            msg = "(error object is not a string)";

        fprintf(stderr, "%s: %s\n", argv[0], msg);
        lua_pop(*L, 1);
    }
    return 0;
}