#ifndef OPTIONS_H
#define OPTIONS_H

struct options{
    const char *filename;
    int show_help;
};

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }

extern struct options g_options;

#endif