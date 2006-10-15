char x86 =
#if defined(CONFIG_M386)
3;
#elif defined(CONFIG_M486)
4;
#elif defined(CONFIG_M586)
5;
#elif defined(CONFIG_M686)
6;
#else
#error "CPU type is undefined!"
#endif

