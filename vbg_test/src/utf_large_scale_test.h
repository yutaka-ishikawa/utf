#include <mpi.h>
#if defined(__FCC_version__) // for FJ MPI
    #include <mpi-ext.h>
#endif


/*
 * Constants
 */
#define FUNC_SUCCESS    0
#define FUNC_INCORRECT  1
#define FUNC_ERROR      2

#define VBG_PASS        0
#define VBG_NOT_PASS    1

enum FUNC_ID
{
    FUNC_MPI_BARRIER,
    FUNC_MPI_BCAST,
    FUNC_MPI_REDUCE,
    FUNC_MPI_ALLREDUCE
};

enum COMM_ID
{
    COMM_WORLD,
    COMM_SPLIT,
    COMM_DUP,
    COMM_CREATE,
    COMM_SPLIT_FREE,
    COMM_DUP_FREE,
    COMM_CREATE_FREE,
    COMM_SPLIT_NO_FREE,
    COMM_DUP_NO_FREE,
    COMM_CREATE_NO_FREE
};

enum OP_ID
{
    OP_SUM,
    OP_BAND,
    OP_BOR,
    OP_BXOR,
    OP_LAND,
    OP_LOR,
    OP_LXOR,
    OP_MAX,
    OP_MIN,
    OP_MAXLOC,
    OP_MINLOC
};

enum DATATYPE_ID
{
    DT_CHAR,
    DT_SHORT,
    DT_INT,
    DT_LONG,
    DT_LONG_LONG_INT,
    DT_LONG_LONG,
    DT_UNSIGNED_CHAR,
    DT_UNSIGNED_SHORT,
    DT_UNSIGNED,
    DT_UNSIGNED_LONG,
    DT_UNSIGNED_LONG_LONG,
    DT_INT8_T,
    DT_INT16_T,
    DT_INT32_T,
    DT_INT64_T,
    DT_UINT8_T,
    DT_UINT16_T,
    DT_UINT32_T,
    DT_UINT64_T,
    DT_WCHAR,
    DT_BYTE,
    DT_C_BOOL,
    DT_FLOAT,
    DT_DOUBLE,
    DT_LONG_DOUBLE,
#if defined(__clang__)
    DT_C_FLOAT16,
#endif
    DT_C_FLOAT_COMPLEX,
    DT_C_COMPLEX,
    DT_C_DOUBLE_COMPLEX,
    DT_C_LONG_DOUBLE_COMPLEX,
    DT_SHORT_INT,
    DT_2INT,
    DT_LONG_INT
};

const char *FUNC_STR[] =
{
    "Barrier",
    "Bcast",
    "Reduce",
    "Allreduce"
};

const char *COMM_STR[] =
{
    "WORLD",
    "SPLIT",
    "DUP",
    "CREATE",
    "SPLIT_FREE",
    "DUP_FREE",
    "CREATE_FREE",
    "SPLIT_NO_FREE",
    "DUP_NO_FREE",
    "CREATE_NO_FREE"
};

const char *OP_STR[] =
{
    "MPI_SUM",
    "MPI_BAND",
    "MPI_BOR",
    "MPI_BXOR",
    "MPI_LAND",
    "MPI_LOR",
    "MPI_LXOR",
    "MPI_MAX",
    "MPI_MIN",
    "MPI_MAXLOC",
    "MPI_MINLOC"
};

const char *DATATYPE_STR[] =
{
    "MPI_CHAR",
    "MPI_SHORT",
    "MPI_INT",
    "MPI_LONG",
    "MPI_LONG_LONG_INT",
    "MPI_LONG_LONG",
    "MPI_UNSIGNED_CHAR",
    "MPI_UNSIGNED_SHORT",
    "MPI_UNSIGNED",
    "MPI_UNSIGNED_LONG",
    "MPI_UNSIGNED_LONG_LONG",
    "MPI_INT8_T",
    "MPI_INT16_T",
    "MPI_INT32_T",
    "MPI_INT64_T",
    "MPI_UINT8_T",
    "MPI_UINT16_T",
    "MPI_UINT32_T",
    "MPI_UINT64_T",
    "MPI_WCHAR",
    "MPI_BYTE",
    "MPI_C_BOOL",
    "MPI_FLOAT",
    "MPI_DOUBLE",
    "MPI_LONG_DOUBLE",
#if defined(__clang__)
    "MPI_C_FLOAT16",
#endif
    "MPI_C_FLOAT_COMPLEX",
    "MPI_C_COMPLEX",
    "MPI_C_DOUBLE_COMPLEX",
    "MPI_C_LONG_DOUBLE_COMPLEX",
    "MPI_SHORT_INT",
    "MPI_2INT",
    "MPI_LONG_INT"
};


/*
 * Tables
 */
const int test_func_tbl[] =
{
    FUNC_MPI_BARRIER,
    FUNC_MPI_BCAST,
    FUNC_MPI_REDUCE,
    FUNC_MPI_ALLREDUCE 
};

const int test_comm_tbl[] =
{
    COMM_WORLD,
    COMM_SPLIT,
    COMM_DUP,
    COMM_CREATE,
    COMM_SPLIT_NO_FREE,
    COMM_DUP_NO_FREE,
    COMM_CREATE_NO_FREE,
    COMM_SPLIT_FREE,
    COMM_DUP_FREE,
    COMM_CREATE_FREE
};

const MPI_Op test_op_tbl[] =
{
    MPI_SUM,
    MPI_BAND,
    MPI_BOR,
    MPI_BXOR,
    MPI_LAND,
    MPI_LOR,
    MPI_LXOR,
    MPI_MAX,
    MPI_MIN,
    MPI_MAXLOC,
    MPI_MINLOC
};

const MPI_Datatype test_datatype_tbl[] =
{
    MPI_CHAR,
    MPI_SHORT,
    MPI_INT,
    MPI_LONG,
    MPI_LONG_LONG_INT,
    MPI_LONG_LONG,
    MPI_UNSIGNED_CHAR,
    MPI_UNSIGNED_SHORT,
    MPI_UNSIGNED,
    MPI_UNSIGNED_LONG,
    MPI_UNSIGNED_LONG_LONG,
    MPI_INT8_T,
    MPI_INT16_T,
    MPI_INT32_T,
    MPI_INT64_T,
    MPI_UINT8_T,
    MPI_UINT16_T,
    MPI_UINT32_T,
    MPI_UINT64_T,
    MPI_WCHAR,
    MPI_BYTE,
    MPI_C_BOOL,
    MPI_FLOAT,
    MPI_DOUBLE,
    MPI_LONG_DOUBLE,
#if defined(__clang__)
    MPIX_C_FLOAT16,
#endif
    MPI_C_FLOAT_COMPLEX,
    MPI_C_COMPLEX,
    MPI_C_DOUBLE_COMPLEX,
    MPI_C_LONG_DOUBLE_COMPLEX,
    MPI_SHORT_INT,
    MPI_2INT,
    MPI_LONG_INT
};

static const int MAX_FUNC     = sizeof(test_func_tbl)/sizeof(int);
static const int MAX_COMM     = sizeof(test_comm_tbl)/sizeof(int);
static const int MAX_OP       = sizeof(test_op_tbl)/sizeof(MPI_Op);
static const int MAX_DATATYPE = sizeof(test_datatype_tbl)/sizeof(MPI_Datatype);


/*
 * Type definitions
 */
// for MPI_SHORT_INT/MPI_2INT/MPI_LONG_INT
//  val: value part, ind: index part
typedef struct _short_int { short val; int ind; } short_int;
typedef struct _int_int   { int   val; int ind; } int_int;
typedef struct _long_int  { long  val; int ind; } long_int;

#if USE_WTIME
    typedef double   utf_timer_t;
#else
    typedef uint64_t utf_timer_t;
#endif


/*
 * Macros
 */
#define ANS_LAND(a, b)    ((a) && (b))
#define ANS_LOR(a, b)     ((a) || (b))
#define ANS_LXOR(a, b)    (((a) && !(b)) || (!(a) && (b)))

