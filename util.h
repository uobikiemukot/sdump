/* See LICENSE for licence details. */
/* error functions */
/*
void logging(ERROR, char *format, ...)
{
	va_list arg;
	FILE *outfp;

	outfp = (logfp == NULL) ? stderr: logfp;
	fprintf(outfp, ">>ERROR<<\t");
	va_start(arg, format);
	vfprintf(outfp, format, arg);
	va_end(arg);
}

void warn(char *format, ...)
{
	va_list arg;
	FILE *outfp;

	outfp = (logfp == NULL) ? stderr: logfp;
	fprintf(outfp, ">>WARN<<\t");
	va_start(arg, format);
	vfprintf(outfp, format, arg);
	va_end(arg);
}

void debug(char *format, ...)
{
	va_list arg;
	FILE *outfp;

	if (!VERBOSE)
		return;

	outfp = (logfp == NULL) ? stderr: logfp;
	fprintf(outfp, ">>DEBUG<<\t");
	va_start(arg, format);
	vfprintf(outfp, format, arg);
	va_end(arg);
}
*/

enum loglevel {
	DEBUG = 0,
	WARN,
	ERROR,
	FATAL,
};

void logging(int loglevel, char *format, ...)
{
	va_list arg;
	//FILE *outfp;

	static const char *loglevel2str[] = {
		[DEBUG] = "DEBUG",
		[WARN]  = "WARN",
		[ERROR] = "ERROR",
		[FATAL] = "FATAL",
	};

	if (loglevel == DEBUG && !VERBOSE)
		return;

	//outfp = (logfp == NULL) ? stderr: logfp;

	fprintf(stderr, ">>%s<<\t", loglevel2str[loglevel]);
	va_start(arg, format);
	vfprintf(stderr, format, arg);
	va_end(arg);
}

/*
void fatal(char *str)
{
	fprintf(stderr, "%s\n", str);
	exit(EXIT_FAILURE);
}
*/

/* wrapper of C functions */
int eopen(const char *path, int flag)
{
	int fd;
	errno = 0;

	if ((fd = open(path, flag)) < 0) {
		logging(ERROR, "cannot open \"%s\"\n", path);
		logging(ERROR, "open: %s\n", strerror(errno));
	}
	return fd;
}

int eclose(int fd)
{
	int ret = 0;
	errno = 0;

	if ((ret = close(fd)) < 0)
		logging(ERROR, "close: %s\n", strerror(errno));

	return ret;
}

FILE *efopen(const char *path, char *mode)
{
	FILE *fp;
	errno = 0;

	if ((fp = fopen(path, mode)) == NULL) {
		logging(ERROR, "cannot open \"%s\"\n", path);
		logging(ERROR, "fopen: %s\n", strerror(errno));
	}
	return fp;
}

int efclose(FILE *fp)
{
	int ret;
	errno = 0;

	if ((ret = fclose(fp)) < 0)
		logging(ERROR, "fclose: %s\n", strerror(errno));

	return ret;
}

void *emmap(void *addr, size_t len, int prot, int flag, int fd, off_t offset)
{
	uint32_t *fp;
	errno = 0;

	if ((fp = (uint32_t *) mmap(addr, len, prot, flag, fd, offset)) == MAP_FAILED)
		logging(ERROR, "mmap: %s\n", strerror(errno));

	return fp;
}

int emunmap(void *ptr, size_t len)
{
	int ret;
	errno = 0;

	if ((ret = munmap(ptr, len)) < 0)
		logging(ERROR, "munmap: %s\n", strerror(errno));

	return ret;
}

void *ecalloc(size_t nmemb, size_t size)
{
	void *ptr;
	errno = 0;

	if ((ptr = calloc(nmemb, size)) == NULL)
		logging(ERROR, "calloc: %s\n", strerror(errno));

	return ptr;
}

long int estrtol(const char *nptr, char **endptr, int base)
{
	long int ret;
	errno = 0;

	ret = strtol(nptr, endptr, base);
	if (ret == LONG_MIN || ret == LONG_MAX) {
		logging(ERROR, "strtol: %s\n", strerror(errno));
		return 0;
	}

	return ret;
}

/*
int estat(const char *restrict path, struct stat *restrict buf)
{
	int ret;
	errno = 0;

	if ((ret = stat(path, buf)) < 0)
		logging(ERROR, "stat: %s\n", strerror(errno));

	return ret;
}
*/

/* some useful functions */
int str2num(char *str)
{
	if (str == NULL)
		return 0;

	return estrtol(str, NULL, 10);
}

static inline void swapint(int *a, int *b)
{
	int tmp = *a;
	*a  = *b;
	*b  = tmp;
}

static inline int my_ceil(int val, int div)
{
	return (val + div - 1) / div;
}

static inline uint32_t bit_reverse(uint32_t val, int bits)
{
	uint32_t ret = val;
	int shift = bits - 1;

	for (val >>= 1; val; val >>= 1) {
		ret <<= 1;
		ret |= val & 1;
		shift--;
	}

	return ret <<= shift;
}
