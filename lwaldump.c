#include "postgres.h"
#include "utils/pg_lsn.h"

#include "access/xlogutils.h"
#include "access/xlogreader.h"
#include "access/xlogrecord.h"
#include "access/xlog.h"

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
        last_lsn = xlogreader->EndRecPtr;
    }

    /*
     * Restore immediately the timeline where it was previously, as
     * read_local_xlog_page() could have changed it if the record was read
     * while recovery was finishing or if the timeline has jumped in-between.
     */
    ThisTimeLineID = save_currtli;

    XLogReaderFree(xlogreader);

    PG_RETURN_LSN(last_lsn);
}
