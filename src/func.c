/*
** 2002 February 23
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains the C functions that implement various SQL这个文件包含实现各种SQL函数的SQLite的C函数。
** functions of SQLite.  
**
** There is only one exported symbol in this file - the function
** sqliteRegisterBuildinFunctions() found at the bottom of the file.这个文件只有一个出口标志——函数sqliteRegisterBuildinFunctions()在文件的底部。
** All other code has file scope.
*/
#include "sqliteInt.h"
#include <stdlib.h>
#include <assert.h>//断言就是用于在代码中捕捉这些假设，可以将断言看作是异常处理的一种高级形式。
//断言表示为一些布尔表达式，程序员相信在程序中的某个特定点该表达式值为真。
//assert是宏，而不是函数。在C的assert.h头文件中。
//assert宏的原型定义在<assert.h>中，其作用是如果它的条件返回错误，则终止程序执行
#include "vdbeInt.h"

/*
** Return the collating function associated with a function.返回排序函数与函数有关。
*/
static CollSeq *sqlite3GetFuncCollSeq(sqlite3_context *context){
  return context->pColl;//返回排序的序列
}

/*
** Indicate that the accumulator load should be skipped on this
** iteration of the aggregate loop.表明累加器负载应该跳过这个迭代的总循环。。
*/
static void sqlite3SkipAccumulatorLoad(sqlite3_context *context){
  context->skipFlag = 1;//跳转标志为1
}

/*
** Implementation of the non-aggregate min() and max() functions
实现非聚合min()和max()函数
*/
static void minmaxFunc(
  sqlite3_context *context,//函数上下文
  int argc,
  sqlite3_value **argv
){
  int i;
  int mask;    /* 0 for min() or 0xffffffff for max();0为最小,-1为最大
  因为int 为带符号类型,带符号类型最高为是符号位
  ,又因为0xFFFFFFFF,也就是四个字节32 bits全是1, 符号位是1,所以这个数是负数 */
  int iBest;
  CollSeq *pColl;

  assert( argc>1 );//如果 argc>1，则继续执行，否则终止程序执行
  mask = sqlite3_user_data(context)==0 ? 0 : -1;//判断最小最大值
  pColl = sqlite3GetFuncCollSeq(context);
  assert( pColl );
  assert( mask==-1 || mask==0 );//断言最大值是-1或者最小值是0
  iBest = 0;
  if( sqlite3_value_type(argv[0])==SQLITE_NULL ) return;	//自定义接口函数
  for(i=1; i<argc; i++){
    if( sqlite3_value_type(argv[i])==SQLITE_NULL ) return;
    if( (sqlite3MemCompare(argv[iBest], argv[i], pColl)^mask)>=0 ){	//比较大小
      testcase( mask==0 );
      iBest = i;
    }
  }
  sqlite3_result_value(context, argv[iBest]);	//返回最值
}

/*
** Return the type of the argument.返回的类型参数。
*/
static void typeofFunc(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **argv
){
  const char *z = 0;
  UNUSED_PARAMETER(NotUsed);//从未用过的参数
  switch( sqlite3_value_type(argv[0]) ){
    case SQLITE_INTEGER: z = "integer"; break;		//带符号整型
    case SQLITE_TEXT:    z = "text";    break;		//字符串文本
    case SQLITE_FLOAT:   z = "real";    break;		//浮点数
    case SQLITE_BLOB:    z = "blob";    break;		//二进制大对象(如何输入就如何存储，不改变格式)
    default:             z = "null";    break;		//空值
  }
  sqlite3_result_text(context, z, -1, SQLITE_STATIC);//返回结果
}


/*
** Implementation of the length() function长度函数的实现
*/
static void lengthFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int len;

  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  switch( sqlite3_value_type(argv[0]) ){
    case SQLITE_BLOB:					//二进制
    case SQLITE_INTEGER:				//整型
    case SQLITE_FLOAT: {				//浮点数
      sqlite3_result_int(context, sqlite3_value_bytes(argv[0]));//返回标量值
      break;
    }
    case SQLITE_TEXT: {					//文本
      const unsigned char *z = sqlite3_value_text(argv[0]);
      if( z==0 ) return;				//长度为0，返回0
      len = 0;
      while( *z ){
        len++;
        SQLITE_SKIP_UTF8(z);
      }
      sqlite3_result_int(context, len);	//返回长度
      break;
    }
    default: {
      sqlite3_result_null(context);	    //返回空值
      break;
    }
  }
}

/*
** Implementation of the abs() function.abs函数的实现。
**
** IMP: R-23979-26855 The abs(X) function returns the absolute value of
** the numeric argument X. abs(X)函数返回数值的绝对值参数X
*/
static void absFunc(sqlite3_context *context, int argc, sqlite3_value **argv)//绝对值函数
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  switch( sqlite3_value_type(argv[0]) ){
    case SQLITE_INTEGER: {
      i64 iVal = sqlite3_value_int64(argv[0]);
      if( iVal<0 ){                //判断如果ival小于0
        if( (iVal<<1)==0 ){
          /* IMP: R-35460-15084 If X is the integer -9223372036854775807 then
          ** abs(X) throws an integer overflow error since there is no
          ** equivalent positive 64-bit two complement value. 如果X是整数-9223372036854775807
          那么abs(X)抛出一个整数溢出错误,  因为没有积极的64位两个补充值。*/
          sqlite3_result_error(context, "integer overflow", -1);   //返回错误，整型溢出
          return;
        }
        iVal = -iVal;      //取ival的负赋值给ival
      } 
      sqlite3_result_int64(context, iVal);
      break;
    }
    case SQLITE_NULL: {
      /* IMP: R-37434-19929 Abs(X) returns NULL if X is NULL. 如果x是空，则返回绝对值为空*/
      sqlite3_result_null(context);
      break;
    }
    default: {
      /* Because sqlite3_value_double() returns 0.0 if the argument is not
      ** something that can be converted into a number, we have:
      ** IMP: R-57326-31541 Abs(X) return 0.0 if X is a string or blob that
      ** cannot be converted to a numeric value.
      因为sqlite3_value_double()返回0.如果这个内容不可以被转换成一个数字值，
      Abs(X)返回0，如果X是一个字符串或blob,不能被转换成数字值。
      */
      double rVal = sqlite3_value_double(argv[0]);
      if( rVal<0 ) rVal = -rVal;			//如果rval小于0，取rval的负赋值给rval
      sqlite3_result_double(context, rVal);	//返回绝对值
      break;
    }
  }
}

/*
** Implementation of the substr() function.substr()函数的实现substr 取部份字符串string substr(string string, int start, int [length])
**
** substr(x,p1,p2)  returns p2 characters of x[] beginning with p1.
** p1 is 1-indexed.  So substr(x,1,1) returns the first character
** of x.  If x is text, then we actually count UTF-8 characters.
** If x is a blob, then we count bytes.
**
** If p1 is negative, then we begin abs(p1) from the end of x[].
**
** If p2 is negative, return the p2 characters preceeding p1.
substr(x,p1,p2)是返回从p1开始查找x[]中第p2的字符
。所以substr(x,1,1)返回第一个字符x,
如果x是文本,那么我们实际上计算utf - 8字符数。
如果x是一个blob,那么我们计数字节。
如果p1是负的,那么我们开始从abs(p1)中x[]尾端算起。
如果p2是负的,返回倒数第p2个字符。
*/
static void substrFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const unsigned char *z;
  const unsigned char *z2;
  int len;
  int p0type;
  i64 p1, p2;
  int negP2 = 0;

  assert( argc==3 || argc==2 );
  if( sqlite3_value_type(argv[1])==SQLITE_NULL
   || (argc==3 && sqlite3_value_type(argv[2])==SQLITE_NULL)	//判断为空值的情况
  ){
    return;	//返回空
  }
  p0type = sqlite3_value_type(argv[0]);
  p1 = sqlite3_value_int(argv[1]);
  if( p0type==SQLITE_BLOB ){	//判断取值是否为二进制
    len = sqlite3_value_bytes(argv[0]);
    z = sqlite3_value_blob(argv[0]);
    if( z==0 ) return;
    assert( len==sqlite3_value_bytes(argv[0]) );
  }else{//若z是文本			
    z = sqlite3_value_text(argv[0]);
    if( z==0 ) return;
    len = 0;
    if( p1<0 ){	//若p1为负
      for(z2=z; *z2; len++){
        SQLITE_SKIP_UTF8(z2);														
      }
    }
  }
  if( argc==3 ){
    p2 = sqlite3_value_int(argv[2]);												
    if( p2<0 ){																		
      p2 = -p2;
      negP2 = 1;
    }
  }else{
    p2 = sqlite3_context_db_handle(context)->aLimit[SQLITE_LIMIT_LENGTH];
  }
  if( p1<0 ){
    p1 += len;
    if( p1<0 ){
      p2 += p1;
      if( p2<0 ) p2 = 0;
      p1 = 0;
    }
  }else if( p1>0 ){
    p1--;
  }else if( p2>0 ){
    p2--;
  }
  if( negP2 ){
    p1 -= p2;
    if( p1<0 ){
      p2 += p1;
      p1 = 0;
    }
  }
  assert( p1>=0 && p2>=0 );
  if( p0type!=SQLITE_BLOB ){
    while( *z && p1 ){
      SQLITE_SKIP_UTF8(z);
      p1--;
    }
    for(z2=z; *z2 && p2; p2--){
      SQLITE_SKIP_UTF8(z2);
    }
    sqlite3_result_text(context, (char*)z, (int)(z2-z), SQLITE_TRANSIENT);
  }else{
    if( p1+p2>len ){
      p2 = len-p1;
      if( p2<0 ) p2 = 0;
    }
    sqlite3_result_blob(context, (char*)&z[p1], (int)p2, SQLITE_TRANSIENT);
  }
}

/*
** Implementation of the round() function
返回数字表达式并四舍五入为指定的长度或精度。
*/
#ifndef SQLITE_OMIT_FLOATING_POINT
static void roundFunc(sqlite3_context *context, int argc, sqlite3_value **argv){		
  int n = 0;
  double r;
  char *zBuf;
  assert( argc==1 || argc==2 );
  if( argc==2 ){
    if( SQLITE_NULL==sqlite3_value_type(argv[1]) ) return;	//先判断为空值的情况
    n = sqlite3_value_int(argv[1]);
    if( n>30 ) n = 30;
    if( n<0 ) n = 0;
  }
  if( sqlite3_value_type(argv[0])==SQLITE_NULL ) return;
  r = sqlite3_value_double(argv[0]);
  /* If Y==0 and X will fit in a 64-bit int
  ** handle the rounding directly,
  ** otherwise use printf.
  如果Y = X = 0,适合在一个64位整数,
直接处理输出
否则使用printf。
  */
  if( n==0 && r>=0 && r<LARGEST_INT64-1 ){
    r = (double)((sqlite_int64)(r+0.5));
  }else if( n==0 && r<0 && (-r)<LARGEST_INT64-1 ){
    r = -(double)((sqlite_int64)((-r)+0.5));
  }else{
    zBuf = sqlite3_mprintf("%.*f",n,r);
    if( zBuf==0 ){
      sqlite3_result_error_nomem(context);
      return;
    }
    sqlite3AtoF(zBuf, &r, sqlite3Strlen30(zBuf), SQLITE_UTF8);
    sqlite3_free(zBuf);
  }
  sqlite3_result_double(context, r);
}
#endif

/*
** Allocate nByte bytes of space using sqlite3_malloc(). If the
** allocation fails, call sqlite3_result_error_nomem() to notify
** the database handle that malloc() has failed and return NULL.
** If nByte is larger than the maximum string or blob length, then
** raise an SQLITE_TOOBIG exception and return NULL.
分配nByte字节的空间使用sqlite3_malloc()。
如果分配失败,调用sqlite3_result_error_nomem()通知数据库句柄,malloc()失败,返回NULL。
如果nByte大于最大字符串或blob长度,然后引发一个SQLITE_TOOBIG异常,返回NULL。
*/
static void *contextMalloc(sqlite3_context *context, i64 nByte){//该函数分配了NBytes个字节,并返回了指向这块内存的指针
//如果分配失败，则返回一个空指针null
  char *z;
  sqlite3 *db = sqlite3_context_db_handle(context);
  assert( nByte>0 );
  testcase( nByte==db->aLimit[SQLITE_LIMIT_LENGTH] );
  testcase( nByte==db->aLimit[SQLITE_LIMIT_LENGTH]+1 );
  if( nByte>db->aLimit[SQLITE_LIMIT_LENGTH] ){	//如果nByte大于最大字符串或blob长度,然后引发一个SQLITE_TOOBIG异常,返回NULL。
    sqlite3_result_error_toobig(context);
    z = 0;
  }else{
    z = sqlite3Malloc((int)nByte);
    if( !z ){
      sqlite3_result_error_nomem(context);//如果分配失败,调用sqlite3_result_error_nomem()通知数据库句柄,malloc()失败,返回NULL?
    }
  }
  return z;
}

/*
** Implementation of the upper() and lower() SQL functions.
实现upper 函数把字符串转换为大写字母和 lower 函数把字符串转换为小写字母
*/
static void upperFunc(sqlite3_context *context, int argc, sqlite3_value **argv){//返回将小写字符数据转换为大写的字符表达式。
  char *z1;
  const char *z2;
  int i, n;
  UNUSED_PARAMETER(argc);
  z2 = (char*)sqlite3_value_text(argv[0]);
  n = sqlite3_value_bytes(argv[0]);
  /* Verify that the call to _bytes() does not invalidate the _text() pointer 
  验证调用_bytes()并不能否定_text()的指针*/
  assert( z2==(char*)sqlite3_value_text(argv[0]) );
  if( z2 ){
    z1 = contextMalloc(context, ((i64)n)+1);
    if( z1 ){
      for(i=0; i<n; i++){
        z1[i] = (char)sqlite3Toupper(z2[i]);//将小写字母转化为大写字母
      }
      sqlite3_result_text(context, z1, n, sqlite3_free);
    }
  }
}
static void lowerFunc(sqlite3_context *context, int argc, sqlite3_value **argv){ //将大写字符数据转换为小写字符数据后返回字符表达式。
  char *z1;
  const char *z2;
  int i, n;
  UNUSED_PARAMETER(argc);
  z2 = (char*)sqlite3_value_text(argv[0]);
  n = sqlite3_value_bytes(argv[0]);
  /* Verify that the call to _bytes() does not invalidate the _text() pointer
  验证调用_bytes()并不能否定_text()的指针*/
  assert( z2==(char*)sqlite3_value_text(argv[0]) );
  if( z2 ){
    z1 = contextMalloc(context, ((i64)n)+1);
    if( z1 ){
      for(i=0; i<n; i++){
        z1[i] = sqlite3Tolower(z2[i]);	//将大写字母转化为大写字母
      }
      sqlite3_result_text(context, z1, n, sqlite3_free);
    }
  }
}


#if 0  /* This function is never used. */
/*
** The COALESCE() and IFNULL() functions used to be implemented as shown
** here.  But now they are implemented as VDBE code so that unused arguments
** do not have to be computed.  This legacy implementation is retained as
** comment.
*/
/*
** Implementation of the IFNULL(), NVL(), and COALESCE() functions.  
** All three do the same thing.  They return the first non-NULL
** argument.
*/
static void ifnullFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int i;
  for(i=0; i<argc; i++){
    if( SQLITE_NULL!=sqlite3_value_type(argv[i]) ){
      sqlite3_result_value(context, argv[i]);
      break;
    }
  }
}
#endif /* NOT USED */
#define ifnullFunc versionFunc   /* Substitute function - never called 代用函数-从未调用*/

/*
** Implementation of random().  Return a random integer. 
SQLite RANDOM 函数返回一个介于 -9223372036854775808 和 +9223372036854775807 
之间的伪随机整数。
*/
static void randomFunc(	//返回随机数。
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  sqlite_int64 r;
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  sqlite3_randomness(sizeof(r), &r);
  if( r<0 ){
    /* We need to prevent a random number of 0x8000000000000000 
    ** (or -9223372036854775808) since when you do abs() of that
    ** number of you get the same value back again.  To do this
    ** in a way that is testable, mask the sign bit off of negative
    ** values, resulting in a positive value.  Then take the 
    ** 2s complement of that positive value.  The end result can
    ** therefore be no less than -9223372036854775807.
    我们需要防止随机数为0 x8000000000000000(或-9223372036854775808),
    因为当你用abs()时又回到相同的值。
    要做到这一点的方式是可测试的,掩盖了符号位的负值,结果为正值。
    然后给这2s 正值 补充。最终的结果因此可以不少于-9223372036854775807。
    */
    r = -(r & LARGEST_INT64);
  }
  sqlite3_result_int64(context, r);
}

/*
** Implementation of randomblob(N).  Return a random blob
** that is N bytes long.
randomblob(N )函数返回一个N-byte 二进制大对象blob

*/
static void randomBlob(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int n;
  unsigned char *p;
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  n = sqlite3_value_int(argv[0]);
  if( n<1 ){
    n = 1;
  }
  p = contextMalloc(context, n);
  if( p ){
    sqlite3_randomness(n, p);
    sqlite3_result_blob(context, (char*)p, n, sqlite3_free);//返回随机二进制大对象
  }
}

/*
** Implementation of the last_insert_rowid() SQL function.  The return
** value is the same as the sqlite3_last_insert_rowid() API function.
last_insert_rowid()SQL函数的实现。返回值是sqlite3_last_insert_rowid()API函数一样的。
*/
static void last_insert_rowid(	//返回last_insert_rowid( )ROWID 最后一个连接从数据库中插入的行, 
//该调用该函数last_insert_rowid()SQL 函数将被添加sqlite3_last_insert_rowid( )C/C++ 接口函数.
  sqlite3_context *context, 
  int NotUsed, 
  sqlite3_value **NotUsed2
){
  sqlite3 *db = sqlite3_context_db_handle(context);
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  /* IMP: R-51513-12026 The last_insert_rowid() SQL function is a
  ** wrapper around the sqlite3_last_insert_rowid() C/C++ interface
  ** function.
last_insert_rowid()的SQL函数是一个sqlite3_last_insert_rowid() C / c++接口功能 的封装器*/
  sqlite3_result_int64(context, sqlite3_last_insert_rowid(db));
}

/*
** Implementation of the changes() SQL function.
**
** IMP: R-62073-11209 The changes() SQL function is a wrapper
** around the sqlite3_changes() C/C++ function and hence follows the same
** rules for counting changes.
该函数返回最近执行的INSERT、UPDATE和DELETE语句所影响的数据行数。
我们也可以通过执行C/C++函数sqlite3_changes()得到相同的结果.
*/
static void changes(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  sqlite3 *db = sqlite3_context_db_handle(context);
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  sqlite3_result_int(context, sqlite3_changes(db));//返回数据行数
}

/*
** Implementation of the total_changes() SQL function.  The return value is
** the same as the sqlite3_total_changes() API function.
该函数返回自从该连接被打开时起，INSERT、UPDATE和DELETE语句总共影响的行数。
我们也可以通过C/C++接口函数sqlite3_total_changes()得到相同的结果。

*/
static void total_changes(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  sqlite3 *db = sqlite3_context_db_handle(context);
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  /* IMP: R-52756-41993 This function is a wrapper around the
  ** sqlite3_total_changes() C/C++ interface. 这个函数是一个sqlite3_total_changes()C / c++接口的封装包。*/
  sqlite3_result_int(context, sqlite3_total_changes(db));//返回总影响的行数
}

/*
** A structure defining how to do GLOB-style comparisons.
结构定义如何做GLOB-style比较。
*/
struct compareInfo {
  u8 matchAll;
  u8 matchOne;
  u8 matchSet;
  u8 noCase;
};

/*
** For LIKE and GLOB matching on EBCDIC machines, assume that every
** character is exactly one byte in size.  Also, all characters are
** able to participate in upper-case-to-lower-case mappings in EBCDIC
** whereas only characters less than 0x80 do in ASCII.
为使LIKE和GLOB操作在EBCDIC机上匹配，假设每一个字符都是完全大小的一个字节。
同时，所有的字符都能参与大写小写字符映射在EBCDIC而只有少于在ASCII码里的0x80。
*/
#if defined(SQLITE_EBCDIC)//EBCDIC是扩充的二进制编码的十进制交换码; 
# define sqlite3Utf8Read(A,C)  (*(A++))//读取要比较的两个字符A、C
# define GlogUpperToLower(A)   A = sqlite3UpperToLower[A]
#else
# define GlogUpperToLower(A)   if( !((A)&~0x7f) ){ A = sqlite3UpperToLower[A]; }
#endif

static const struct compareInfo globInfo = { '*', '?', '[', 0 };
/* The correct SQL-92 behavior is for the LIKE operator to ignore
** case.  Thus  'a' LIKE 'A' would be true. 
正确sql - 92行为是给LIKE操作符忽视例子。因此' a 'LIKE' A'是对的*/
static const struct compareInfo likeInfoNorm = { '%', '_',   0, 1 };
/* If SQLITE_CASE_SENSITIVE_LIKE is defined, then the LIKE operator
** is case sensitive causing 'a' LIKE 'A' to be false 
如果sqlite_case_sensitive_like被定义，那么LIKE操作符中的' a 'LIKE' A' 将是错的*/
static const struct compareInfo likeInfoAlt = { '%', '_',   0, 0 };

/*
** Compare two UTF-8 strings for equality where the first string can
** potentially be a "glob" expression.  Return true (1) if they
** are the same and false (0) if they are different.
**
** Globbing rules:
**
**      '*'       Matches any sequence of zero or more characters.
**
**      '?'       Matches exactly one character.
**
**     [...]      Matches one character from the enclosed list of
**                characters.
**
**     [^...]     Matches one character not in the enclosed list.
**
** With the [...] and [^...] matching, a ']' character can be included
** in the list by making it the first character after '[' or '^'.  A
** range of characters can be specified using '-'.  Example:
** "[a-z]" matches any single lower-case letter.  To match a '-', make
** it the last character in the list.
**
** This routine is usually quick, but can be N**2 in the worst case.
**
** Hints: to match '*' or '?', put them in "[]".  Like this:
**
**         abc[*]xyz        Matches "abc*xyz" only
比较两个UTF-8字符串相等，第一个字符串可以
可能是一个“glob”的表达。返回true（1）如果他们
是相同的错误（0）如果他们是不同的。
globbing支持的通配符：
“*”匹配任何序列的零个或多个字符。
“？”匹配一个字符。
【……]匹配一个字符从封闭列表特征。
【^……]匹配一个字符不在封闭列表。
与[……]和[ ^……]匹配，“]”字符可以包括
在列表中的第一个字符后，“它[’或‘^’。一个
字符的范围可以指定使用“-”。例：
“[a-z]”匹配任何一个小写字母。匹配一个“-”，成为
它的最后一个字符列表。
这个程序通常是快速的，但可能是n××2在最坏的情况下。
提示：匹配“*”或“？”，把它们放在“[ ]”。像这样的：
abc [*] xyz比赛“abc * xyz”
*/
static int patternCompare(
  const u8 *zPattern,              /* The glob pattern 通配符匹配操作符模式*/
  const u8 *zString,               /* The string to compare against the glob 对通配符匹配操作符的字符串比较*/
  const struct compareInfo *pInfo, /* Information about how to do the compare 信息如何做比较*/
  u32 esc                          /* The escape character 转义字符*/
){
  u32 c, c2;
  int invert;
  int seen;
  u8 matchOne = pInfo->matchOne;
  u8 matchAll = pInfo->matchAll;
  u8 matchSet = pInfo->matchSet;
  u8 noCase = pInfo->noCase; 
  int prevEscape = 0;     /* True if the previous character was 'escape' 如果前面的字符是转义的，则为真*/

  while( (c = sqlite3Utf8Read(zPattern,&zPattern))!=0 ){//用utf8格式读取的字符不为空时
    if( !prevEscape && c==matchAll ){
      while( (c=sqlite3Utf8Read(zPattern,&zPattern)) == matchAll
               || c == matchOne ){
        if( c==matchOne && sqlite3Utf8Read(zString, &zString)==0 ){
          return 0;
        }
      }
      if( c==0 ){
        return 1;
      }else if( c==esc ){
        c = sqlite3Utf8Read(zPattern, &zPattern);
        if( c==0 ){
          return 0;
        }
      }else if( c==matchSet ){
        assert( esc==0 );         /* This is GLOB, not LIKE 这是GLOB,不是LIKE操作符*/
        assert( matchSet<0x80 );  /* '[' is a single-byte character “[”是一个单字节字符*/
        while( *zString && patternCompare(&zPattern[-1],zString,pInfo,esc)==0 ){
          SQLITE_SKIP_UTF8(zString);
        }
        return *zString!=0;
      }
      while( (c2 = sqlite3Utf8Read(zString,&zString))!=0 ){
        if( noCase ){
          GlogUpperToLower(c2);
          GlogUpperToLower(c);
          while( c2 != 0 && c2 != c ){
            c2 = sqlite3Utf8Read(zString, &zString);
            GlogUpperToLower(c2);
          }
        }else{
          while( c2 != 0 && c2 != c ){
            c2 = sqlite3Utf8Read(zString, &zString);
          }
        }
        if( c2==0 ) return 0;
        if( patternCompare(zPattern,zString,pInfo,esc) ) return 1;
      }
      return 0;
    }else if( !prevEscape && c==matchOne ){
      if( sqlite3Utf8Read(zString, &zString)==0 ){
        return 0;
      }
    }else if( c==matchSet ){
      u32 prior_c = 0;
      assert( esc==0 );    /* This only occurs for GLOB, not LIKE  这仅对GLOB出现,不是LIKE操作符*/
      seen = 0;
      invert = 0;
      c = sqlite3Utf8Read(zString, &zString);
      if( c==0 ) return 0;
      c2 = sqlite3Utf8Read(zPattern, &zPattern);
      if( c2=='^' ){
        invert = 1;
        c2 = sqlite3Utf8Read(zPattern, &zPattern);
      }
      if( c2==']' ){
        if( c==']' ) seen = 1;
        c2 = sqlite3Utf8Read(zPattern, &zPattern);
      }
      while( c2 && c2!=']' ){
        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){
          c2 = sqlite3Utf8Read(zPattern, &zPattern);
          if( c>=prior_c && c<=c2 ) seen = 1;
          prior_c = 0;
        }else{
          if( c==c2 ){
            seen = 1;
          }
          prior_c = c2;
        }
        c2 = sqlite3Utf8Read(zPattern, &zPattern);
      }
      if( c2==0 || (seen ^ invert)==0 ){
        return 0;
      }
    }else if( esc==c && !prevEscape ){
      prevEscape = 1;
    }else{
      c2 = sqlite3Utf8Read(zString, &zString);
      if( noCase ){
        GlogUpperToLower(c);
        GlogUpperToLower(c2);
      }
      if( c!=c2 ){
        return 0;
      }
      prevEscape = 0;
    }
  }
  return *zString==0;
}

/*
** Count the number of times that the LIKE operator (or GLOB which is
** just a variation of LIKE) gets called.  This is used for testing
** only.
计算LIKE操作符(或仅是一个LIKE操作符的变体的GLOB)调用的次数。
这仅仅是用于测试。
*/
#ifdef SQLITE_TEST
int sqlite3_like_count = 0;//初始化
#endif


/*
** Implementation of the like() SQL function.  This function implements
** the build-in LIKE operator.  The first argument to the function is the
** pattern and the second argument is the string.  So, the SQL statements:
**
**       A LIKE B
**
** is implemented as like(B,A).
**
** This same function (with a different compareInfo structure) computes
** the GLOB operator.
实现SQL like()函数，这个函数实现了内置的LIKE操作符。
函数的第一个参数是模式和第二个参数是字符串。
因此SOL语句为 A LIKE B 被实现为like(B,A).
同样的功能(不同的compareInfo结构)计算通配符匹配操作符运算符。
*/
static void likeFunc(//确定给定的字符串是否与指定的模式匹配。
  sqlite3_context *context, 
  int argc, 
  sqlite3_value **argv
){
  const unsigned char *zA, *zB;
  u32 escape = 0;
  int nPat;
  sqlite3 *db = sqlite3_context_db_handle(context);

  zB = sqlite3_value_text(argv[0]);
  zA = sqlite3_value_text(argv[1]);

  /* Limit the length of the LIKE or GLOB pattern to avoid problems
  ** of deep recursion and N*N behavior in patternCompare().
  限制LIKE 或 GLOB 模式的长度以避免在patternCompare()里的 深度递归和 N*N 行为
  */
  nPat = sqlite3_value_bytes(argv[0]);
  testcase( nPat==db->aLimit[SQLITE_LIMIT_LIKE_PATTERN_LENGTH] );
  testcase( nPat==db->aLimit[SQLITE_LIMIT_LIKE_PATTERN_LENGTH]+1 );
  if( nPat > db->aLimit[SQLITE_LIMIT_LIKE_PATTERN_LENGTH] ){
    sqlite3_result_error(context, "LIKE or GLOB pattern too complex", -1);
    return;
  }
  assert( zB==sqlite3_value_text(argv[0]) );  /* Encoding did not change 编码没有改变*/

  if( argc==3 ){
    /* The escape character string must consist of a single UTF-8 character.
    ** Otherwise, return an error.
    转义字符的字符串必须包括一个单UTF-8字符。
    否则，返回一个错误。
    */
    const unsigned char *zEsc = sqlite3_value_text(argv[2]);
    if( zEsc==0 ) return;
    if( sqlite3Utf8CharLen((char*)zEsc, -1)!=1 ){
      sqlite3_result_error(context, 
          "ESCAPE expression must be a single character", -1);
      return;
    }
    escape = sqlite3Utf8Read(zEsc, &zEsc);
  }
  if( zA && zB ){
    struct compareInfo *pInfo = sqlite3_user_data(context);
#ifdef SQLITE_TEST
    sqlite3_like_count++;//LIKE 操作符的次数加1
#endif
    
    sqlite3_result_int(context, patternCompare(zB, zA, pInfo, escape));
  }
}

/*
** Implementation of the NULLIF(x,y) function.  The result is the first
** argument if the arguments are different.  The result is NULL if the
** arguments are equal to each other.
实现NULLIF(x,y)函数。如果参数是不同的，则结果是第一个参数。
如果参数是相等，则结果为空
*/
static void nullifFunc(//如果函数参数相同，返回NULL，否则返回第一个参数。
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **argv
){
  CollSeq *pColl = sqlite3GetFuncCollSeq(context);//调用sqlite3GetFuncCollSeq(sqlite3_context *context)函数
  UNUSED_PARAMETER(NotUsed);
  if( sqlite3MemCompare(argv[0], argv[1], pColl)!=0 ){//比较两个参数是否相等
    sqlite3_result_value(context, argv[0]);
  }
}

/*
** Implementation of the sqlite_version() function.  The result is the version
** of the SQLite library that is running.
sqlite_version()功能的实现。结果是SQLite库正在运行的版本。
*/
static void versionFunc(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  /* IMP: R-48699-48617 This function is an SQL wrapper around the
  ** sqlite3_libversion() C-interface.这个函数是一个关于sqlite3_libversion()c接口的SQL封装 */
  sqlite3_result_text(context, sqlite3_libversion(), -1, SQLITE_STATIC);
}

/*
** Implementation of the sqlite_source_id() function. The result is a string
** that identifies the particular version of the source code used to build
** SQLite.
实现sqlite_source_id()函数。结果是一个字符串,
该字符串标识用于构建SQLite源代码的特定版本
*/
static void sourceidFunc(
  sqlite3_context *context,
  int NotUsed,
  sqlite3_value **NotUsed2
){
  UNUSED_PARAMETER2(NotUsed, NotUsed2);
  /* IMP: R-24470-31136 This function is an SQL wrapper around the
  ** sqlite3_sourceid() C interface. 此函数是该函数的周围的SQL 包装sqlite3_libversion( )c接口*/
  sqlite3_result_text(context, sqlite3_sourceid(), -1, SQLITE_STATIC);
}

/*
** Implementation of the sqlite_compileoption_used() function.
** The result is an integer that identifies if the compiler option
** was used to build SQLite.
sqlite_compileoption_used()功能实现。其结果是一个整数，
,该整数标识用于构建SQLite的编译器选项。
*/
#ifndef SQLITE_OMIT_COMPILEOPTION_DIAGS
static void compileoptionusedFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const char *zOptName;
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  /* IMP: R-39564-36305 The sqlite_compileoption_used() SQL
  ** function is a wrapper around the sqlite3_compileoption_used() C/C++
  ** function.此函数是该函数的周围的SQL 包装sqlite3_compileoption_used() C/C++接口
  */
  if( (zOptName = (const char*)sqlite3_value_text(argv[0]))!=0 ){
    sqlite3_result_int(context, sqlite3_compileoption_used(zOptName));
  }
}
#endif /* SQLITE_OMIT_COMPILEOPTION_DIAGS */

/*
** Implementation of the sqlite_compileoption_get() function. 
** The result is a string that identifies the compiler options 
** used to build SQLite.
sqlite_compileoption_get() 功能实现。其结果是一个字符串，
,该字符串标识用于构建SQLite的编译器选项。
*/
#ifndef SQLITE_OMIT_COMPILEOPTION_DIAGS
static void compileoptiongetFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int n;
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  /* IMP: R-04922-24076 The sqlite_compileoption_get() SQL function
  ** is a wrapper around the sqlite3_compileoption_get() C/C++ function.
 此函数是该函数的周围的SQL 包装sqlite3_compileoption_get() C/C++接口
  */
  n = sqlite3_value_int(argv[0]);
  sqlite3_result_text(context, sqlite3_compileoption_get(n), -1, SQLITE_STATIC);
}
#endif /* SQLITE_OMIT_COMPILEOPTION_DIAGS */

/* Array for converting from half-bytes (nybbles) into ASCII hex
** digits.从半字节数组转换为ASCII码的十六进制数字（nybbles）。 */
static const char hexdigits[] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' 
};

/*
** EXPERIMENTAL - This is not an official function.  The interface may
** change.  This function may disappear.  Do not write code that depends
** on this function.
**
** Implementation of the QUOTE() function.  This function takes a single
** argument.  If the argument is numeric, the return value is the same as
** the argument.  If the argument is NULL, the return value is the string
** "NULL".  Otherwise, the argument is enclosed in single quotes with
** single-quote escapes.
实验-这是不是一个正式的功能。该接口可以改变。这个功能可能会消失。不写代码，取决于该函数。
quote()功能的实现。这个函数使用一个参数。如果参数为数值，返回值和参数相同。
如果参数为空，返回值是字符串“空”。否则，该参数含在单引号转义引号
*/
static void quoteFunc(sqlite3_context *context, int argc, sqlite3_value **argv){//quote( )函数返回一个字符串, 
//它是对参数的值适合于网络部署管理器另一个SQL 语句字符串是合法的内部引号,
//将应用程序部署到single-quotes 与转码BLOBs是编码的十六进制数文字
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  switch( sqlite3_value_type(argv[0]) ){
    case SQLITE_FLOAT: {
      double r1, r2;
      char zBuf[50];
      r1 = sqlite3_value_double(argv[0]);
      sqlite3_snprintf(sizeof(zBuf), zBuf, "%!.15g", r1);
      sqlite3AtoF(zBuf, &r2, 20, SQLITE_UTF8);
      if( r1!=r2 ){
        sqlite3_snprintf(sizeof(zBuf), zBuf, "%!.20e", r1);
      }
      sqlite3_result_text(context, zBuf, -1, SQLITE_TRANSIENT);
      break;
    }
    case SQLITE_INTEGER: {//整数
      sqlite3_result_value(context, argv[0]);
      break;
    }
    case SQLITE_BLOB: {//二进制大对象
      char *zText = 0;
      char const *zBlob = sqlite3_value_blob(argv[0]);
      int nBlob = sqlite3_value_bytes(argv[0]);
      assert( zBlob==sqlite3_value_blob(argv[0]) ); /* No encoding change 没有编码的变化*/
      zText = (char *)contextMalloc(context, (2*(i64)nBlob)+4); 
      if( zText ){
        int i;
        for(i=0; i<nBlob; i++){
          zText[(i*2)+2] = hexdigits[(zBlob[i]>>4)&0x0F];
          zText[(i*2)+3] = hexdigits[(zBlob[i])&0x0F];
        }
        zText[(nBlob*2)+2] = '\'';
        zText[(nBlob*2)+3] = '\0';
        zText[0] = 'X';
        zText[1] = '\'';
        sqlite3_result_text(context, zText, -1, SQLITE_TRANSIENT);
        sqlite3_free(zText);
      }
      break;
    }
    case SQLITE_TEXT: {//文本
      int i,j;
      u64 n;
      const unsigned char *zArg = sqlite3_value_text(argv[0]);
      char *z;

      if( zArg==0 ) return;
      for(i=0, n=0; zArg[i]; i++){ if( zArg[i]=='\'' ) n++; }
      z = contextMalloc(context, ((i64)i)+((i64)n)+3);
      if( z ){
        z[0] = '\'';
        for(i=0, j=1; zArg[i]; i++){
          z[j++] = zArg[i];
          if( zArg[i]=='\'' ){
            z[j++] = '\'';
          }
        }
        z[j++] = '\'';
        z[j] = 0;
        sqlite3_result_text(context, z, j, sqlite3_free);
      }
      break;
    }
    default: {
      assert( sqlite3_value_type(argv[0])==SQLITE_NULL );//断言类型为空
      sqlite3_result_text(context, "NULL", 4, SQLITE_STATIC);
      break;
    }
  }
}

/*
** The hex() function.  Interpret the argument as a blob.  Return
** a hexadecimal rendering as text.
hex()函数，解释的参数作为BLOB。返回十六进制显示为文本。
hex()函数，解释的参数作为BLOB(binary large object二进制大对象）。返回呈现一个十六进制文本。
*/
static void hexFunc(//将其参数hex( )函数作为BLOB 并返回一个字符串, 它呈现的内容是大写十六进制blob
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int i, n;
  const unsigned char *pBlob;
  char *zHex, *z;
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  pBlob = sqlite3_value_blob(argv[0]);
  n = sqlite3_value_bytes(argv[0]);
  assert( pBlob==sqlite3_value_blob(argv[0]) );  /* No encoding change 没有编码的变化*/
  z = zHex = contextMalloc(context, ((i64)n)*2 + 1);//调用 contextMalloc();
  if( zHex ){
    for(i=0; i<n; i++, pBlob++){
      unsigned char c = *pBlob;
      *(z++) = hexdigits[(c>>4)&0xf];
      *(z++) = hexdigits[c&0xf];
    }
    *z = 0;
    sqlite3_result_text(context, zHex, n*2, sqlite3_free);//返回文本类型
  }
}

/*
** The zeroblob(N) function returns a zero-filled blob of size N bytes.
zeroblob(N)函数返回一个N个字节大小的zero-filled blob。
zeroblob(N)函数返回一个N个字节大小的被零填充的blob。
*/
static void zeroblobFunc(//zeroblob(n )函数返回一个BLOB n字节的0x00 组成的SQLite这些zeroblobs 
//非常高效地管理zeroblobs可用于编写的BLOB 的更高版本预留空间使用增量BLOB I/O 
//此SQL 函数使用的sqlite3_result_zeroblob( )从C/C++ 接口例程
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  i64 n;
  sqlite3 *db = sqlite3_context_db_handle(context);
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  n = sqlite3_value_int64(argv[0]);
  testcase( n==db->aLimit[SQLITE_LIMIT_LENGTH] );
  testcase( n==db->aLimit[SQLITE_LIMIT_LENGTH]+1 );
  if( n>db->aLimit[SQLITE_LIMIT_LENGTH] ){
    sqlite3_result_error_toobig(context);
  }else{
    sqlite3_result_zeroblob(context, (int)n); /* IMP: R-00293-64994 */
  }
}

/*
** The replace() function.  Three arguments are all strings: call
** them A, B, and C. The result is also a string which is derived
** from A by replacing every occurance of B with C.  The match
** must be exact.  Collating sequences are not used.
replace()函数。三个参数都是字符串:称之为A,B,c .结果也是字符串,这个字符串来自A，通过取代每一个出现B和C得来。
必须精确匹配。排序序列不被使用。
*/
static void replaceFunc(//replace() 函数所返回一个字符串, 该字符串通过字符串替换形成的字符串。函数所返回一个字符串，该字符串是通过字符串替换形成的字符串
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const unsigned char *zStr;        /* The input string A 输入字符串A*/
  const unsigned char *zPattern;    /* The pattern string B 模式字符串B */
  const unsigned char *zRep;        /* The replacement string C 替换字符串C*/
  unsigned char *zOut;              /* The output输出 */
  int nStr;                /* Size of zStr Al输入字符串的大小*/
  int nPattern;            /* Size of zPattern 模式字符串的大小*/
  int nRep;                /* Size of zRep 替换字符串的大小*/
  i64 nOut;                /* Maximum size of zOut 输出字符串的最大大小*/
  int loopLimit;           /* Last zStr[] that might match zPattern[] 最后 zStr[] 可能匹配的zPattern[] */
  int i, j;                /* Loop counters 循环计数*/

  assert( argc==3 );
  UNUSED_PARAMETER(argc);
  zStr = sqlite3_value_text(argv[0]);
  if( zStr==0 ) return;
  nStr = sqlite3_value_bytes(argv[0]);//l输入字符串
  assert( zStr==sqlite3_value_text(argv[0]) );  /* No encoding change 没有编码的变化*/
  zPattern = sqlite3_value_text(argv[1]);
  if( zPattern==0 ){
    assert( sqlite3_value_type(argv[1])==SQLITE_NULL
            || sqlite3_context_db_handle(context)->mallocFailed );
    return;
  }
  if( zPattern[0]==0 ){
    assert( sqlite3_value_type(argv[1])!=SQLITE_NULL );
    sqlite3_result_value(context, argv[0]);
    return;
  }
  nPattern = sqlite3_value_text(argv[1]);
  if( zPattern==0 ){//模式字符串为空
    assert( sqlite3_value_type(argv[1])==SQLITE_NULL
            || sqlite3_context_db_handle(context)->mallocFailed );
    return;
  }
  if( zPattern[0]==0 ){
    assert( sqlite3_value_type(argv[1])!=SQLITE_NULL );
    sqlite3_result_value(context, argv[0]);
    return;
  }
  nPattern = sqlite3_value_bytes(argv[1]);
  assert( zPattern==sqlite3_value_text(argv[1]) );  /* No encoding change 没有编码的变化*/
  zRep = sqlite3_value_text(argv[2]);//替换字符串
  if( zRep==0 ) return;
  nRep = sqlite3_value_bytes(argv[2]);
  assert( zRep==sqlite3_value_text(argv[2]) );
  nOut = nStr + 1;
  assert( nOut<SQLITE_MAX_LENGTH );
  zOut = contextMalloc(context, (i64)nOut);
  if( zOut==0 ){
    return;
  }
  loopLimit = nStr - nPattern;  
  for(i=j=0; i<=loopLimit; i++){
    if( zStr[i]!=zPattern[0] || memcmp(&zStr[i], zPattern, nPattern) ){
      zOut[j++] = zStr[i];
    }else{
      u8 *zOld;
      sqlite3 *db = sqlite3_context_db_handle(context);
      nOut += nRep - nPattern;
      testcase( nOut-1==db->aLimit[SQLITE_LIMIT_LENGTH] );
      testcase( nOut-2==db->aLimit[SQLITE_LIMIT_LENGTH] );
      if( nOut-1>db->aLimit[SQLITE_LIMIT_LENGTH] ){
        sqlite3_result_error_toobig(context);
        sqlite3_free(zOut);
        return;
      }
      zOld = zOut;
      zOut = sqlite3_realloc(zOut, (int)nOut);
      if( zOut==0 ){
        sqlite3_result_error_nomem(context);
        sqlite3_free(zOld);
        return;
      }
      memcpy(&zOut[j], zRep, nRep);
      j += nRep;
      i += nPattern-1;
    }
  }
  assert( j+nStr-i+1==nOut );
  memcpy(&zOut[j], &zStr[i], nStr-i);
  j += nStr - i;
  assert( j<=nOut );
  zOut[j] = 0;
  sqlite3_result_text(context, (char*)zOut, j, sqlite3_free);
}

/*
** Implementation of the TRIM(), LTRIM(), and RTRIM() functions.
** The userdata is 0x1 for left trim, 0x2 for right trim, 0x3 for both.
实现TRIM(), LTRIM(), and RTRIM()函数。用户数据是0x1代表左侧空格符移除，
0x2代表右侧空格符移除即尾部的空格符清除，0x3代表两侧空格符都移除。
实现TRIM(), LTRIM(), and RTRIM()函数。用户数据是0x1代表左对齐，
0x2代表右对齐，0x3代表两端对齐。
*/
static void trimFunc(//trim() 函数所返回一个字符串
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const unsigned char *zIn;         /* Input string 输入字符串*/
  const unsigned char *zCharSet;    /* Set of characters to trim 设置移除类型*/
  int nIn;                          /* Number of bytes in input 输入字节数*/
  int flags;                        /* 1: trimleft  2: trimright  3: trim 标志1 ，左侧字符移除，标志2 ，右侧字符移除， 标志3 ，两侧字符移除*/
  int i;                            /* Loop counter 循环计数*/
  unsigned char *aLen = 0;          /* Length of each character in zCharSet zCharSet每个字符的长度*/
  unsigned char **azChar = 0;       /* Individual characters in zCharSet zCharSet单个字符*/
  int nChar;                        /* Number of characters in zCharSet zCharSet字符数*/

  if( sqlite3_value_type(argv[0])==SQLITE_NULL ){
    return;
  }
  zIn = sqlite3_value_text(argv[0]);
  if( zIn==0 ) return;//判断字符串是否为空
  nIn = sqlite3_value_bytes(argv[0]);
  assert( zIn==sqlite3_value_text(argv[0]) );
  if( argc==1 ){
    static const unsigned char lenOne[] = { 1 };
    static unsigned char * const azOne[] = { (u8*)" " };
    nChar = 1;
    aLen = (u8*)lenOne;
    azChar = (unsigned char **)azOne;
    zCharSet = 0;
  }else if( (zCharSet = sqlite3_value_text(argv[1]))==0 ){
    return;
  }else{
    const unsigned char *z;
    for(z=zCharSet, nChar=0; *z; nChar++){
      SQLITE_SKIP_UTF8(z);
    }
    if( nChar>0 ){
      azChar = contextMalloc(context, ((i64)nChar)*(sizeof(char*)+1));
      if( azChar==0 ){
        return;
      }
      aLen = (unsigned char*)&azChar[nChar];//移除类型的长度
      for(z=zCharSet, nChar=0; *z; nChar++){
        azChar[nChar] = (unsigned char *)z;
        SQLITE_SKIP_UTF8(z);
        aLen[nChar] = (u8)(z - azChar[nChar]);
      }
    }
  }
  if( nChar>0 ){
    flags = SQLITE_PTR_TO_INT(sqlite3_user_data(context));//移除标志
    if( flags & 1 ){//标志1 ，左侧字符移除
      while( nIn>0 ){
        int len = 0;
        for(i=0; i<nChar; i++){
          len = aLen[i];
          if( len<=nIn && memcmp(zIn, azChar[i], len)==0 ) break;
        }
        if( i>=nChar ) break;
        zIn += len;
        nIn -= len;
      }
    }
    if( flags & 2 ){//标志2 ，右侧字符移除
      while( nIn>0 ){
        int len = 0;
        for(i=0; i<nChar; i++){
          len = aLen[i];
          if( len<=nIn && memcmp(&zIn[nIn-len],azChar[i],len)==0 ) break;
        }
        if( i>=nChar ) break;
        nIn -= len;
      }
    }
    if( zCharSet ){
      sqlite3_free(azChar);
    }
  }
  sqlite3_result_text(context, (char*)zIn, nIn, SQLITE_TRANSIENT);//返回最后移除的字符串
}


/* IMP: R-25361-16150 This function is omitted from SQLite by default. It
** is only available if the SQLITE_SOUNDEX compile-time option is used
** when SQLite is built.
这个函数是省略了默认SQLite。它
只有SQLITE_SOUNDEX编译时选项时该函数才可用
SQLite默认省略这个函数。当构建SQLite时，如果SQLITE_SOUNDEX 编译时选择使用，这个函数才可用。
*/
#ifdef SQLITE_SOUNDEX
/*
** Compute the soundex encoding of a word.计算一个字的Soundex编码
**
** IMP: R-59782-00072 The soundex(X) function returns a string that is the
** soundex encoding of the string X. 
soundex(X)函数返回一个字符串,该字符串是字符串X的soundex编码。
*/
static void soundexFunc(//	计算字符串X的soundex编码。
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  char zResult[8];
  const u8 *zIn;
  int i, j;
  static const unsigned char iCode[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,
    1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,
    0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,
    1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,
  };
  assert( argc==1 );
  zIn = (u8*)sqlite3_value_text(argv[0]);
  if( zIn==0 ) zIn = (u8*)"";
  for(i=0; zIn[i] && !sqlite3Isalpha(zIn[i]); i++){}
  if( zIn[i] ){
    u8 prevcode = iCode[zIn[i]&0x7f];
    zResult[0] = sqlite3Toupper(zIn[i]);//小写字母转化为大写字母函数，对UTF-8字符不能提供好的支持。
    for(j=1; j<4 && zIn[i]; i++){
      int code = iCode[zIn[i]&0x7f];
      if( code>0 ){
        if( code!=prevcode ){
          prevcode = code;
          zResult[j++] = code + '0';
        }
      }else{
        prevcode = 0;
      }
    }
    while( j<4 ){
      zResult[j++] = '0';
    }
    zResult[j] = 0;
    sqlite3_result_text(context, zResult, 4, SQLITE_TRANSIENT);
  }else{
    /* IMP: R-64894-50321 The string "?000" is returned if the argument
    ** is NULL or contains no ASCII alphabetic characters.参数为NULL或不包含ASCII字母字符时返回字符串"?000".
        如果参数是NULL或者不包含ASCII字母字符，那么返回字符串"？000" 。     
	  */
    sqlite3_result_text(context, "?000", 4, SQLITE_STATIC);
  }
}
#endif /* SQLITE_SOUNDEX */

#ifndef SQLITE_OMIT_LOAD_EXTENSION
/*
** A function that loads a shared-library extension then returns NULL.
一个加载共享库的函数扩展然后返回NULL。

*/
static void loadExt(sqlite3_context *context, int argc, sqlite3_value **argv){
  const char *zFile = (const char *)sqlite3_value_text(argv[0]);
  const char *zProc;
  sqlite3 *db = sqlite3_context_db_handle(context);
  char *zErrMsg = 0;

  if( argc==2 ){
    zProc = (const char *)sqlite3_value_text(argv[1]);
  }else{
    zProc = 0;
  }
  if( zFile && sqlite3_load_extension(db, zFile, zProc, &zErrMsg) ){
    sqlite3_result_error(context, zErrMsg, -1);
    sqlite3_free(zErrMsg);
  }
}
#endif


/*
** An instance of the following structure holds the context of a
** sum() or avg() aggregate computation.
以下结构的实例持有的上下文中sum()或avg()聚集计算。
*/
typedef struct SumCtx SumCtx;
struct SumCtx {
  double rSum;      /* Floating point sum 浮点数的总和*/
  i64 iSum;         /* Integer sum 整数总和*/   
  i64 cnt;          /* Number of elements summed 元素数量的总和*/
  u8 overflow;      /* True if integer overflow seen 如果整数溢出，是真的*/
  u8 approx;        /* True if non-integer value was input to the sum 如果当输入均为非整数时，和是精确的*/
};

/*
** Routines used to compute the sum, average, and total.
**
** The SUM() function follows the (broken) SQL standard which means
** that it returns NULL if it sums over no inputs.  TOTAL returns
** 0.0 in that case.  In addition, TOTAL always returns a float where
** SUM might return an integer if it never encounters a floating point
** value.  TOTAL never fails, but SUM might through an exception if
** it overflows an integer.
例子用计算总和，平均值和总和。
该sum()功能如下（broken）SQL标准，这意味着
如果总和没有输入，返回null。在这种情况下TOTAL返回0
此外，TOTAL总是返回一个浮点数，在这里若SUM从没有遇到一个浮点数，SUM可能返回一个整数。
TOTAL总不会失败，但sum可能出现一个异常如果它溢出整数。
*/
static void sumStep(sqlite3_context *context, int argc, sqlite3_value **argv){
  SumCtx *p;
  int type;
  assert( argc==1 );
  UNUSED_PARAMETER(argc);
  p = sqlite3_aggregate_context(context, sizeof(*p));
  type = sqlite3_value_numeric_type(argv[0]);
  if( p && type!=SQLITE_NULL ){
    p->cnt++;
    if( type==SQLITE_INTEGER ){
      i64 v = sqlite3_value_int64(argv[0]);
      p->rSum += v;
      if( (p->approx|p->overflow)==0 && sqlite3AddInt64(&p->iSum, v) ){
        p->overflow = 1;
      }
    }else{
      p->rSum += sqlite3_value_double(argv[0]);//浮点数的总和
      p->approx = 1;
    }
  }
}
static void sumFinalize(sqlite3_context *context){
  SumCtx *p;
  p = sqlite3_aggregate_context(context, 0);
  if( p && p->cnt>0 ){
    if( p->overflow ){
      sqlite3_result_error(context,"integer overflow",-1);//返回错误整数溢出为-1
    }else if( p->approx ){
      sqlite3_result_double(context, p->rSum);//如果不是整数时返回浮点数的总和
    }else{
      sqlite3_result_int64(context, p->iSum);//反之返回整数总和
    }
  }
}
static void avgFinalize(sqlite3_context *context){//平均值
  SumCtx *p;
  p = sqlite3_aggregate_context(context, 0);
  if( p && p->cnt>0 ){
    sqlite3_result_double(context, p->rSum/(double)p->cnt);
  }
}
static void totalFinalize(sqlite3_context *context){
  SumCtx *p;
  p = sqlite3_aggregate_context(context, 0);
  /* (double)0 In case of SQLITE_OMIT_FLOATING_POINT... (double)0为缺省浮点值*/
  sqlite3_result_double(context, p ? p->rSum : (double)0);
}

/*
** The following structure keeps track of state information for the
** count() aggregate function.
以下的结构保持跟踪状态信息的count()聚合函数。
*/
typedef struct CountCtx CountCtx;
struct CountCtx {
  i64 n;
};

/*
** Routines to implement the count() aggregate function.
例程来实现count()聚合函数。

*/
static void countStep(sqlite3_context *context, int argc, sqlite3_value **argv){
  CountCtx *p;
  p = sqlite3_aggregate_context(context, sizeof(*p));
  if( (argc==0 || SQLITE_NULL!=sqlite3_value_type(argv[0])) && p ){
    p->n++;
  }

#ifndef SQLITE_OMIT_DEPRECATED
  /* The sqlite3_aggregate_count() function is deprecated.  But just to make
  ** sure it still operates correctly, verify that its count agrees with our 
  ** internal count when using count(*) and when the total count can be
  ** expressed as a 32-bit integer.
  sqlite3_aggregate_count()函数被弃用。 只是以保证它仍然运行正确,
  验证其同意我们内部计数时使用count(*)和total计数可以表示为一个32位整数。*/
  assert( argc==1 || p==0 || p->n>0x7fffffff
          || p->n==sqlite3_aggregate_count(context) );
#endif
}   
static void countFinalize(sqlite3_context *context){
  CountCtx *p;
  p = sqlite3_aggregate_context(context, 0);
  sqlite3_result_int64(context, p ? p->n : 0);
}

/*
** Routines to implement min() and max() aggregate functions.
例程来实现min() 和 max() 聚合函数。
*/
static void minmaxStep(
  sqlite3_context *context, 
  int NotUsed, 
  sqlite3_value **argv
){
  Mem *pArg  = (Mem *)argv[0];
  Mem *pBest;
  UNUSED_PARAMETER(NotUsed);

  pBest = (Mem *)sqlite3_aggregate_context(context, sizeof(*pBest));
  if( !pBest ) return;

  if( sqlite3_value_type(argv[0])==SQLITE_NULL ){
    if( pBest->flags ) sqlite3SkipAccumulatorLoad(context);
  }else if( pBest->flags ){
    int max;
    int cmp;
    CollSeq *pColl = sqlite3GetFuncCollSeq(context);//调觭qlite3GetFuncCollSeq(context)函数
    /* This step function is used for both the min() and max() aggregates,
    ** the only difference between the two being that the sense of the
    ** comparison is inverted. For the max() aggregate, the
    ** sqlite3_user_data() function returns (void *)-1. For min() it
    ** returns (void *)db, where db is the sqlite3* database pointer.
    ** Therefore the next statement sets variable 'max' to 1 for the max()
    ** aggregate, or 0 for min().
    这阶梯函数用于min()和max()聚集,两者之间唯一的区别是,比较的意义是倒置的。
    对于max()聚合函数,sqlite3_user_data()函数返回(void *)-1。对于min(),它返回(void *)db,db数据库sqlite3 *指针。
    因此下一个语句集变量“max”max()聚合为1 ,或min()为0。
    */
    max = sqlite3_user_data(context)!=0;
    cmp = sqlite3MemCompare(pBest, pArg, pColl);
    if( (max && cmp<0) || (!max && cmp>0) ){
      sqlite3VdbeMemCopy(pBest, pArg);
    }else{
      sqlite3SkipAccumulatorLoad(context);//调用 sqlite3SkipAccumulatorLoad(context)函数
    }
  }else{
    sqlite3VdbeMemCopy(pBest, pArg);
  }
}
static void minMaxFinalize(sqlite3_context *context){
  sqlite3_value *pRes;
  pRes = (sqlite3_value *)sqlite3_aggregate_context(context, 0);
  if( pRes ){
    if( pRes->flags ){
      sqlite3_result_value(context, pRes);
    }
    sqlite3VdbeMemRelease(pRes);
  }
}

/*
** group_concat(EXPR, ?SEPARATOR?)
这个功能返回的结果集是一个group里面的非空值组成的字符串，
如果group里全是空值的时候它返回的是NULL。
*/
static void groupConcatStep(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const char *zVal;
  StrAccum *pAccum;
  const char *zSep;
  int nVal, nSep;
  assert( argc==1 || argc==2 );
  if( sqlite3_value_type(argv[0])==SQLITE_NULL ) return;//group里全是空值的时候它返回的是NULL。
  pAccum = (StrAccum*)sqlite3_aggregate_context(context, sizeof(*pAccum));

  if( pAccum ){
    sqlite3 *db = sqlite3_context_db_handle(context);
    int firstTerm = pAccum->useMalloc==0;
    pAccum->useMalloc = 2;
    pAccum->mxAlloc = db->aLimit[SQLITE_LIMIT_LENGTH];
    if( !firstTerm ){
      if( argc==2 ){
        zSep = (char*)sqlite3_value_text(argv[1]);
        nSep = sqlite3_value_bytes(argv[1]);
      }else{
        zSep = ",";
        nSep = 1;
      }
      sqlite3StrAccumAppend(pAccum, zSep, nSep);
    }
    zVal = (char*)sqlite3_value_text(argv[0]);
    nVal = sqlite3_value_bytes(argv[0]);
    sqlite3StrAccumAppend(pAccum, zVal, nVal);
  }
}
static void groupConcatFinalize(sqlite3_context *context){
  StrAccum *pAccum;
  pAccum = sqlite3_aggregate_context(context, 0);
  if( pAccum ){
    if( pAccum->tooBig ){
      sqlite3_result_error_toobig(context);
    }else if( pAccum->mallocFailed ){
      sqlite3_result_error_nomem(context);
    }else{    
      sqlite3_result_text(context, sqlite3StrAccumFinish(pAccum), -1, 
                          sqlite3_free);
    }
  }
}

/*
** This routine does per-connection function registration.  Most
** of the built-in functions above are part of the global function set.
** This routine only deals with those that are not global.
这个例程是在每个连接函数注册。
最上面的内置函数是全局函数集的一部分。
这个例程只处理那些不是全局的函数。
*/
void sqlite3RegisterBuiltinFunctions(sqlite3 *db){
  int rc = sqlite3_overload_function(db, "MATCH", 2);
  assert( rc==SQLITE_NOMEM || rc==SQLITE_OK );
  if( rc==SQLITE_NOMEM ){
    db->mallocFailed = 1;
  }
}

/*
** Set the LIKEOPT flag on the 2-argument function with the given name.
设置LIKEOPT标志给定2-argument函数名字。
*/
static void setLikeOptFlag(sqlite3 *db, const char *zName, u8 flagVal){
  FuncDef *pDef;
  pDef = sqlite3FindFunction(db, zName, sqlite3Strlen30(zName),
                             2, SQLITE_UTF8, 0);
  if( ALWAYS(pDef) ){
    pDef->flags = flagVal;
  }
}

/*
** Register the built-in LIKE and GLOB functions.  The caseSensitive
** parameter determines whether or not the LIKE operator is case
** sensitive.  GLOB is always case sensitive.
注册内置的LIKE and GLOB函数。
caseSensitive参数确定LIKE 操作符是否区分大小写的。 
GLOB永远都是区分大小写的。
*/
void sqlite3RegisterLikeFunctions(sqlite3 *db, int caseSensitive){
  struct compareInfo *pInfo;
  if( caseSensitive ){
    pInfo = (struct compareInfo*)&likeInfoAlt;//区分大小写
  }else{
    pInfo = (struct compareInfo*)&likeInfoNorm;//不区分大小写
  }
  sqlite3CreateFunc(db, "like", 2, SQLITE_UTF8, pInfo, likeFunc, 0, 0, 0);
  sqlite3CreateFunc(db, "like", 3, SQLITE_UTF8, pInfo, likeFunc, 0, 0, 0);
  sqlite3CreateFunc(db, "glob", 2, SQLITE_UTF8, 
      (struct compareInfo*)&globInfo, likeFunc, 0, 0, 0);
  setLikeOptFlag(db, "glob", SQLITE_FUNC_LIKE | SQLITE_FUNC_CASE);
  setLikeOptFlag(db, "like", 
      caseSensitive ? (SQLITE_FUNC_LIKE | SQLITE_FUNC_CASE) : SQLITE_FUNC_LIKE);
}

/*
** pExpr points to an expression which implements a function.  If
** it is appropriate to apply the LIKE optimization to that function
** then set aWc[0] through aWc[2] to the wildcard characters and
** return TRUE.  If the function is not a LIKE-style function then
** return FALSE.
pExpr指向一个实现一个函数表达式。如果它适合应用LIKE 的最优化，
函数将AWC [ 0 ]通过AWC [ 2 ]的通配符并返回true。
如果函数是一个不 LIKE-style函数那么返回false。
*/
int sqlite3IsLikeFunction(sqlite3 *db, Expr *pExpr, int *pIsNocase, char *aWc){
  FuncDef *pDef;
  if( pExpr->op!=TK_FUNCTION 
   || !pExpr->x.pList 
   || pExpr->x.pList->nExpr!=2
  ){
    return 0;
  }
  assert( !ExprHasProperty(pExpr, EP_xIsSelect) );
  pDef = sqlite3FindFunction(db, pExpr->u.zToken, 
                             sqlite3Strlen30(pExpr->u.zToken),
                             2, SQLITE_UTF8, 0);
  if( NEVER(pDef==0) || (pDef->flags & SQLITE_FUNC_LIKE)==0 ){
    return 0;
  }

  /* The memcpy() statement assumes that the wildcard characters are
  ** the first three statements in the compareInfo structure.  The
  ** asserts() that follow verify that assumption
  memcpy()语句认为通配符compareInfo中的前三个语句结构。
  asserts() 将验证这个假设
  */
  memcpy(aWc, pDef->pUserData, 3);
  assert( (char*)&likeInfoAlt == (char*)&likeInfoAlt.matchAll );
  assert( &((char*)&likeInfoAlt)[1] == (char*)&likeInfoAlt.matchOne );
  assert( &((char*)&likeInfoAlt)[2] == (char*)&likeInfoAlt.matchSet );
  *pIsNocase = (pDef->flags & SQLITE_FUNC_CASE)==0;
  return 1;
}

/*
** All all of the FuncDef structures in the aBuiltinFunc[] array above
** to the global function hash table.  This occurs at start-time (as
** a consequence of calling sqlite3_initialize()).
**
** After this routine runs
所有的FuncDef结构都在aBuiltinFunc[]数组上面的全局函数散列表中。
这发生在起始时间(由于调用sqlite3_initialize())。
这个例程运行后

*/
void sqlite3RegisterGlobalFunctions(void){
  /*
  ** The following array holds FuncDef structures for all of the functions
  ** defined in this file.
  **
  ** The array cannot be constant since changes are made to the
  ** FuncDef.pHash elements at start-time.  The elements of this array
  ** are read-only after initialization is complete.
以下数组持有在这个文件中定义的所有函数的FuncDef结构。
由于改变了FuncDef，数组不能成为常量。pHash元素起始时间。
这个数组的元素初始化完成后是只读的。
  */
  static SQLITE_WSD FuncDef aBuiltinFunc[] = {
    FUNCTION(ltrim,              1, 1, 0, trimFunc         ),
    FUNCTION(ltrim,              2, 1, 0, trimFunc         ),
    FUNCTION(rtrim,              1, 2, 0, trimFunc         ),
    FUNCTION(rtrim,              2, 2, 0, trimFunc         ),
    FUNCTION(trim,               1, 3, 0, trimFunc         ),
    FUNCTION(trim,               2, 3, 0, trimFunc         ),
    FUNCTION(min,               -1, 0, 1, minmaxFunc       ),
    FUNCTION(min,                0, 0, 1, 0                ),
    AGGREGATE(min,               1, 0, 1, minmaxStep,      minMaxFinalize ),
    FUNCTION(max,               -1, 1, 1, minmaxFunc       ),
    FUNCTION(max,                0, 1, 1, 0                ),
    AGGREGATE(max,               1, 1, 1, minmaxStep,      minMaxFinalize ),
    FUNCTION2(typeof,            1, 0, 0, typeofFunc,  SQLITE_FUNC_TYPEOF),
    FUNCTION2(length,            1, 0, 0, lengthFunc,  SQLITE_FUNC_LENGTH),
    FUNCTION(substr,             2, 0, 0, substrFunc       ),
    FUNCTION(substr,             3, 0, 0, substrFunc       ),
    FUNCTION(abs,                1, 0, 0, absFunc          ),
#ifndef SQLITE_OMIT_FLOATING_POINT
    FUNCTION(round,              1, 0, 0, roundFunc        ),
    FUNCTION(round,              2, 0, 0, roundFunc        ),
#endif
    FUNCTION(upper,              1, 0, 0, upperFunc        ),
    FUNCTION(lower,              1, 0, 0, lowerFunc        ),
    FUNCTION(coalesce,           1, 0, 0, 0                ),
    FUNCTION(coalesce,           0, 0, 0, 0                ),
    FUNCTION2(coalesce,         -1, 0, 0, ifnullFunc,  SQLITE_FUNC_COALESCE),
    FUNCTION(hex,                1, 0, 0, hexFunc          ),
    FUNCTION2(ifnull,            2, 0, 0, ifnullFunc,  SQLITE_FUNC_COALESCE),
    FUNCTION(random,             0, 0, 0, randomFunc       ),
    FUNCTION(randomblob,         1, 0, 0, randomBlob       ),
    FUNCTION(nullif,             2, 0, 1, nullifFunc       ),
    FUNCTION(sqlite_version,     0, 0, 0, versionFunc      ),
    FUNCTION(sqlite_source_id,   0, 0, 0, sourceidFunc     ),
    FUNCTION(sqlite_log,         2, 0, 0, errlogFunc       ),
#ifndef SQLITE_OMIT_COMPILEOPTION_DIAGS
    FUNCTION(sqlite_compileoption_used,1, 0, 0, compileoptionusedFunc  ),
    FUNCTION(sqlite_compileoption_get, 1, 0, 0, compileoptiongetFunc  ),
#endif /* SQLITE_OMIT_COMPILEOPTION_DIAGS */
    FUNCTION(quote,              1, 0, 0, quoteFunc        ),
    FUNCTION(last_insert_rowid,  0, 0, 0, last_insert_rowid),
    FUNCTION(changes,            0, 0, 0, changes          ),
    FUNCTION(total_changes,      0, 0, 0, total_changes    ),
    FUNCTION(replace,            3, 0, 0, replaceFunc      ),
    FUNCTION(zeroblob,           1, 0, 0, zeroblobFunc     ),
  #ifdef SQLITE_SOUNDEX
    FUNCTION(soundex,            1, 0, 0, soundexFunc      ),
  #endif
  #ifndef SQLITE_OMIT_LOAD_EXTENSION
    FUNCTION(load_extension,     1, 0, 0, loadExt          ),
    FUNCTION(load_extension,     2, 0, 0, loadExt          ),
  #endif
    AGGREGATE(sum,               1, 0, 0, sumStep,         sumFinalize    ),
    AGGREGATE(total,             1, 0, 0, sumStep,         totalFinalize    ),
    AGGREGATE(avg,               1, 0, 0, sumStep,         avgFinalize    ),
 /* AGGREGATE(count,             0, 0, 0, countStep,       countFinalize  ), */
    {0,SQLITE_UTF8,SQLITE_FUNC_COUNT,0,0,0,countStep,countFinalize,"count",0,0},
    AGGREGATE(count,             1, 0, 0, countStep,       countFinalize  ),
    AGGREGATE(group_concat,      1, 0, 0, groupConcatStep, groupConcatFinalize),
    AGGREGATE(group_concat,      2, 0, 0, groupConcatStep, groupConcatFinalize),
  
    LIKEFUNC(glob, 2, &globInfo, SQLITE_FUNC_LIKE|SQLITE_FUNC_CASE),
  #ifdef SQLITE_CASE_SENSITIVE_LIKE
    LIKEFUNC(like, 2, &likeInfoAlt, SQLITE_FUNC_LIKE|SQLITE_FUNC_CASE),
    LIKEFUNC(like, 3, &likeInfoAlt, SQLITE_FUNC_LIKE|SQLITE_FUNC_CASE),
  #else
    LIKEFUNC(like, 2, &likeInfoNorm, SQLITE_FUNC_LIKE),
    LIKEFUNC(like, 3, &likeInfoNorm, SQLITE_FUNC_LIKE),
  #endif
  };

  int i;
  FuncDefHash *pHash = &GLOBAL(FuncDefHash, sqlite3GlobalFunctions);
  FuncDef *aFunc = (FuncDef*)&GLOBAL(FuncDef, aBuiltinFunc);

  for(i=0; i<ArraySize(aBuiltinFunc); i++){
    sqlite3FuncDefInsert(pHash, &aFunc[i]);
  }
  sqlite3RegisterDateTimeFunctions();
#ifndef SQLITE_OMIT_ALTERTABLE
  sqlite3AlterFunctions();
#endif
}

