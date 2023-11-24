/*
 * lzdatagen - LZ data generator
 *
 * Copyright 2016-2023 Joergen Ibsen
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

#include "lzdatagen.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#include "pcg_basic.h"

/* Limits for length values, based on zlib */
#define MIN_LEN 3
#define MAX_LEN 258
#define NUM_LEN (MAX_LEN - MIN_LEN + 1)

/* Number of lengths to generate at a time */
#define LEN_PER_CHUNK 512

/* Number of literal samples to generate (must be power of 2) */
#define SAMPLE_SIZE 16384U

/* Block size for sampled generation */
#define BLOCK_SIZE (1024UL * 1024)

/**
 * Generate random double.
 *
 * @note Not perfectly distributed, but more than adequate for this use.
 *
 * @return random double in range [0;1)
 */
static double
rand_double(void)
{
	return pcg32_random() / (UINT32_MAX + 1.0);
}

/**
 * Generate random literals.
 *
 * Generate literals following a power distribution. If `lit_exp` is 1.0, the
 * distribution is linear. As `lit_exp` grows, the likelihood of small values
 * increases.
 *
 * @param ptr pointer to where to store literals
 * @param size number of literals to generate
 * @param lit_exp exponent used for distribution
 */
static void
generate_literals_from_distribution(unsigned char *ptr, size_t size, double lit_exp)
{
	size_t i;

	for (i = 0; i < size; ++i) {
		ptr[i] = (unsigned char) (256 * powf((float) rand_double(), (float) lit_exp));
	}
}

/**
 * Generate random literals from `samples`.
 *
 * Generate literals by selecting randomly from `samples`.
 *
 * @param ptr pointer to where to store literals
 * @param size number of literals to generate
 * @param samples pointer to array of SAMPLE_SIZE random literals
 */
static void
generate_literals_from_samples(unsigned char *ptr, size_t size, const unsigned char *samples)
{
	size_t i;

	for (i = 0; i < size; ++i) {
		ptr[i] = samples[pcg32_random() % SAMPLE_SIZE];
	}
}

/**
 * Generate random literals.
 *
 * If `samples` is `NULL` generate literals using `lit_exp`, otherwise
 * select random literals from `samples`.
 *
 * @see generate_literals_from_distribution
 * @see generate_literals_from_samples
 *
 * @param ptr pointer to where to store literals
 * @param size number of literals to generate
 * @param samples pointer to array of SAMPLE_SIZE random literals or NULL
 */
static void
generate_literals(unsigned char *ptr, size_t size, double lit_exp, const unsigned char *samples)
{
	if (samples) {
		generate_literals_from_samples(ptr, size, samples);
	}
	else {
		generate_literals_from_distribution(ptr, size, lit_exp);
	}
}

/**
 * Generate random lengths.
 *
 * Generate length frequencies following a power distribution. If `len_exp` is
 * 1.0, the distribution is linear. As `len_exp` grows, the likelihood of small
 * values increases.
 *
 * @note The size of the frequency table is `NUM_LEN`, the range of possible
 * length values, while `num` is the number of length values to generate.
 *
 * @param len_freq pointer to where to store length frequencies
 * @param num number of lengths to generate
 * @param len_exp exponent used for distribution
 */
static void
generate_lengths(unsigned int len_freq[NUM_LEN], size_t num, double len_exp)
{
	size_t i;

	for (i = 0; i < NUM_LEN; ++i) {
		len_freq[i] = 0;
	}

	for (i = 0; i < num; ++i) {
		size_t len = (size_t) (NUM_LEN * powf((float) rand_double(), (float) len_exp));

		assert(len < NUM_LEN);

		len_freq[len]++;
	}
}

/**
 * Generate compressible data.
 *
 * Internal function that performs the actual data generation.
 *
 * If `samples` is `NULL` generate literals using `lit_exp`, otherwise
 * select random literals from `samples`.
 *
 * @param ptr pointer to where to store generated data
 * @param size number of bytes to generate
 * @param ratio desired compression ratio
 * @param len_exp exponent used for distribution of lenghts
 * @param lit_exp exponent used for distribution of literals
 * @param samples pointer to array of SAMPLE_SIZE random literals or NULL
 */
static void
generate_data_internal(void *ptr, size_t size, double ratio, double len_exp, double lit_exp, const unsigned char *samples)
{
	unsigned int len_freq[NUM_LEN];
	unsigned char buffer[MAX_LEN];
	unsigned char *p = (unsigned char *) ptr;
	size_t cur_len = 0;
	size_t i = 0;
	int last_was_match = 0;

	len_freq[0] = 0;

	while (i < size) {
		size_t len;

		/* Find next length with non-zero frequency */
		while (len_freq[cur_len] == 0) {
			if (cur_len == 0) {
				generate_literals(buffer, MAX_LEN, lit_exp, samples);

				generate_lengths(len_freq, LEN_PER_CHUNK, len_exp);

				cur_len = NUM_LEN;
			}

			cur_len--;
		}

		len = MIN_LEN + cur_len;

		len_freq[cur_len]--;

		if (len > size - i) {
			len = size - i;
		}

		if (rand_double() < 1.0 / ratio) {
			/* Insert len literals */
			generate_literals(p, len, lit_exp, samples);

			last_was_match = 0;
		}
		else {
			/* Insert literal to break up matches */
			if (last_was_match) {
				generate_literals(p, 1, lit_exp, samples);
				i++;
				p++;

				if (len > size - i) {
					len = size - i;
				}
			}

			/* Insert match of length len */
			memcpy(p, buffer, len);

			last_was_match = 1;
		}

		i += len;
		p += len;
	}
}

void
lzdg_generate_data(void *ptr, size_t size, double ratio, double len_exp, double lit_exp)
{
	generate_data_internal(ptr, size, ratio, len_exp, lit_exp, NULL);
}

void
lzdg_generate_data_bulk(void *ptr, size_t size, double ratio, double len_exp, double lit_exp)
{
	unsigned char samples[SAMPLE_SIZE];
	unsigned char *p = (unsigned char *) ptr;
	size_t offs = 0;

	while (offs < size) {
		size_t num = size - offs > BLOCK_SIZE ? BLOCK_SIZE : size - offs;

		/* Fill samples with random literals following the lit_exp distribution */
		generate_literals(samples, SAMPLE_SIZE, lit_exp, NULL);

		generate_data_internal(p + offs, num, ratio, len_exp, lit_exp, samples);

		offs += num;
	}
}
