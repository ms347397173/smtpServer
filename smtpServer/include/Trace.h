#ifdef __DEBUG__
#define __TRACE__(...) fprintf(stdout,"file[%s] line[%d] func[%s]",__FILE__,__LINE__,__func__);\
fprintf(stdout,__VA_ARGS__);
#else
#define __TRACE__
#endif
