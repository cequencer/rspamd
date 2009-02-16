/*
 * Copyright (c) 2009, Rambler media
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Rambler media ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * OSB tokenizer
 */

#include <sys/types.h>
#include "tokenizers.h"


/* Coefficients that are used for OSB tokenizer */
static const int primes[] = {
	1, 7,
	3, 13,
	5, 29,
	11, 51,
	23, 101,
	47, 203,
	97, 407,
	197, 817,
	397, 1637,
	797, 3277,
};

int
osb_tokenize_text (struct tokenizer *tokenizer, memory_pool_t *pool, f_str_t *input, GTree **tree)
{
	token_node_t *new = NULL;
	f_str_t token = { NULL, 0, 0 };
	uint32_t hashpipe[FEATURE_WINDOW_SIZE], h1, h2;
	int i;

	/* First set all bytes of hashpipe to some common value */
	for (i = 0; i < FEATURE_WINDOW_SIZE; i ++) {
		hashpipe[i] = 0xABCDEF;
	}
	
	if (*tree == NULL) {
		*tree = g_tree_new (token_node_compare_func);
		memory_pool_add_destructor (pool, (pool_destruct_func)g_tree_destroy, *tree);
	}

	msg_debug ("osb_tokenize_text: got input length: %zd", input->len);

	while (tokenizer->get_next_word (input, &token)) {
		/* Shift hashpipe */
		for (i = FEATURE_WINDOW_SIZE - 1; i > 0; i --) {
			hashpipe[i] = hashpipe[i - 1];
		}
		hashpipe[0] = fstrhash (&token);
		
		for (i = 1; i < FEATURE_WINDOW_SIZE; i ++) {
			h1 = hashpipe[0]* primes[0] + hashpipe[i] * primes[i<<1];
		    h2 = hashpipe[0] * primes[1] + hashpipe[i] * primes[(i<<1)-1];
			new = memory_pool_alloc (pool, sizeof (token_node_t));
			new->h1 = h1;
			new->h2 = h2;

			if (g_tree_lookup (*tree, new) == NULL) {
				g_tree_insert (*tree, new, new);
			}
		}
	}

	return TRUE;
}

/*
 * vi:ts=4
 */
