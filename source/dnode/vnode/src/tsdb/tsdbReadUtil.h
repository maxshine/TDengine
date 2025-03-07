/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TDENGINE_TSDBREADUTIL_H
#define TDENGINE_TSDBREADUTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tsdbDataFileRW.h"
#include "tsdbUtil2.h"

#define ASCENDING_TRAVERSE(o) (o == TSDB_ORDER_ASC)

typedef enum {
  READER_STATUS_SUSPEND = 0x1,
  READER_STATUS_NORMAL = 0x2,
} EReaderStatus;

typedef enum {
  EXTERNAL_ROWS_PREV = 0x1,
  EXTERNAL_ROWS_MAIN = 0x2,
  EXTERNAL_ROWS_NEXT = 0x3,
} EContentData;

typedef struct SBlockInfoBuf {
  int32_t currentIndex;
  SArray* pData;
  int32_t numPerBucket;
  int32_t numOfTables;
} SBlockInfoBuf;

typedef struct {
  STbDataIter* iter;
  int32_t      index;
  bool         hasVal;
} SIterInfo;

typedef struct STableBlockScanInfo {
  uint64_t  uid;
  TSKEY     lastKey;
  TSKEY     lastKeyInStt;       // last accessed key in stt
  SArray*   pBlockList;         // block data index list, SArray<SBrinRecord>
  SArray*   pMemDelData;        // SArray<SDelData>
  SArray*   pfileDelData;       // SArray<SDelData> from each file set
  SIterInfo iter;               // mem buffer skip list iterator
  SIterInfo iiter;              // imem buffer skip list iterator
  SArray*   delSkyline;         // delete info for this table
  int32_t   fileDelIndex;       // file block delete index
  int32_t   lastBlockDelIndex;  // delete index for last block
  bool      iterInit;           // whether to initialize the in-memory skip list iterator or not
} STableBlockScanInfo;

typedef struct SResultBlockInfo {
  SSDataBlock* pResBlock;
  bool         freeBlock;
  int64_t      capacity;
} SResultBlockInfo;

typedef struct SCostSummary {
  int64_t numOfBlocks;
  double  blockLoadTime;
  double  buildmemBlock;
  int64_t headFileLoad;
  double  headFileLoadTime;
  int64_t smaDataLoad;
  double  smaLoadTime;
  int64_t lastBlockLoad;
  double  lastBlockLoadTime;
  int64_t composedBlocks;
  double  buildComposedBlockTime;
  double  createScanInfoList;
  double  createSkylineIterTime;
  double  initLastBlockReader;
} SCostSummary;

typedef struct STableUidList {
  uint64_t* tableUidList;  // access table uid list in uid ascending order list
  int32_t   currentIndex;  // index in table uid list
} STableUidList;

typedef struct {
  int32_t numOfBlocks;
  int32_t numOfLastFiles;
} SBlockNumber;

typedef struct SBlockIndex {
  int32_t     ordinalIndex;
  int64_t     inFileOffset;
  STimeWindow window;  // todo replace it with overlap flag.
} SBlockIndex;

typedef struct SBlockOrderWrapper {
  int64_t              uid;
  int64_t              offset;
  STableBlockScanInfo* pInfo;
} SBlockOrderWrapper;

typedef struct SBlockOrderSupporter {
  SBlockOrderWrapper** pDataBlockInfo;
  int32_t*             indexPerTable;
  int32_t*             numOfBlocksPerTable;
  int32_t              numOfTables;
} SBlockOrderSupporter;

typedef struct SBlockLoadSuppInfo {
  TColumnDataAggArray colAggArray;
  SColumnDataAgg      tsColAgg;
  int16_t*            colId;
  int16_t*            slotId;
  int32_t             numOfCols;
  char**              buildBuf;  // build string tmp buffer, todo remove it later after all string format being updated.
  bool                smaValid;  // the sma on all queried columns are activated
} SBlockLoadSuppInfo;

typedef struct SLastBlockReader {
  STimeWindow        window;
  SVersionRange      verRange;
  int32_t            order;
  uint64_t           uid;
  SMergeTree         mergeTree;
  SSttBlockLoadInfo* pInfo;
  int64_t            currentKey;
} SLastBlockReader;

typedef struct SFilesetIter {
  int32_t           numOfFiles;    // number of total files
  int32_t           index;         // current accessed index in the list
  TFileSetArray*    pFilesetList;  // data file set list
  int32_t           order;
  SLastBlockReader* pLastBlockReader;  // last file block reader
} SFilesetIter;

typedef struct SFileDataBlockInfo {
  // index position in STableBlockScanInfo in order to check whether neighbor block overlaps with it
  uint64_t    uid;
  int32_t     tbBlockIdx;
  SBrinRecord record;
} SFileDataBlockInfo;

typedef struct SDataBlockIter {
  int32_t    numOfBlocks;
  int32_t    index;
  SArray*    blockList;  // SArray<SFileDataBlockInfo>
  int32_t    order;
  SDataBlk   block;  // current SDataBlk data
  SSHashObj* pTableMap;
} SDataBlockIter;

typedef struct SFileBlockDumpInfo {
  int32_t totalRows;
  int32_t rowIndex;
  int64_t lastKey;
  bool    allDumped;
} SFileBlockDumpInfo;

typedef struct SReaderStatus {
  bool                  loadFromFile;       // check file stage
  bool                  composedDataBlock;  // the returned data block is a composed block or not
  SSHashObj*            pTableMap;          // SHash<STableBlockScanInfo>
  STableBlockScanInfo** pTableIter;         // table iterator used in building in-memory buffer data blocks.
  STableUidList         uidList;            // check tables in uid order, to avoid the repeatly load of blocks in STT.
  SFileBlockDumpInfo    fBlockDumpInfo;
  STFileSet*            pCurrentFileset;  // current opened file set
  SBlockData            fileBlockData;
  SFilesetIter          fileIter;
  SDataBlockIter        blockIter;
  SArray*               pLDataIterArray;
  SRowMerger            merger;
  SColumnInfoData*      pPrimaryTsCol;  // primary time stamp output col info data
} SReaderStatus;

struct STsdbReader {
  STsdb*             pTsdb;
  STsdbReaderInfo    info;
  TdThreadMutex      readerMutex;
  EReaderStatus      flag;
  int32_t            code;
  uint64_t           rowsNum;
  SResultBlockInfo   resBlockInfo;
  SReaderStatus      status;
  char*              idStr;  // query info handle, for debug purpose
  int32_t            type;   // query type: 1. retrieve all data blocks, 2. retrieve direct prev|next rows
  SBlockLoadSuppInfo suppInfo;
  STsdbReadSnap*     pReadSnap;
  SCostSummary       cost;
  SHashObj**         pIgnoreTables;
  SSHashObj*         pSchemaMap;   // keep the retrieved schema info, to avoid the overhead by repeatly load schema
  SDataFileReader*   pFileReader;  // the file reader
  SBlockInfoBuf      blockInfoBuf;
  EContentData       step;
  STsdbReader*       innerReader[2];
};

typedef struct SBrinRecordIter {
  SArray*          pBrinBlockList;
  SBrinBlk*        pCurrentBlk;
  int32_t          blockIndex;
  int32_t          recordIndex;
  SDataFileReader* pReader;
  SBrinBlock       block;
  SBrinRecord      record;
} SBrinRecordIter;

STableBlockScanInfo* getTableBlockScanInfo(SSHashObj* pTableMap, uint64_t uid, const char* id);

SSHashObj* createDataBlockScanInfo(STsdbReader* pTsdbReader, SBlockInfoBuf* pBuf, const STableKeyInfo* idList,
                                   STableUidList* pUidList, int32_t numOfTables);
void       clearBlockScanInfo(STableBlockScanInfo* p);
void       destroyAllBlockScanInfo(SSHashObj* pTableMap);
void       resetAllDataBlockScanInfo(SSHashObj* pTableMap, int64_t ts, int32_t step);
void       cleanupInfoFoxNextFileset(SSHashObj* pTableMap);
int32_t    ensureBlockScanInfoBuf(SBlockInfoBuf* pBuf, int32_t numOfTables);
void       clearBlockScanInfoBuf(SBlockInfoBuf* pBuf);
void*      getPosInBlockInfoBuf(SBlockInfoBuf* pBuf, int32_t index);

// brin records iterator
void         initBrinRecordIter(SBrinRecordIter* pIter, SDataFileReader* pReader, SArray* pList);
SBrinRecord* getNextBrinRecord(SBrinRecordIter* pIter);
void         clearBrinBlockIter(SBrinRecordIter* pIter);

// initialize block iterator API
int32_t initBlockIterator(STsdbReader* pReader, SDataBlockIter* pBlockIter, int32_t numOfBlocks, SArray* pTableList);
bool    blockIteratorNext(SDataBlockIter* pBlockIter, const char* idStr);

// load tomb data API (stt/mem only for one table each, tomb data from data files are load for all tables at one time)
void    loadMemTombData(SArray** ppMemDelData, STbData* pMemTbData, STbData* piMemTbData, int64_t ver);
int32_t loadDataFileTombDataForAll(STsdbReader* pReader);
int32_t loadSttTombDataForAll(STsdbReader* pReader, SSttFileReader* pSttFileReader, SSttBlockLoadInfo* pLoadInfo);

#ifdef __cplusplus
}
#endif

#endif  // TDENGINE_TSDBREADUTIL_H
