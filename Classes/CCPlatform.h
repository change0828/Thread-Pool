
#if defined(__GNUC__) && (__GNUC__ >= 4)
#define CC_FORMAT_PRINTF(formatPos, argPos) __attribute__((__format__(printf, formatPos, argPos)))
#elif defined(__has_attribute)
#if __has_attribute(format)
#define CC_FORMAT_PRINTF(formatPos, argPos) __attribute__((__format__(printf, formatPos, argPos)))
#endif // __has_attribute(format)
#else
#define CC_FORMAT_PRINTF(formatPos, argPos)
#endif

/**
@brief Output Debug message.
*/
void log(const char * format, ...) CC_FORMAT_PRINTF(1, 2);

#define NET_LOG(format, ...) log(format, ##__VA_ARGS__)