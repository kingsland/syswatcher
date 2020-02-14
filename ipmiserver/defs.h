#ifndef DEFS_H
#define DEFS_H
#define COLOR_NONE      "\e[0m"
#define COLOR_L_RED     "\e[1;31m"
#define COLOR_L_GREEN   "\e[1;32m"
#define COLOR_L_GRAY    "\e[1;37m"
#ifdef PRINT_DEBUG
#define print_err(fmt,...)    fprintf(stderr, "%s %d: "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define print_info(fmt,...)    fprintf(stdout, "%s %d: "fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define print_err(fmt,...)    fprintf(stderr, "%s %d: "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define print_info(fmt,...)
#endif
#define print_red(fmt,...)    printf(COLOR_L_RED"%s %d: " fmt COLOR_NONE"\n",  \
                                __func__, __LINE__, ##__VA_ARGS__)
#define print_green(fmt,...)    printf(COLOR_L_GREEN"%s %d: " fmt COLOR_NONE"\n",  \
                                __func__, __LINE__, ##__VA_ARGS__)
#define print_gray(fmt,...)    printf(COLOR_L_GRAY"%s %d: " fmt COLOR_NONE"\n",  \
                                __func__, __LINE__, ##__VA_ARGS__)

#endif  //end of DEFS_H
