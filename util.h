/* See LICENSE for licence details. */
/* error functions */
enum loglevel_t {
	DEBUG = 0,
	WARN,
	ERROR,
	FATAL,
};

void logging(enum loglevel_t loglevel, char *format, ...)
{
	va_list arg;
	static const char *loglevel2str[] = {
		[DEBUG] = "DEBUG",
		[WARN]  = "WARN",
		[ERROR] = "ERROR",
		[FATAL] = "FATAL",
	};

	if (loglevel == DEBUG && !VERBOSE)
		return;

	fprintf(stderr, ">>%s<<\t", loglevel2str[loglevel]);
	va_start(arg, format);
	vfprintf(stderr, format, arg);
	va_end(arg);
}

/* wrapper of C functions */
int eopen(const char *path, int flag)
{
	int fd;
	errno = 0;

	if ((fd = open(path, flag)) < 0) {
		logging(ERROR, "couldn't open \"%s\"\n", path);
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
		logging(ERROR, "couldn't open \"%s\"\n", path);
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

int emkstemp(char *template)
{
	int ret;
	errno = 0;

	if ((ret = mkstemp(template)) < 0) {
		logging(ERROR, "couldn't open \"%s\"\n", template);
		logging(ERROR, "mkstemp: %s\n", strerror(errno));
	}

	return ret;
}

int eselect(int max_fd, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *tv)
{
	int ret;
	errno = 0;

	if ((ret = select(max_fd, readfds, writefds, errorfds, tv)) < 0) {
		if (errno == EINTR)
			return eselect(max_fd, readfds, writefds, errorfds, tv);
		else
			logging(ERROR, "select: %s\n", strerror(errno));
	}
	return ret;
}

ssize_t ewrite(int fd, const void *buf, size_t size)
{
	ssize_t ret;
	errno = 0;

	if ((ret = write(fd, buf, size)) < 0) {
		if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
			return ewrite(fd, buf, size);
		else
			logging(ERROR, "write: %s\n", strerror(errno));
	} else if (ret < (ssize_t) size) {
		return ret + ewrite(fd, (char *) buf + ret, size - ret);
	}
	return ret;
}

int esigaction(int signo, struct sigaction *act, struct sigaction *oact)
{
	int ret;
	errno = 0;

	if ((ret = sigaction(signo, act, oact)) < 0)
		logging(ERROR, "sigaction: %s\n", strerror(errno));

	return ret;
}

/* some useful functions */
int str2num(char *str)
{
	if (str == NULL)
		return 0;

	return estrtol(str, NULL, 10);
}
