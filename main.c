#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static void
errx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");

	exit(1);
}


int
main(int __unused, char *__unused[])
{
	printf("herrow herrow\n");

	return (0);
}
