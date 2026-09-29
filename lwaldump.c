/*-------------------------------------------------------------------------
 *
 * hello_ext.c
 *     example extenstion for PostgreSQL
 *
 * Copyright (c) 2014-2015, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *      hello_ext/hello_ext.c
 *
 *-------------------------------------------------------------------------
 */

#define FRONTEND 1

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"
#include "postgres.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access/xlogreader.h"
#include "access/xlogrecord.h"
#include "access/xlog_internal.h"
#include "access/xlog.h"
#include "access/transam.h"
#include "common/fe_memutils.h"
#include "common/logging.h"
#include "getopt_long.h"
#include "miscadmin.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(lwaldump);

Datum
lwaldump(PG_FUNCTION_ARGS)
{
    XLogRecord *record;
    XLogReaderState *xlogreader;
    char       *errormsg;
    TimeLineID save_currtli = ThisTimeLineID;
    XLogRecPtr last_lsn = GetXLogReplayRecPtr(&ThisTimeLineID);

    xlogreader = XLogReaderAllocate(wal_segment_size, NULL,
                                    XL_ROUTINE(.page_read = &read_local_xlog_page,
                                               .segment_open = &wal_segment_open,
                                               .segment_close = &wal_segment_close),
                                    NULL);
    if (!xlogreader)
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("out of memory"),
                 errdetail("Failed while allocating a WAL reading processor.")));

    XLogBeginRead(xlogreader, last_lsn);
    record = XLogReadRecord(xlogreader, &errormsg);

    for (;;)
    {
        /* try to read the next record */
        record = XLogReadRecord(xlogreader, &errormsg);
        if (!record)
        {
            break;
        }
        /* after reading the first record, continue at next one */
        first_record = InvalidXLogRecPtr;
        last_lsn = xlogreader->EndRecPtr;
    }

    /*
     * Restore immediately the timeline where it was previously, as
     * read_local_xlog_page() could have changed it if the record was read
     * while recovery was finishing or if the timeline has jumped in-between.
     */
    ThisTimeLineID = save_currtli;

    XLogReaderFree(xlogreader_state);

    PG_RETURN_LSN(last_lsn);
}
