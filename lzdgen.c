/*
 * lzdgen - LZ data generator example
 *
 * Copyright 2016 Joergen Ibsen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define _POSIX_C_SOURCE 200809L

#if defined(_MSC_VER)
#  define _CRT_NONSTDC_NO_DEPRECATE
#  define _CRT_SECURE_NO_WARNINGS
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#  include <io.h>
#else
#  include <unistd.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lzdatagen.h"
#include "parg.h"
#include "pcg_basic.h"

#define EXE_NAME "lzdgen"

#define BLOCK_SIZE (1024 * 1024)

/* Wrapper around strtoull to parse size suffixes */
static unsigned long long
strtosize(const char *s, char **endptr, int base)
{
	char *ep = NULL;
	unsigned long long v;

	if (endptr != NULL) {
		*endptr = (char *) s;
	}

	errno = 0;

	v = strtoull(s, &ep, base);

	if (v == ULLONG_MAX && errno == ERANGE) {
		ep = (char *) s;
	}

	switch (tolower(*ep)) {
	case 'k':
		if (v >= ULLONG_MAX / 1024ULL) {
			return ULLONG_MAX;
		}
		v *= 1024ULL;
		++ep;
		break;
	case 'm':
		if (v >= ULLONG_MAX / (1024 * 1024ULL)) {
			return ULLONG_MAX;
		}
		v *= 1024 * 1024ULL;
		++ep;
		break;
	case 'g':
		if (v >= ULLONG_MAX / (1024 * 1024 * 1024ULL)) {
			return ULLONG_MAX;
		}
		v *= 1024 * 1024 * 1024ULL;
		++ep;
		break;
	case 't':
		if (v >= ULLONG_MAX / (1024 * 1024 * 1024 * 1024ULL)) {
			return ULLONG_MAX;
		}
		v *= 1024 * 1024 * 1024 * 1024ULL;
		++ep;
		break;
	default:
		break;
	}

	if (endptr != NULL) {
		*endptr = ep;
	}

	return v;
}

static void
printf_error(const char *fmt, ...)
{
	va_list arg;

	fprintf(stderr, EXE_NAME ": ");

	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);

	fprintf(
	    stderr,
	    "\n"
	    "usage: " EXE_NAME " [-fhVv] [-l EXP] [-m EXP] [-r RATIO] [-S SEED]\n"
	    "              [-s SIZE] OUTFILE\n");
}

static void
print_help(void)
{
	printf(
	    "usage: " EXE_NAME " [options] OUTFILE\n"
	    "\n"
	    "Generate compressible data for testing purposes.\n"
	    "\n"
	    "options:\n"
	    "  -f, --force            overwrite output file\n"
	    "  -h, --help             print this help and exit\n"
	    "  -l, --literal-exp EXP  literal distribution exponent [3.0]\n"
	    "  -m, --match-exp EXP    match length distribution exponent [3.0]\n"
	    "  -o, --output OUTFILE   write output to OUTFILE\n"
	    "  -r, --ratio RATIO      compression ratio target [3.0]\n"
	    "  -S, --seed SEED        use 64-bit SEED to seed PRNG\n"
	    "  -s, --size SIZE        size with opt. k/m/g suffix [1m]\n"
	    "  -V, --version          print version and exit\n"
	    "  -v, --verbose          verbose mode\n"
	    "\n"
	    "If OUTFILE is `-', write to standard output.\n");
}

static void
print_version(void)
{
	printf(
	    EXE_NAME " " LZDG_VER_STRING "\n"
	    "\n"
	    "Copyright 2016 Joergen Ibsen\n"
	    "\n"
	    "Licensed under the Apache License, Version 2.0.\n"
	    "There is NO WARRANTY, to the extent permitted by law.\n");
}

int
main(int argc, char *argv[])
{
	struct parg_state ps;
	double ratio = 3.0;
	double len_exp = 3.0;
	double lit_exp = 3.0;
	unsigned char *buffer = NULL;
	const char *outfile = NULL;
	FILE *fp = NULL;
	uint64_t seed;
	size_t size = 1024 * 1024;
	size_t offs = 0;
	int flag_force = 0;
	int flag_verbose = 0;
	int retval = EXIT_FAILURE;
	int c;

	const struct parg_option long_options[] = {
		{ "force", PARG_NOARG, NULL, 'f' },
		{ "help", PARG_NOARG, NULL, 'h' },
		{ "literal-exp", PARG_REQARG, NULL, 'l' },
		{ "match-exp", PARG_REQARG, NULL, 'm' },
		{ "output", PARG_REQARG, NULL, 'o' },
		{ "ratio", PARG_REQARG, NULL, 'r' },
		{ "seed", PARG_REQARG, NULL, 'S' },
		{ "size", PARG_REQARG, NULL, 's' },
		{ "version", PARG_NOARG, NULL, 'V' },
		{ "verbose", PARG_NOARG, NULL, 'v' },
		{ 0, 0, 0, 0 }
	};

	seed = time(NULL) ^ (intptr_t) &printf;

	parg_init(&ps);

	while ((c = parg_getopt_long(&ps, argc, argv, "fl:m:o:r:S:s:hVv", long_options, NULL)) != -1) {
		switch (c) {
		case 1:
		case 'o':
			if (outfile == NULL) {
				outfile = ps.optarg;
			}
			else {
				printf_error("too many arguments");
				return EXIT_FAILURE;
			}
			break;
		case 'f':
			flag_force = 1;
			break;
		case 'l':
			{
				char *ep = NULL;
				double v;

				errno = 0;

				v = strtod(ps.optarg, &ep);

				if (ep == ps.optarg || *ep != '\0' || errno == ERANGE) {
					printf_error("literal exponent must be a floating point value");
					return EXIT_FAILURE;
				}

				lit_exp = v;
			}
			break;
		case 'm':
			{
				char *ep = NULL;
				double v;

				errno = 0;

				v = strtod(ps.optarg, &ep);

				if (ep == ps.optarg || *ep != '\0' || errno == ERANGE) {
					printf_error("match exponent must be a floating point value");
					return EXIT_FAILURE;
				}

				len_exp = v;
			}
			break;
		case 'r':
			{
				char *ep = NULL;
				double v;

				errno = 0;

				v = strtod(ps.optarg, &ep);

				if (ep == ps.optarg || *ep != '\0' || errno == ERANGE || v < 1.0) {
					printf_error("ratio must be a floating point value >= 1.0");
					return EXIT_FAILURE;
				}

				ratio = v;
			}
			break;
		case 'S':
			{
				char *ep = NULL;
				uint64_t n;

				n = strtoull(ps.optarg, &ep, 0);

				if (ep == ps.optarg || *ep != '\0' || errno == ERANGE) {
					printf_error("seed value error");
					return EXIT_FAILURE;
				}

				seed = n;
			}
			break;
		case 's':
			{
				char *ep = NULL;
				size_t n;

				n = strtosize(ps.optarg, &ep, 0);

				if (ep == ps.optarg || *ep != '\0' || n == 0) {
					printf_error("size must be a positive integer");
					return EXIT_FAILURE;
				}

				size = n;
			}
			break;
		case 'h':
			print_help();
			return EXIT_SUCCESS;
			break;
		case 'V':
			print_version();
			return EXIT_SUCCESS;
			break;
		case 'v':
			flag_verbose++;
			break;
		default:
			printf_error("option error at `%s'", argv[ps.optind - 1]);
			return EXIT_FAILURE;
			break;
		}
	}

	if (outfile == NULL) {
		printf_error("too few arguments");
		return EXIT_FAILURE;
	}

	if (strcmp(outfile, "-") == 0) {
#if defined(_WIN32) || defined(__CYGWIN__)
		if (setmode(fileno(stdout), O_BINARY) == -1) {
			perror(EXE_NAME ": unable to set binary mode");
			goto out;
		}
#endif
		fp = stdout;
	}
	else {
		int fd = -1;

#if defined(_WIN32) || defined(__CYGWIN__)
		fd = open(outfile,
		          O_RDWR | O_CREAT | (flag_force ? O_TRUNC : O_EXCL) | O_BINARY,
		          S_IREAD | S_IWRITE);
#else
		fd = open(outfile,
		          O_RDWR | O_CREAT | (flag_force ? O_TRUNC : O_EXCL),
		          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif

		if (fd < 0) {
			perror(EXE_NAME ": unable to open output file");
			goto out;
		}

		fp = fdopen(fd, "wb");

		if (fp == NULL) {
			close(fd);
			perror(EXE_NAME ": unable to open output file");
			goto out;
		}
	}

	pcg32_srandom(seed, 0xC0FFEE);

	if (flag_verbose > 0) {
		fprintf(stderr, EXE_NAME ": seed 0x%016" PRIX64 "\n", seed);
	}

	buffer = malloc(BLOCK_SIZE);

	if (buffer == NULL) {
		perror(EXE_NAME ": unable to allocate buffer");
		goto out;
	}

	while (offs < size) {
		size_t num = size - offs > BLOCK_SIZE ? BLOCK_SIZE : size - offs;

		lzdg_generate_data(buffer, num, ratio, len_exp, lit_exp);

		if (fwrite(buffer, 1, num, fp) != num) {
			perror(EXE_NAME ": write error");
			goto out;
		}

		offs += num;
	}

	retval = EXIT_SUCCESS;

out:
	if (fp != NULL) {
		fclose(fp);
	}

	free(buffer);
	buffer = NULL;

	return retval;
}
