/*
 * pgnodemx
 *
 * SQL functions that allow capture of node OS metrics from PostgreSQL
 * Joe Conway <joe@crunchydata.com>
 *
 * This code is released under the PostgreSQL license.
 *
 * Copyright 2020 Crunchy Data Solutions, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without a written
 * agreement is hereby granted, provided that the above copyright notice
 * and this paragraph and the following two paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL CRUNCHY DATA SOLUTIONS, INC. BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,
 * INCLUDING LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
 * DOCUMENTATION, EVEN IF THE CRUNCHY DATA SOLUTIONS, INC. HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE CRUNCHY DATA SOLUTIONS, INC. SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE CRUNCHY DATA SOLUTIONS, INC. HAS NO
 * OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 */

#include "postgres.h"

#include <linux/magic.h>
#include <sys/vfs.h>
#include <unistd.h>

#include "catalog/pg_authid.h"
#include "catalog/pg_type_d.h"
#include "fmgr.h"
#include "funcapi.h"
#include "lib/qunique.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "port.h"
#include "storage/fd.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/guc_tables.h"
#include "utils/int8.h"
#include "utils/memutils.h"
#include "utils/varlena.h"

#include "cgroup.h"

PG_MODULE_MAGIC;

void _PG_init(void);
extern Datum pgnodemx_cgroup_mode(PG_FUNCTION_ARGS);
extern Datum pgnodemx_cgroup_path(PG_FUNCTION_ARGS);
extern Datum pgnodemx_cgroup_process_count(PG_FUNCTION_ARGS);
extern Datum pgnodemx_memory_pressure(PG_FUNCTION_ARGS);
extern Datum pgnodemx_memstat_int64(PG_FUNCTION_ARGS);
extern Datum pgnodemx_keyed_memstat_int64(PG_FUNCTION_ARGS);

/*
 * Entrypoint of this module.
 */
void
_PG_init(void)
{
	/* Be sure we do initialization only once */
	static bool inited = false;

	if (inited)
		return;

	/* Must be loaded with shared_preload_libraries */
	if (!process_shared_preload_libraries_in_progress)
		ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						errmsg("pgnodemx: must be loaded via shared_preload_libraries")));

	DefineCustomBoolVariable("pgnodemx.containerized",
							 "True if operating inside a container",
							 NULL, &containerized, false, PGC_POSTMASTER /* PGC_SIGHUP */,
							 0, NULL, NULL, NULL);

	DefineCustomStringVariable("pgnodemx.cgrouproot",
							   "Path to root cgroup",
							   NULL, &cgrouproot, "/sys/fs/cgroup", PGC_POSTMASTER /* PGC_SIGHUP */,
							   0, NULL, NULL, NULL);

	set_cgmode();

	/* must determine if containerized before setting cgpath */
	set_containerized();
	set_cgpath();

    inited = true;
}

PG_FUNCTION_INFO_V1(pgnodemx_cgroup_mode);
Datum
pgnodemx_cgroup_mode(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(cstring_to_text(cgmode));
}

PG_FUNCTION_INFO_V1(pgnodemx_cgroup_path);
Datum
pgnodemx_cgroup_path(PG_FUNCTION_ARGS)
{
	char ***values;
	int		nrow = cgpath->nkvp;
	int		ncol = 2;
	int		i;

	values = (char ***) palloc(nrow * sizeof(char **));
	for (i = 0; i < nrow; ++i)
	{
		values[i] = (char **) palloc(ncol * sizeof(char *));

		values[i][0] = pstrdup(cgpath->keys[i]);
		values[i][1] = pstrdup(cgpath->values[i]);
	}

	return form_srf(fcinfo, values, nrow, ncol, cgpath_sig);
}

PG_FUNCTION_INFO_V1(pgnodemx_cgroup_process_count);
Datum
pgnodemx_cgroup_process_count(PG_FUNCTION_ARGS)
{
	/* cgmembers returns pid count */
	PG_RETURN_INT32(cgmembers(NULL));
}

PG_FUNCTION_INFO_V1(pgnodemx_memory_pressure);
Datum
pgnodemx_memory_pressure(PG_FUNCTION_ARGS)
{
	StringInfo	ftr = makeStringInfo();
	int		nlines;
	char  **lines;

	appendStringInfo(ftr, "%s/%s", get_cgpath_value("memory"), "memory.pressure");
	if (is_cgroup_v1)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				errmsg("pgnodemx: file %s not supported under cgroup v1", ftr->data)));

	lines = read_nlsv(ftr->data, &nlines);
	if (nlines > 0)
	{
		char			 ***values;
		int					nrow = nlines;
		int					ncol;
		int					i;
		kvpairs			   *nkl;

		values = (char ***) palloc(nrow * sizeof(char **));
		for (i = 0; i < nrow; ++i)
		{
			int		j;

			nkl = parse_nested_keyed_line(lines[i]);
			ncol = nkl->nkvp;
			if (ncol != MEM_PRESS_NCOL)
				ereport(ERROR,
						(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						errmsg("pgnodemx: unexpected format read from %s", ftr->data)));

			values[i] = (char **) palloc(ncol * sizeof(char *));
			for (j = 0; j < ncol; ++j)
				values[i][j] = pstrdup(nkl->values[j]);
		}

		return form_srf(fcinfo, values, nrow, ncol, mem_press_sig);
	}

	return (Datum) 0;
}

PG_FUNCTION_INFO_V1(pgnodemx_cgroup_scalar_bigint);
Datum
pgnodemx_cgroup_scalar_bigint(PG_FUNCTION_ARGS)
{
	StringInfo	ftr = makeStringInfo();
	char	   *fname = convert_and_check_filename(PG_GETARG_TEXT_PP(0));
	char	   *p = strchr(fname, '.');
	Size		len;
	char	   *controller;

	if (!p)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				errmsg("pgnodemx: missing \".\" in filename %s", PROC_CGROUP_FILE)));

	len = (p - fname);
	controller = pnstrdup(fname, len);
	appendStringInfo(ftr, "%s/%s", get_cgpath_value(controller), fname);

	PG_RETURN_INT64(getInt64FromFile(ftr->data));
}

PG_FUNCTION_INFO_V1(pgnodemx_keyed_memstat_int64);
Datum
pgnodemx_keyed_memstat_int64(PG_FUNCTION_ARGS)
{
	StringInfo	ftr = makeStringInfo();
	char	   *fname = convert_and_check_filename(PG_GETARG_TEXT_PP(0));
	int			nlines;
	char	  **lines;

	appendStringInfo(ftr, "%s/%s", get_cgpath_value("memory"), fname);

	lines = read_nlsv(ftr->data, &nlines);
	if (nlines > 0)
	{
		char	 ***values;
		int			nrow = nlines;
		int			ncol = 2;
		int			i;
		char	  **fkl;

		values = (char ***) palloc(nrow * sizeof(char **));
		for (i = 0; i < nrow; ++i)
		{
			fkl = parse_flat_keyed_line(lines[i]);

			values[i] = (char **) palloc(ncol * sizeof(char *));
			values[i][0] = pstrdup(fkl[0]);
			values[i][1] = pstrdup(fkl[1]);
		}

		return form_srf(fcinfo, values, nrow, ncol, flat_keyed_int64_sig);
	}

	return (Datum) 0;
}

