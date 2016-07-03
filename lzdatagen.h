/*
 * lzdatagen - LZ data generator
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
 */

#ifndef LZDATAGEN_H_INCLUDED
#define LZDATAGEN_H_INCLUDED

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LZDG_VER_MAJOR 0        /**< Major version number */
#define LZDG_VER_MINOR 1        /**< Minor version number */
#define LZDG_VER_PATCH 0        /**< Patch version number */
#define LZDG_VER_STRING "0.1.0" /**< Version number as a string */

void
lzdg_generate_data(void *ptr, size_t size, double ratio, double len_exp, double lit_exp);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LZDATAGEN_H_INCLUDED */
