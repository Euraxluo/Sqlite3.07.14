/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
*************************************************************************
** This header file defines the interface that the sqlite page cache
** subsystem.  The page cache subsystem reads and writes a file a page
** at a time and provides a journal for rollback.
*/
//���ͷ�ļ�������һ��SQLite����ҳ����ϵͳ�Ľ��档���ҳ�滺����ϵͳ����дһ���ļ���һ��ҳ�沢���ṩ��һ���ع�����־��
#ifndef _PAGER_H_
#define _PAGER_H_
//���û�ж���pager-h���ǾͶ���һ��
/*
** Default maximum size for persistent journal files. A negative 
** value means no limit. This value may be overridden using the 
** sqlite3PagerJournalSizeLimit() API. See also "PRAGMA journal_size_limit".
*/
//�Ե�ǰ��־�ļ��������ֵ��һ������ֵ��ζ��û�����ơ�
//���ֵ���ܱ�����sqlite3PagerJournalSizeLimit����Ӧ�ó�����渲д��Ҳ�ǿ�������ָʾ journal-size-limit��
#ifndef SQLITE_DEFAULT_JOURNAL_SIZE_LIMIT
  #define SQLITE_DEFAULT_JOURNAL_SIZE_LIMIT -1
#endif
//���û�ж���SQLite-default-journal-size-limit(SQLiteĬ����־��С����)���ǾͶ���һ����
/*
** The type used to represent a page number.  The first page in a file
** is called page 1.  0 is used to represent "not a page".
*/
//��������Ǳ����ڱ�ʾҳ��������ҳ�ţ��ġ��ļ��еĵ�һҳ����Ϊpage 1��0�Ǳ�������ʾ��û��ҳ�桱��
typedef u32 Pgno;
����PgnoΪu32���͵�
/*
** Each open file is managed by a separate instance of the "Pager" structure.
*/
//ÿ���򿪵��ļ�������һ�������ġ�Pager���ṹʵ������
typedef struct Pager Pager;
����Pager����ΪPager��ǰ�����ͽṹ��
/*
** Handle type for pages.
*/
//����ҳ�����͡�
typedef struct PgHdr DbPage;
����ÿһ���ڴ�ҳ���ҳ��ͷ�ṹΪDbPage
/*
** Page number PAGER_MJ_PGNO is never used in an SQLite database (it is
** reserved for working around a windows/posix incompatibility). It is
** used in the journal to signify that the remainder of the journal file 
** is devoted to storing a master journal name - there are no more pages to
** roll back. See comments for function writeMasterJournal() in pager.c 
** for details.
*/
/*ҳ��PAGER_MJ_PGNO��δ��һ��SQLite���ݿ���ʹ�ù�(����רΪ�����windows / posix���������Ᵽ����)��
��������־����������ʾ��־�ļ������ಿ�������ڴ洢����־����-û�и����ҳ�����ڻع��ˡ�
��pager.c����writeMasterJournal()�ĺ��������۵�ϸ�ڡ�*/
#define PAGER_MJ_PGNO(x) ((Pgno)((PENDING_BYTE/((x)->pageSize))+1))
����PAGER_MJ_PGNO(x)  ( (Pgno)  ( (PENDING_BYTE/ ((x)->pageSize) )+1))
/*
** Allowed values for the flags parameter to sqlite3PagerOpen().
**
** NOTE: These values must match the corresponding BTREE_ values in btree.h.
*/
//����sqlite3PagerOpen()�ı�־������ֵ��ע��:��Щֵ����ƥ����Ӧ����btree.h BTREEֵ��
#define PAGER_OMIT_JOURNAL  0x0001    /* Do not use a rollback journal */
#define PAGER_MEMORY        0x0002    /* In-memory database */
����PAGER_OMIT_JOURNAL 0 x0001 / *��ʹ�ûع���־* / ����
����PAGER_MEMORY 0 x0002 /*�ڴ����ݿ�* /
/*
** Valid values for the second argument to sqlite3PagerLockingMode().
*/
sqlite3PagerLockingMode()�ڶ�����������Чֵ��
#define PAGER_LOCKINGMODE_QUERY      -1
#define PAGER_LOCKINGMODE_NORMAL      0
#define PAGER_LOCKINGMODE_EXCLUSIVE   1
#����PAGER_LOCKINGMODE_QUERY -1 ����
#����PAGER_LOCKINGMODE_NORMAL 0 ����
#����PAGER_LOCKINGMODE_EXCLUSIVE 1
/*
** Numeric constants that encode the journalmode.  
*/
//���ֳ���journalmode���롣
#define PAGER_JOURNALMODE_QUERY     (-1)  /* Query the value of journalmode */
#define PAGER_JOURNALMODE_DELETE      0   /* Commit by deleting journal file */
#define PAGER_JOURNALMODE_PERSIST     1   /* Commit by zeroing journal header */
#define PAGER_JOURNALMODE_OFF         2   /* Journal omitted.  */
#define PAGER_JOURNALMODE_TRUNCATE    3   /* Commit by truncating journal */
#define PAGER_JOURNALMODE_MEMORY      4   /* In-memory journal file */
#define PAGER_JOURNALMODE_WAL         5   /* Use write-ahead logging */
 /*#����PAGER_JOURNALMODE_QUERY(-1)    /*��ѯjournalmode��ֵ* /����
#����PAGER_JOURNALMODE_DELETE 0  //�ύͨ��ɾ����־�ļ�����
# define PAGER_JOURNALMODE_PERSIST 1 //�ύͨ��������־���⡡��
#����PAGER_JOURNALMODE_OFF 2 //ʡ����־��* / ����
#����PAGER_JOURNALMODE_TRUNCATE 3 / *ͨ��ɾ����־�ύ* / ����
#����PAGER_JOURNALMODE_MEMORY 4 / *�ڴ���־�ļ�* / ����
#����PAGER_JOURNALMODE_WAL 5 / *ʹ��дǰ��־��¼* /
/*
** The remainder of this file contains the declarations of the functions
** that make up the Pager sub-system API. See source code comments for 
** a detailed description of each routine.
*/
//���ļ���ʣ�ಿ�ְ����ĺ�����������������������Pager��ϵͳAPI��Ϊ���˽�ÿ�����̵���ϸ����ȥ��Դ��ע�͡�
/* Open and close a Pager connection. */ 
int sqlite3PagerOpen(
  sqlite3_vfs*,//pvfs  ��Ӧ�õ������ļ�ϵͳ             
  Pager **ppPager,//out�����ﷵ��ҳ��ṹ
  const char*,// zFilename //���򿪵����ݿ��ļ�����
  int,//nExtra ��ÿһ���ڴ�ҳ���ϸ��ӵĶ�����ֽ�
  int,//flags �����ļ��ı�־
  int,//vfsFlag ͨ��sqlite3-vfs.XOpen�����ı�־
  void(*xReinit)(DbPage*)
)

int sqlite3PagerClose(Pager *pPager);

int sqlite3PagerReadFileheader(Pager*, int, unsigned char*);

/* Functions used to configure a Pager object. */
void sqlite3PagerSetBusyhandler(Pager*, int(*)(void *), void *);

int sqlite3PagerSetPagesize(Pager*, u32*, int);

int sqlite3PagerMaxPageCount(Pager*, int);

void sqlite3PagerSetCachesize(Pager*, int);

void sqlite3PagerShrink(Pager*);

void sqlite3PagerSetSafetyLevel(Pager*,int,int,int);

int sqlite3PagerLockingMode(Pager *, int);

int sqlite3PagerSetJournalMode(Pager *, int);

int sqlite3PagerGetJournalMode(Pager*);

int sqlite3PagerOkToChangeJournalMode(Pager*);

i64 sqlite3PagerJournalSizeLimit(Pager *, i64);

sqlite3_backup **sqlite3PagerBackupPtr(Pager*);

/* Functions used to obtain and release page references. */ 
int sqlite3PagerAcquire(Pager *pPager, Pgno pgno, DbPage **ppPage, int clrFlag);
#define sqlite3PagerGet(A,B,C) sqlite3PagerAcquire(A,B,C,0)

DbPage *sqlite3PagerLookup(Pager *pPager, Pgno pgno);

void sqlite3PagerRef(DbPage*);

void sqlite3PagerUnref(DbPage*);

/* Operations on page references. */
int sqlite3PagerWrite(DbPage*);

void sqlite3PagerDontWrite(DbPage*);

int sqlite3PagerMovepage(Pager*,DbPage*,Pgno,int);

int sqlite3PagerPageRefcount(DbPage*);

void *sqlite3PagerGetData(DbPage *); 

void *sqlite3PagerGetExtra(DbPage *); 

/* Functions used to manage pager transactions and savepoints. */
void sqlite3PagerPagecount(Pager*, int*);

int sqlite3PagerBegin(Pager*, int exFlag, int);

int sqlite3PagerCommitPhaseOne(Pager*,const char *zMaster, int);

int sqlite3PagerExclusiveLock(Pager*);

int sqlite3PagerSync(Pager *pPager);

int sqlite3PagerCommitPhaseTwo(Pager*);
int sqlite3PagerRollback(Pager*);
int sqlite3PagerOpenSavepoint(Pager *pPager, int n);
int sqlite3PagerSavepoint(Pager *pPager, int op, int iSavepoint);
int sqlite3PagerSharedLock(Pager *pPager);

int sqlite3PagerCheckpoint(Pager *pPager, int, int*, int*);
int sqlite3PagerWalSupported(Pager *pPager);
int sqlite3PagerWalCallback(Pager *pPager);
int sqlite3PagerOpenWal(Pager *pPager, int *pisOpen);
int sqlite3PagerCloseWal(Pager *pPager);
#ifdef SQLITE_ENABLE_ZIPVFS
  int sqlite3PagerWalFramesize(Pager *pPager);
#endif

/* Functions used to query pager state and configuration. */
u8 sqlite3PagerIsreadonly(Pager*);
int sqlite3PagerRefcount(Pager*);
int sqlite3PagerMemUsed(Pager*);
const char *sqlite3PagerFilename(Pager*, int);
const sqlite3_vfs *sqlite3PagerVfs(Pager*);
sqlite3_file *sqlite3PagerFile(Pager*);
const char *sqlite3PagerJournalname(Pager*);
int sqlite3PagerNosync(Pager*);
void *sqlite3PagerTempSpace(Pager*);
int sqlite3PagerIsMemdb(Pager*);
void sqlite3PagerCacheStat(Pager *, int, int, int *);
void sqlite3PagerClearCache(Pager *);

/* Functions used to truncate the database file. */
void sqlite3PagerTruncateImage(Pager*,Pgno);

#if defined(SQLITE_HAS_CODEC) && !defined(SQLITE_OMIT_WAL)
void *sqlite3PagerCodec(DbPage *);
#endif

/* Functions to support testing and debugging. */
#if !defined(NDEBUG) || defined(SQLITE_TEST)
  Pgno sqlite3PagerPagenumber(DbPage*);
  int sqlite3PagerIswriteable(DbPage*);
#endif
#ifdef SQLITE_TEST
  int *sqlite3PagerStats(Pager*);
  void sqlite3PagerRefdump(Pager*);
  void disable_simulated_io_errors(void);
  void enable_simulated_io_errors(void);
#else
# define disable_simulated_io_errors()
# define enable_simulated_io_errors()
#endif
//SQLite��Pager���ְ����ܷ��а˸�ģ�飬ÿ��ģ�鶼�������Բ�ͬ���ܵĺ����������ÿ��ģ�����˵����
//1��/�ر�һ��ҳ������
/*���ģ����Ҫ����򿪡��ر�һ��ҳ�棬���Ҷ�ȡ���ݿ��ļ����⡣��Ҫ�Ŀռ䣬����·�����Ȳ��洢����·������
��ҳ�����̶��ṹ�Ĵ洢������ҳ���ļ�������־������ҳ��Ĭ�ϴ�Сֵ �����÷����һع�ǰͬ����
������־�Ĳ�ͬ�����ַ������ݿ⣬����������÷����һع�����������ȡ���ݿ���⣬
��ȡ�ļ�ǰN���ֽڷ���pDestָ��Ĵ洢�С�
*/
//2����ҳ�����ĺ���
/*���ģ����Ҫ��������æ������ҳ���С������ۼ�ҳ����������ڴ�ҳ������ҳ�氲ȫ�ȼ���ҳ����ģʽ����־ģʽ��
���Ի�ȡҳ����ģʽ����־ģʽ��ָ�򱸷ݵ�ָ�룬�����ж���־ģʽ�Ƿ�ı䣬���Ҿ����ܵ��ͷŴ洢��
������ʱ���ò�����æ�������ı������õ�ҳ���С����*pPageSize�д��ݣ��������ݿ�ҳ��������ֵ��
�ı��ڴ�ҳ������������ҳ������ҳ���о����ܵ��ͷŴ洢����ҳ�����ð�ȫ�ȼ���
��ȡ/����ҳ�����ģʽ����־�ļ��Ĵ�С���ƣ�����ҳ�����־ģʽ�����ص�ǰ����־ģʽ��
�ж��Ƿ���Ըı���־ģʽ���õ�ָ�򱸷ݵ�ָ�롣
*/
//3��ȡ���ͷ�ҳ�����õĺ�����
/*���ģ����Ҫ�����ȡҳ������á�����ҳ�桢�������ü������������ͷ�ҳ�����á�
��ȡҳ��pgno��ҳ������ã��ж�����ҳ���Ƿ��ڻ����У�ҳ���ʼ����������Ϊ0��
��ȡ�����б��Ҷ��ҳ���һع�����㲢������ҳ�����д����־���ݵĻ����У�
����ʧ�ܺ󷵻ش�����벢����*ppPagerΪNULL����ȡ�����е�ҳ�棬��������򷵻�ָ���0��ҳ�����ü�����������
�ͷ�ҳ�����á�
*/
//4��ҳ�����õĲ���
/*���ģ����Ҫ�����ҳ����Ϊ��д���ƶ�ҳ�棬�������ҳ��������ָ�����ݵ�ָ���ָ��nExtra��ָ�롣
����ҳ����Ϊ��д�����޸�ǰ��������麯������ֵ������֮�����轫ҳ����Ϣд�ش��̣���λҳ�����ƶ�ҳ�棻
��ȡ����ҳ����������ȡҳ�����ݵ�ָ�룻��ȡҳ��nExtra�Ķ���ռ��ֽ�ָ�롣
*/
//5����ҳ������ͱ����ĺ���
/*���ģ����Ҫ�����ȡ���ݿ�ҳ����������ʼһ��д����ͬ�����ݿ��ļ���������������
��ȡ���ݿ���ҳ����������ָ��ҳ���Ͽ�ʼһ��д���񣻶�ҳ��pPagerͬ�����ݿ��ļ���
���ݿ��ļ�������仯����־�ļ���ȫ���£�ͬ�����ݿ��ļ��������ϣ�д��־��Ծʱ�Զ����û�������
�ع�ģʽ�ɹ����Ĳ�ͬ���أ���������й̶������ı����򿪣��ع����ͷű���㣻��ȡһ����������
��walģʽ�µ��ü��㣻�ж��Ƿ���֧�ֵײ�VFS��ԭ���/�ر�walģʽ���ر���־�ļ������ӡ�
*/
//6	��ѯҳ��״̬/����
/*�ж��Ƿ���ֻ���ļ������ҳ������������õ�ǰҳ��ʹ�õ��ڴ棻������ݿ�����·���������ҳ��VFS�ṹ��
�����ҳ����ص����ݿ��ļ������������־�ļ������ļ������ж��Ƿ�ͬ��ҳ�棻���ҳ���ڲ�����ʱҳ�滺��ָ�룻
���eStat��״̬�����ҳ�滺�档
*/
//7	�ض����ݿ��ļ�
/*�ض��ڴ����ݿ��ļ�ͼ��nPage�ϣ����������޸Ĵ����ϵ����ݿ��ļ���ֻ��������ҳ�������ڲ�״̬��
��ҳ������д����־�ļ���ʱ����walģ��
*/
//8	����/����
 
//����ҳ��ҳ�룻�����Ժͷ��� 

#endif /* _PAGER_H_ */

