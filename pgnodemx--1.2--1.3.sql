/* contrib/pgnodemx/pgnodemx--1.1--1.2.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pgnodemx" to load this file. \quit

CREATE OR REPLACE FUNCTION proc_pid_io(
		OUT pid INTEGER,
		OUT rchar NUMERIC,
		OUT wchar NUMERIC,
		OUT syscr NUMERIC,
		OUT syscw NUMERIC,
		OUT reads NUMERIC,
		OUT writes NUMERIC,
		OUT cwrites NUMERIC)
RETURNS SETOF record
AS 'MODULE_PATHNAME', 'pgnodemx_proc_pid_io'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION proc_pid_cmdline(
		OUT pid INTEGER,
		OUT fullcomm TEXT,
		OUT uid INTEGER,
		OUT username TEXT
)
RETURNS SETOF record
AS 'MODULE_PATHNAME', 'pgnodemx_proc_pid_cmdline'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION proc_pid_stat(
		OUT pid INTEGER,
		OUT comm TEXT,
		OUT state TEXT,
		OUT ppid INTEGER,
		OUT pgrp INTEGER,
		OUT session INTEGER,
		OUT tty_nr INTEGER,
		OUT tpgid INTEGER,
		OUT flags BIGINT,
		OUT minflt NUMERIC,
		OUT cminflt NUMERIC,
		OUT majflt NUMERIC,
		OUT cmajflt NUMERIC,
		OUT utime NUMERIC,
		OUT stime NUMERIC,
		OUT cutime BIGINT,
		OUT cstime BIGINT,
		OUT priority BIGINT,
		OUT nice BIGINT,
		OUT num_threads BIGINT,
		OUT itrealvalue BIGINT,
		OUT starttime NUMERIC,
		OUT vsize NUMERIC,
		OUT rss BIGINT,
		OUT rsslim NUMERIC,
		OUT startcode NUMERIC,
		OUT endcode NUMERIC,
		OUT startstack NUMERIC,
		OUT kstkesp NUMERIC,
		OUT kstkeip NUMERIC,
		OUT signal NUMERIC,
		OUT blocked NUMERIC,
		OUT sigignore NUMERIC,
		OUT sigcatch NUMERIC,
		OUT wchan NUMERIC,
		OUT nswap NUMERIC,
		OUT cnswap NUMERIC,
		OUT exit_signal INTEGER,
		OUT processor INTEGER,
		OUT rt_priority BIGINT,
		OUT policy BIGINT,
		OUT delayacct_blkio_ticks NUMERIC,
		OUT guest_time NUMERIC,
		OUT cguest_time BIGINT,
		OUT start_data NUMERIC,
		OUT end_data NUMERIC,
		OUT start_brk NUMERIC,
		OUT arg_start NUMERIC,
		OUT arg_end NUMERIC,
		OUT env_start NUMERIC,
		OUT env_end NUMERIC,
		OUT exit_code INTEGER)
RETURNS SETOF record
AS 'MODULE_PATHNAME', 'pgnodemx_proc_pid_stat'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kpages_to_bytes(NUMERIC)
RETURNS NUMERIC
AS 'MODULE_PATHNAME', 'pgnodemx_pages_to_bytes'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION proc_cputime(
		OUT "user" BIGINT,
		OUT nice BIGINT,
		OUT system BIGINT,
		OUT idle BIGINT,
		OUT iowait BIGINT)
RETURNS SETOF record
AS 'MODULE_PATHNAME', 'pgnodemx_proc_cputime'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION proc_loadavg(
		OUT load1 FLOAT,
		OUT load5 FLOAT,
		OUT load15 FLOAT,
		OUT last_pid INTEGER)
RETURNS SETOF record
AS 'MODULE_PATHNAME', 'pgnodemx_proc_loadavg'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION pg_proctab(
 OUT pid integer,
 OUT comm character varying,
 OUT fullcomm character varying,
 OUT state character,
 OUT ppid integer,
 OUT pgrp integer,
 OUT session integer,
 OUT tty_nr integer,
 OUT tpgid integer,
 OUT flags integer,
 OUT minflt bigint,
 OUT cminflt bigint,
 OUT majflt bigint,
 OUT cmajflt bigint,
 OUT utime bigint,
 OUT stime bigint,
 OUT cutime bigint,
 OUT cstime bigint,
 OUT priority bigint,
 OUT nice bigint,
 OUT num_threads bigint,
 OUT itrealvalue bigint,
 OUT starttime bigint,
 OUT vsize bigint,
 OUT rss bigint,
 OUT exit_signal integer,
 OUT processor integer,
 OUT rt_priority bigint,
 OUT policy bigint,
 OUT delayacct_blkio_ticks bigint,
 OUT uid integer,
 OUT username character varying,
 OUT rchar bigint,
 OUT wchar bigint,
 OUT syscr bigint,
 OUT syscw bigint,
 OUT reads bigint,
 OUT writes bigint,
 OUT cwrites bigint
)
RETURNS SETOF record
AS $$
SELECT
 s.pid,
 comm,
 fullcomm,
 state,
 ppid,
 pgrp,
 session,
 tty_nr,
 tpgid,
 flags,
 minflt,
 cminflt,
 majflt,
 cmajflt,
 utime,
 stime,
 cutime,
 cstime,
 priority,
 nice,
 num_threads,
 itrealvalue,
 starttime,
 vsize,
 kpages_to_bytes(rss) / 1024 as rss,
 exit_signal,
 processor,
 rt_priority,
 policy,
 delayacct_blkio_ticks,
 uid,
 username,
 rchar,
 wchar,
 syscr,
 syscw,
 reads,
 writes,
 cwrites
FROM proc_pid_stat() s
JOIN proc_pid_cmdline() c
ON s.pid = c.pid
JOIN proc_pid_io() i
ON c.pid = i.pid
$$ LANGUAGE sql;
