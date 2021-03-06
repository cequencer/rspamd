/*-
 * Copyright 2016 Vsevolod Stakhov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef FUZZY_BACKEND_H_
#define FUZZY_BACKEND_H_

#include "config.h"
#include "fuzzy_storage.h"


struct rspamd_fuzzy_backend;

/**
 * Open fuzzy backend
 * @param path file to open (legacy file will be converted automatically)
 * @param err error pointer
 * @return backend structure or NULL
 */
struct rspamd_fuzzy_backend *rspamd_fuzzy_backend_open (const gchar *path,
		gboolean vacuum,
		GError **err);

/**
 * Check specified fuzzy in the backend
 * @param backend
 * @param cmd
 * @return reply with probability and weight
 */
struct rspamd_fuzzy_reply rspamd_fuzzy_backend_check (
		struct rspamd_fuzzy_backend *backend,
		const struct rspamd_fuzzy_cmd *cmd,
		gint64 expire);

/**
 * Prepare storage for updates (by starting transaction)
 */
gboolean rspamd_fuzzy_backend_prepare_update (struct rspamd_fuzzy_backend *backend,
		const gchar *source);

/**
 * Add digest to the database
 * @param backend
 * @param cmd
 * @return
 */
gboolean rspamd_fuzzy_backend_add (struct rspamd_fuzzy_backend *backend,
		const struct rspamd_fuzzy_cmd *cmd);

/**
 * Delete digest from the database
 * @param backend
 * @param cmd
 * @return
 */
gboolean rspamd_fuzzy_backend_del (
		struct rspamd_fuzzy_backend *backend,
		const struct rspamd_fuzzy_cmd *cmd);

/**
 * Commit updates to storage
 */
gboolean rspamd_fuzzy_backend_finish_update (struct rspamd_fuzzy_backend *backend,
		const gchar *source, gboolean version_bump);

/**
 * Sync storage
 * @param backend
 * @return
 */
gboolean rspamd_fuzzy_backend_sync (struct rspamd_fuzzy_backend *backend,
		gint64 expire,
		gboolean clean_orphaned);

/**
 * Close storage
 * @param backend
 */
void rspamd_fuzzy_backend_close (struct rspamd_fuzzy_backend *backend);

gsize rspamd_fuzzy_backend_count (struct rspamd_fuzzy_backend *backend);
gint rspamd_fuzzy_backend_version (struct rspamd_fuzzy_backend *backend, const gchar *source);
gsize rspamd_fuzzy_backend_expired (struct rspamd_fuzzy_backend *backend);

const gchar * rspamd_fuzzy_backend_id (struct rspamd_fuzzy_backend *backend);

#endif /* FUZZY_BACKEND_H_ */
