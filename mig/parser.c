
/*  A Bison parser, made from ../gnumach/mig/parser.y
 by  Bison version A2.5 (Andrew Consortium)
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	sySkip	258
#define	syRoutine	259
#define	sySimpleRoutine	260
#define	sySimpleProcedure	261
#define	syProcedure	262
#define	syFunction	263
#define	sySubsystem	264
#define	syKernelUser	265
#define	syKernelServer	266
#define	syMsgOption	267
#define	syMsgSeqno	268
#define	syWaitTime	269
#define	syNoWaitTime	270
#define	syErrorProc	271
#define	syServerPrefix	272
#define	syUserPrefix	273
#define	syServerDemux	274
#define	syRCSId	275
#define	syImport	276
#define	syUImport	277
#define	sySImport	278
#define	syIn	279
#define	syOut	280
#define	syInOut	281
#define	syRequestPort	282
#define	syReplyPort	283
#define	sySReplyPort	284
#define	syUReplyPort	285
#define	syType	286
#define	syArray	287
#define	syStruct	288
#define	syOf	289
#define	syInTran	290
#define	syOutTran	291
#define	syDestructor	292
#define	syCType	293
#define	syCUserType	294
#define	syCServerType	295
#define	syCString	296
#define	syColon	297
#define	sySemi	298
#define	syComma	299
#define	syPlus	300
#define	syMinus	301
#define	syStar	302
#define	syDiv	303
#define	syLParen	304
#define	syRParen	305
#define	syEqual	306
#define	syCaret	307
#define	syTilde	308
#define	syLAngle	309
#define	syRAngle	310
#define	syLBrack	311
#define	syRBrack	312
#define	syBar	313
#define	syError	314
#define	syNumber	315
#define	sySymbolicType	316
#define	syIdentifier	317
#define	syString	318
#define	syQString	319
#define	syFileName	320
#define	syIPCFlag	321

#line 117 "../gnumach/mig/parser.y"


#include <stdio.h>

#include "error.h"
#include "lexxer.h"
#include "global.h"
#include "mig_string.h"
#include "type.h"
#include "routine.h"
#include "statement.h"

static const char *import_name(statement_kind_t sk);

void
yyerror(const char *s)
{
    error(s);
}

#line 138 "../gnumach/mig/parser.y"
typedef union
{
    u_int number;
    identifier_t identifier;
    const_string_t string;
    statement_kind_t statement_kind;
    ipc_type_t *type;
    struct
    {
	u_int innumber;		/* msgt_name value, when sending */
	const_string_t instr;
	u_int outnumber;	/* msgt_name value, when receiving */
	const_string_t outstr;
	u_int size;		/* 0 means there is no default size */
    } symtype;
    routine_t *routine;
    arg_kind_t direction;
    argument_t *argument;
    ipc_flags_t flag;
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		213
#define	YYFLAG		-32768
#define	YYNTBASE	67

#define YYTRANSLATE(x) ((unsigned)(x) <= 321 ? yytranslate[x] : 112)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
    66
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     1,     4,     7,    10,    13,    16,    19,    22,    25,
    28,    31,    34,    38,    41,    44,    46,    49,    54,    56,
    57,    60,    62,    64,    66,    68,    72,    76,    78,    81,
    84,    87,    90,    94,    96,    98,   100,   104,   107,   111,
   113,   122,   131,   139,   144,   149,   154,   156,   158,   161,
   164,   167,   170,   172,   174,   181,   182,   186,   192,   194,
   196,   198,   202,   204,   209,   215,   223,   229,   235,   240,
   247,   251,   255,   259,   263,   265,   269,   271,   273,   275,
   277,   279,   283,   287,   291,   295,   300,   303,   307,   309,
   313,   318,   319,   321,   323,   325,   327,   329,   331,   333,
   335,   337,   339,   342,   345,   346,   347
};

static const short yyrhs[] = {    -1,
    67,    68,     0,    69,    43,     0,    76,    43,     0,    75,
    43,     0,    77,    43,     0,    78,    43,     0,    79,    43,
     0,    80,    43,     0,    84,    43,     0,    98,    43,     0,
     3,    43,     0,     3,    60,    43,     0,    81,    43,     0,
    83,    43,     0,    43,     0,     1,    43,     0,    70,    71,
    73,    74,     0,     9,     0,     0,    71,    72,     0,    10,
     0,    11,     0,    62,     0,    60,     0,   109,    12,    63,
     0,   109,    14,    63,     0,    15,     0,    16,    62,     0,
    17,    62,     0,    18,    62,     0,    19,    62,     0,   110,
    82,    65,     0,    21,     0,    22,     0,    23,     0,   111,
    20,    64,     0,    31,    85,     0,    62,    51,    86,     0,
    87,     0,    86,    35,    42,    62,    62,    49,    62,    50,
     0,    86,    36,    42,    62,    62,    49,    62,    50,     0,
    86,    37,    42,    62,    49,    62,    50,     0,    86,    38,
    42,    62,     0,    86,    39,    42,    62,     0,    86,    40,
    42,    62,     0,    88,     0,    92,     0,    93,    87,     0,
    94,    87,     0,    52,    87,     0,    95,    87,     0,    96,
     0,    91,     0,    49,    91,    44,    97,    89,    50,     0,
     0,    89,    44,    66,     0,    89,    44,    66,    56,    57,
     0,    60,     0,    61,     0,    90,     0,    90,    58,    90,
     0,    62,     0,    32,    56,    57,    34,     0,    32,    56,
    47,    57,    34,     0,    32,    56,    47,    42,    97,    57,
    34,     0,    32,    56,    97,    57,    34,     0,    33,    56,
    97,    57,    34,     0,    41,    56,    97,    57,     0,    41,
    56,    47,    42,    97,    57,     0,    97,    45,    97,     0,
    97,    46,    97,     0,    97,    47,    97,     0,    97,    48,
    97,     0,    60,     0,    49,    97,    50,     0,    99,     0,
   100,     0,   101,     0,   102,     0,   103,     0,     4,    62,
   104,     0,     5,    62,   104,     0,     7,    62,   104,     0,
     6,    62,   104,     0,     8,    62,   104,   108,     0,    49,
    50,     0,    49,   105,    50,     0,   106,     0,   106,    43,
   105,     0,   107,    62,   108,    89,     0,     0,    24,     0,
    25,     0,    26,     0,    27,     0,    28,     0,    29,     0,
    30,     0,    14,     0,    12,     0,    13,     0,    42,    62,
     0,    42,    85,     0,     0,     0,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   161,   162,   165,   166,   167,   168,   169,   170,   171,   172,
   173,   183,   185,   187,   188,   189,   190,   194,   207,   219,
   220,   223,   229,   237,   240,   243,   260,   266,   274,   282,
   290,   298,   306,   317,   318,   319,   322,   332,   342,   346,
   348,   368,   388,   403,   417,   426,   438,   440,   442,   444,
   446,   448,   450,   454,   460,   469,   471,   478,   487,   493,
   497,   499,   523,   527,   529,   531,   536,   540,   544,   546,
   551,   553,   555,   557,   559,   561,   566,   567,   568,   569,
   570,   573,   577,   581,   585,   589,   593,   595,   600,   602,
   609,   619,   620,   621,   622,   623,   624,   625,   626,   627,
   628,   629,   632,   638,   642,   646,   650
};
#endif


#if YYDEBUG != 0

static const char * const yytname[] = {   "$","error","$undefined.","sySkip",
"syRoutine","sySimpleRoutine","sySimpleProcedure","syProcedure","syFunction",
"sySubsystem","syKernelUser","syKernelServer","syMsgOption","syMsgSeqno","syWaitTime",
"syNoWaitTime","syErrorProc","syServerPrefix","syUserPrefix","syServerDemux",
"syRCSId","syImport","syUImport","sySImport","syIn","syOut","syInOut","syRequestPort",
"syReplyPort","sySReplyPort","syUReplyPort","syType","syArray","syStruct","syOf",
"syInTran","syOutTran","syDestructor","syCType","syCUserType","syCServerType",
"syCString","syColon","sySemi","syComma","syPlus","syMinus","syStar","syDiv",
"syLParen","syRParen","syEqual","syCaret","syTilde","syLAngle","syRAngle","syLBrack",
"syRBrack","syBar","syError","syNumber","sySymbolicType","syIdentifier","syString",
"syQString","syFileName","syIPCFlag","Statements","Statement","Subsystem","SubsystemStart",
"SubsystemMods","SubsystemMod","SubsystemName","SubsystemBase","MsgOption","WaitTime",
"Error","ServerPrefix","UserPrefix","ServerDemux","Import","ImportIndicant",
"RCSDecl","TypeDecl","NamedTypeSpec","TransTypeSpec","TypeSpec","BasicTypeSpec",
"IPCFlags","PrimIPCType","IPCType","PrevTypeSpec","VarArrayHead","ArrayHead",
"StructHead","CStringSpec","IntExp","RoutineDecl","Routine","SimpleRoutine",
"Procedure","SimpleProcedure","Function","Arguments","ArgumentList","Argument",
"Direction","ArgumentType","LookString","LookFileName","LookQString", NULL
};
#endif

static const short yyr1[] = {     0,
    67,    67,    68,    68,    68,    68,    68,    68,    68,    68,
    68,    68,    68,    68,    68,    68,    68,    69,    70,    71,
    71,    72,    72,    73,    74,    75,    76,    76,    77,    78,
    79,    80,    81,    82,    82,    82,    83,    84,    85,    86,
    86,    86,    86,    86,    86,    86,    87,    87,    87,    87,
    87,    87,    87,    88,    88,    89,    89,    89,    90,    90,
    91,    91,    92,    93,    93,    93,    94,    95,    96,    96,
    97,    97,    97,    97,    97,    97,    98,    98,    98,    98,
    98,    99,   100,   101,   102,   103,   104,   104,   105,   105,
   106,   107,   107,   107,   107,   107,   107,   107,   107,   107,
   107,   107,   108,   108,   109,   110,   111
};

static const short yyr2[] = {     0,
     0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     3,     2,     2,     1,     2,     4,     1,     0,
     2,     1,     1,     1,     1,     3,     3,     1,     2,     2,
     2,     2,     3,     1,     1,     1,     3,     2,     3,     1,
     8,     8,     7,     4,     4,     4,     1,     1,     2,     2,
     2,     2,     1,     1,     6,     0,     3,     5,     1,     1,
     1,     3,     1,     4,     5,     7,     5,     5,     4,     6,
     3,     3,     3,     3,     1,     3,     1,     1,     1,     1,
     1,     3,     3,     3,     3,     4,     2,     3,     1,     3,
     4,     0,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     2,     2,     0,     0,     0
};

static const short yydefact[] = {     1,
     0,     0,     0,     0,     0,     0,     0,     0,    19,    28,
     0,     0,     0,     0,     0,    16,     2,     0,    20,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,    77,
    78,    79,    80,    81,     0,     0,     0,    17,    12,     0,
     0,     0,     0,     0,     0,    29,    30,    31,    32,     0,
    38,     3,     0,     5,     4,     6,     7,     8,     9,    14,
    15,    10,    11,     0,     0,    34,    35,    36,     0,     0,
    13,    92,    82,    83,    85,    84,     0,     0,    22,    23,
    24,    21,     0,    26,    27,    33,    37,   101,   102,   100,
    93,    94,    95,    96,    97,    98,    99,    87,     0,    89,
     0,     0,    86,     0,     0,     0,     0,     0,    59,    60,
    63,    39,    40,    47,    61,    54,    48,     0,     0,     0,
    53,    25,    18,    88,    92,     0,   103,   104,     0,     0,
     0,     0,    51,     0,     0,     0,     0,     0,     0,     0,
    49,    50,    52,    90,    56,     0,     0,     0,    75,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    62,    91,     0,     0,     0,    64,     0,     0,     0,     0,
     0,     0,     0,    69,    56,     0,     0,     0,    44,    45,
    46,     0,     0,    65,    76,    71,    72,    73,    74,    67,
    68,     0,     0,     0,     0,     0,    57,     0,    70,    55,
     0,     0,     0,     0,    66,     0,     0,    43,    58,    41,
    42,     0,     0
};

static const short yydefgoto[] = {     1,
    17,    18,    19,    53,    82,    83,   123,    20,    21,    22,
    23,    24,    25,    26,    69,    27,    28,    51,   112,   113,
   114,   162,   115,   116,   117,   118,   119,   120,   121,   150,
    29,    30,    31,    32,    33,    34,    73,    99,   100,   101,
   103,    35,    36,    37
};

static const short yypact[] = {-32768,
     2,    -5,   -31,   -16,    27,    28,    30,    33,-32768,-32768,
    35,    49,    71,    72,    89,-32768,-32768,     6,-32768,    60,
    67,   107,   109,   110,   111,   112,   113,   114,   115,-32768,
-32768,-32768,-32768,-32768,    56,     9,    51,-32768,-32768,   116,
   117,   117,   117,   117,   117,-32768,-32768,-32768,-32768,   118,
-32768,-32768,   -10,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,    98,    99,-32768,-32768,-32768,    95,   100,
-32768,    48,-32768,-32768,-32768,-32768,   121,    47,-32768,-32768,
-32768,-32768,   105,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   120,   124,
   106,   119,-32768,   122,   123,   126,    -7,    47,-32768,-32768,
-32768,   101,-32768,-32768,   125,-32768,-32768,    47,    47,    47,
-32768,-32768,-32768,-32768,    88,   121,   118,-32768,   -13,   -45,
   -12,   127,-32768,   130,   131,   132,   133,   134,   135,    -7,
-32768,-32768,-32768,-32768,-32768,   -29,   -45,   146,-32768,    -6,
    10,   142,    36,   -45,   128,   129,   136,   137,   138,   139,
-32768,   141,   -45,   152,    82,-32768,   -45,   -45,   -45,   -45,
   153,   154,   -45,-32768,    97,   140,   143,   144,-32768,-32768,
-32768,   145,    74,-32768,-32768,    39,    39,-32768,-32768,-32768,
-32768,    78,    41,   147,   148,   150,   151,   155,-32768,-32768,
   156,   157,   158,   149,-32768,   159,   160,-32768,-32768,-32768,
-32768,   192,-32768
};

static const short yypgoto[] = {-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,    92,-32768,   -14,
-32768,    20,    63,   108,-32768,-32768,-32768,-32768,-32768,  -104,
-32768,-32768,-32768,-32768,-32768,-32768,   104,    79,-32768,-32768,
    87,-32768,-32768,-32768
};


#define	YYLAST		219


static const short yytable[] = {    79,
    80,   212,     2,   147,     3,     4,     5,     6,     7,     8,
     9,    39,   163,  -105,   149,  -105,    10,    11,    12,    13,
    14,  -107,  -106,  -106,  -106,   151,   153,   164,    40,    66,
    67,    68,    15,   146,   152,   147,   147,    38,   167,   168,
   169,   170,   165,   148,    16,    41,   149,   149,    52,   175,
   171,    81,   109,   110,   167,   168,   169,   170,   183,    88,
    89,    90,   186,   187,   188,   189,   172,    64,   192,    65,
    70,    91,    92,    93,    94,    95,    96,    97,   104,   105,
   167,   168,   169,   170,   182,   169,   170,   106,    42,    43,
   200,    44,   174,   133,    45,   107,    46,    98,   108,    88,
    89,    90,    54,   141,   142,   143,   109,   110,   111,    55,
    47,    91,    92,    93,    94,    95,    96,    97,   167,   168,
   169,   170,   167,   168,   169,   170,   167,   168,   169,   170,
   198,   185,    48,    49,   199,   134,   135,   136,   137,   138,
   139,   167,   168,   169,   170,    74,    75,    76,    77,    56,
    50,    57,    58,    59,    60,    61,    62,    63,    71,    86,
    84,    85,   102,    87,   122,    72,   125,   126,    78,   124,
   154,   155,   156,   157,   158,   159,   160,   129,   130,   166,
   127,   131,   140,   173,   182,   184,   190,   191,   205,   176,
   177,   213,   196,   128,   193,   201,   202,   178,   179,   180,
   181,   194,   161,   144,   195,   209,   204,   208,   210,   211,
   197,   203,   145,     0,   132,     0,     0,   206,   207
};

static const short yycheck[] = {    10,
    11,     0,     1,    49,     3,     4,     5,     6,     7,     8,
     9,    43,    42,    12,    60,    14,    15,    16,    17,    18,
    19,    20,    21,    22,    23,   130,   131,    57,    60,    21,
    22,    23,    31,    47,    47,    49,    49,    43,    45,    46,
    47,    48,   147,    57,    43,    62,    60,    60,    43,   154,
    57,    62,    60,    61,    45,    46,    47,    48,   163,    12,
    13,    14,   167,   168,   169,   170,    57,    12,   173,    14,
    20,    24,    25,    26,    27,    28,    29,    30,    32,    33,
    45,    46,    47,    48,    44,    47,    48,    41,    62,    62,
    50,    62,    57,   108,    62,    49,    62,    50,    52,    12,
    13,    14,    43,   118,   119,   120,    60,    61,    62,    43,
    62,    24,    25,    26,    27,    28,    29,    30,    45,    46,
    47,    48,    45,    46,    47,    48,    45,    46,    47,    48,
    57,    50,    62,    62,    57,    35,    36,    37,    38,    39,
    40,    45,    46,    47,    48,    42,    43,    44,    45,    43,
    62,    43,    43,    43,    43,    43,    43,    43,    43,    65,
    63,    63,    42,    64,    60,    49,    43,    62,    51,    50,
    44,    42,    42,    42,    42,    42,    42,    56,    56,    34,
    62,    56,    58,    42,    44,    34,    34,    34,    34,    62,
    62,     0,    49,   102,   175,    49,    49,    62,    62,    62,
    62,    62,   140,   125,    62,    57,    56,    50,    50,    50,
    66,    62,   126,    -1,   107,    -1,    -1,    62,    62
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison.simple"

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

#ifndef alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi)
#include <alloca.h>
#else /* not sparc */
#if defined (MSDOS) && !defined (__TURBOC__)
#include <malloc.h>
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
#include <malloc.h>
 #pragma alloca
#else /* not MSDOS, __TURBOC__, or _AIX */
#ifdef __hpux
#ifdef __cplusplus
extern "C" {
void *alloca (unsigned int);
};
#else /* not __cplusplus */
void *alloca ();
#endif /* not __cplusplus */
#endif /* __hpux */
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc.  */
#endif /* not GNU C.  */
#endif /* alloca not defined.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	return(0)
#define YYABORT 	return(1)
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
int yyparse (void);
#endif

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(FROM,TO,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (from, to, count)
     char *from;
     char *to;
     int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *from, char *to, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 192 "/usr/share/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#else
#define YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#endif

int
yyparse(YYPARSE_PARAM)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
      yyss = (short *) alloca (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss1, (char *)yyss, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs1, (char *)yyvs, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls1, (char *)yyls, size * sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 11:
#line 174 "../gnumach/mig/parser.y"
{
    register statement_t *st = stAlloc();

    st->stKind = skRoutine;
    st->stRoutine = yyvsp[-1].routine;
    rtCheckRoutine(yyvsp[-1].routine);
    if (BeVerbose)
	rtPrintRoutine(yyvsp[-1].routine);
;
    break;}
case 12:
#line 184 "../gnumach/mig/parser.y"
{ rtSkip(1); ;
    break;}
case 13:
#line 186 "../gnumach/mig/parser.y"
{ rtSkip(yyvsp[-1].number); ;
    break;}
case 17:
#line 191 "../gnumach/mig/parser.y"
{ yyerrok; ;
    break;}
case 18:
#line 196 "../gnumach/mig/parser.y"
{
    if (BeVerbose)
    {
	printf("Subsystem %s: base = %u%s%s\n\n",
	       SubsystemName, SubsystemBase,
	       IsKernelUser ? ", KernelUser" : "",
	       IsKernelServer ? ", KernelServer" : "");
    }
;
    break;}
case 19:
#line 208 "../gnumach/mig/parser.y"
{
    if (SubsystemName != strNULL)
    {
	warn("previous Subsystem decl (of %s) will be ignored", SubsystemName);
	IsKernelUser = FALSE;
	IsKernelServer = FALSE;
	strfree((string_t) SubsystemName);
    }
;
    break;}
case 22:
#line 224 "../gnumach/mig/parser.y"
{
    if (IsKernelUser)
	warn("duplicate KernelUser keyword");
    IsKernelUser = TRUE;
;
    break;}
case 23:
#line 230 "../gnumach/mig/parser.y"
{
    if (IsKernelServer)
	warn("duplicate KernelServer keyword");
    IsKernelServer = TRUE;
;
    break;}
case 24:
#line 237 "../gnumach/mig/parser.y"
{ SubsystemName = yyvsp[0].identifier; ;
    break;}
case 25:
#line 240 "../gnumach/mig/parser.y"
{ SubsystemBase = yyvsp[0].number; ;
    break;}
case 26:
#line 244 "../gnumach/mig/parser.y"
{
    if (streql(yyvsp[0].string, "MACH_MSG_OPTION_NONE"))
    {
	MsgOption = strNULL;
	if (BeVerbose)
	    printf("MsgOption: canceled\n\n");
    }
    else
    {
	MsgOption = yyvsp[0].string;
	if (BeVerbose)
	    printf("MsgOption %s\n\n",yyvsp[0].string);
    }
;
    break;}
case 27:
#line 261 "../gnumach/mig/parser.y"
{
    WaitTime = yyvsp[0].string;
    if (BeVerbose)
	printf("WaitTime %s\n\n", WaitTime);
;
    break;}
case 28:
#line 267 "../gnumach/mig/parser.y"
{
    WaitTime = strNULL;
    if (BeVerbose)
	printf("NoWaitTime\n\n");
;
    break;}
case 29:
#line 275 "../gnumach/mig/parser.y"
{
    ErrorProc = yyvsp[0].identifier;
    if (BeVerbose)
	printf("ErrorProc %s\n\n", ErrorProc);
;
    break;}
case 30:
#line 283 "../gnumach/mig/parser.y"
{
    ServerPrefix = yyvsp[0].identifier;
    if (BeVerbose)
	printf("ServerPrefix %s\n\n", ServerPrefix);
;
    break;}
case 31:
#line 291 "../gnumach/mig/parser.y"
{
    UserPrefix = yyvsp[0].identifier;
    if (BeVerbose)
	printf("UserPrefix %s\n\n", UserPrefix);
;
    break;}
case 32:
#line 299 "../gnumach/mig/parser.y"
{
    ServerDemux = yyvsp[0].identifier;
    if (BeVerbose)
	printf("ServerDemux %s\n\n", ServerDemux);
;
    break;}
case 33:
#line 307 "../gnumach/mig/parser.y"
{
    register statement_t *st = stAlloc();
    st->stKind = yyvsp[-1].statement_kind;
    st->stFileName = yyvsp[0].string;

    if (BeVerbose)
	printf("%s %s\n\n", import_name(yyvsp[-1].statement_kind), yyvsp[0].string);
;
    break;}
case 34:
#line 317 "../gnumach/mig/parser.y"
{ yyval.statement_kind = skImport; ;
    break;}
case 35:
#line 318 "../gnumach/mig/parser.y"
{ yyval.statement_kind = skUImport; ;
    break;}
case 36:
#line 319 "../gnumach/mig/parser.y"
{ yyval.statement_kind = skSImport; ;
    break;}
case 37:
#line 323 "../gnumach/mig/parser.y"
{
    if (RCSId != strNULL)
	warn("previous RCS decl will be ignored");
    if (BeVerbose)
	printf("RCSId %s\n\n", yyvsp[0].string);
    RCSId = yyvsp[0].string;
;
    break;}
case 38:
#line 333 "../gnumach/mig/parser.y"
{
    register identifier_t name = yyvsp[0].type->itName;

    if (itLookUp(name) != itNULL)
	warn("overriding previous definition of %s", name);
    itInsert(name, yyvsp[0].type);
;
    break;}
case 39:
#line 343 "../gnumach/mig/parser.y"
{ itTypeDecl(yyvsp[-2].identifier, yyval.type = yyvsp[0].type); ;
    break;}
case 40:
#line 347 "../gnumach/mig/parser.y"
{ yyval.type = itResetType(yyvsp[0].type); ;
    break;}
case 41:
#line 350 "../gnumach/mig/parser.y"
{
    yyval.type = yyvsp[-7].type;

    if ((yyval.type->itTransType != strNULL) && !streql(yyval.type->itTransType, yyvsp[-4].identifier))
	warn("conflicting translation types (%s, %s)",
	     yyval.type->itTransType, yyvsp[-4].identifier);
    yyval.type->itTransType = yyvsp[-4].identifier;

    if ((yyval.type->itInTrans != strNULL) && !streql(yyval.type->itInTrans, yyvsp[-3].identifier))
	warn("conflicting in-translation functions (%s, %s)",
	     yyval.type->itInTrans, yyvsp[-3].identifier);
    yyval.type->itInTrans = yyvsp[-3].identifier;

    if ((yyval.type->itServerType != strNULL) && !streql(yyval.type->itServerType, yyvsp[-1].identifier))
	warn("conflicting server types (%s, %s)",
	     yyval.type->itServerType, yyvsp[-1].identifier);
    yyval.type->itServerType = yyvsp[-1].identifier;
;
    break;}
case 42:
#line 370 "../gnumach/mig/parser.y"
{
    yyval.type = yyvsp[-7].type;

    if ((yyval.type->itServerType != strNULL) && !streql(yyval.type->itServerType, yyvsp[-4].identifier))
	warn("conflicting server types (%s, %s)",
	     yyval.type->itServerType, yyvsp[-4].identifier);
    yyval.type->itServerType = yyvsp[-4].identifier;

    if ((yyval.type->itOutTrans != strNULL) && !streql(yyval.type->itOutTrans, yyvsp[-3].identifier))
	warn("conflicting out-translation functions (%s, %s)",
	     yyval.type->itOutTrans, yyvsp[-3].identifier);
    yyval.type->itOutTrans = yyvsp[-3].identifier;

    if ((yyval.type->itTransType != strNULL) && !streql(yyval.type->itTransType, yyvsp[-1].identifier))
	warn("conflicting translation types (%s, %s)",
	     yyval.type->itTransType, yyvsp[-1].identifier);
    yyval.type->itTransType = yyvsp[-1].identifier;
;
    break;}
case 43:
#line 390 "../gnumach/mig/parser.y"
{
    yyval.type = yyvsp[-6].type;

    if ((yyval.type->itDestructor != strNULL) && !streql(yyval.type->itDestructor, yyvsp[-3].identifier))
	warn("conflicting destructor functions (%s, %s)",
	     yyval.type->itDestructor, yyvsp[-3].identifier);
    yyval.type->itDestructor = yyvsp[-3].identifier;

    if ((yyval.type->itTransType != strNULL) && !streql(yyval.type->itTransType, yyvsp[-1].identifier))
	warn("conflicting translation types (%s, %s)",
	     yyval.type->itTransType, yyvsp[-1].identifier);
    yyval.type->itTransType = yyvsp[-1].identifier;
;
    break;}
case 44:
#line 404 "../gnumach/mig/parser.y"
{
    yyval.type = yyvsp[-3].type;

    if ((yyval.type->itUserType != strNULL) && !streql(yyval.type->itUserType, yyvsp[0].identifier))
	warn("conflicting user types (%s, %s)",
	     yyval.type->itUserType, yyvsp[0].identifier);
    yyval.type->itUserType = yyvsp[0].identifier;

    if ((yyval.type->itServerType != strNULL) && !streql(yyval.type->itServerType, yyvsp[0].identifier))
	warn("conflicting server types (%s, %s)",
	     yyval.type->itServerType, yyvsp[0].identifier);
    yyval.type->itServerType = yyvsp[0].identifier;
;
    break;}
case 45:
#line 418 "../gnumach/mig/parser.y"
{
    yyval.type = yyvsp[-3].type;

    if ((yyval.type->itUserType != strNULL) && !streql(yyval.type->itUserType, yyvsp[0].identifier))
	warn("conflicting user types (%s, %s)",
	     yyval.type->itUserType, yyvsp[0].identifier);
    yyval.type->itUserType = yyvsp[0].identifier;
;
    break;}
case 46:
#line 428 "../gnumach/mig/parser.y"
{
    yyval.type = yyvsp[-3].type;

    if ((yyval.type->itServerType != strNULL) && !streql(yyval.type->itServerType, yyvsp[0].identifier))
	warn("conflicting server types (%s, %s)",
	     yyval.type->itServerType, yyvsp[0].identifier);
    yyval.type->itServerType = yyvsp[0].identifier;
;
    break;}
case 47:
#line 439 "../gnumach/mig/parser.y"
{ yyval.type = yyvsp[0].type; ;
    break;}
case 48:
#line 441 "../gnumach/mig/parser.y"
{ yyval.type = yyvsp[0].type; ;
    break;}
case 49:
#line 443 "../gnumach/mig/parser.y"
{ yyval.type = itVarArrayDecl(yyvsp[-1].number, yyvsp[0].type); ;
    break;}
case 50:
#line 445 "../gnumach/mig/parser.y"
{ yyval.type = itArrayDecl(yyvsp[-1].number, yyvsp[0].type); ;
    break;}
case 51:
#line 447 "../gnumach/mig/parser.y"
{ yyval.type = itPtrDecl(yyvsp[0].type); ;
    break;}
case 52:
#line 449 "../gnumach/mig/parser.y"
{ yyval.type = itStructDecl(yyvsp[-1].number, yyvsp[0].type); ;
    break;}
case 53:
#line 451 "../gnumach/mig/parser.y"
{ yyval.type = yyvsp[0].type; ;
    break;}
case 54:
#line 455 "../gnumach/mig/parser.y"
{
    yyval.type = itShortDecl(yyvsp[0].symtype.innumber, yyvsp[0].symtype.instr,
		     yyvsp[0].symtype.outnumber, yyvsp[0].symtype.outstr,
		     yyvsp[0].symtype.size);
;
    break;}
case 55:
#line 462 "../gnumach/mig/parser.y"
{
    yyval.type = itLongDecl(yyvsp[-4].symtype.innumber, yyvsp[-4].symtype.instr,
		    yyvsp[-4].symtype.outnumber, yyvsp[-4].symtype.outstr,
		    yyvsp[-4].symtype.size, yyvsp[-2].number, yyvsp[-1].flag);
;
    break;}
case 56:
#line 470 "../gnumach/mig/parser.y"
{ yyval.flag = flNone; ;
    break;}
case 57:
#line 472 "../gnumach/mig/parser.y"
{
    if (yyvsp[-2].flag & yyvsp[0].flag)
	warn("redundant IPC flag ignored");
    else
	yyval.flag = yyvsp[-2].flag | yyvsp[0].flag;
;
    break;}
case 58:
#line 479 "../gnumach/mig/parser.y"
{
    if (yyvsp[-2].flag != flDealloc)
	warn("only Dealloc is variable");
    else
	yyval.flag = yyvsp[-4].flag | flMaybeDealloc;
;
    break;}
case 59:
#line 488 "../gnumach/mig/parser.y"
{
    yyval.symtype.innumber = yyval.symtype.outnumber = yyvsp[0].number;
    yyval.symtype.instr = yyval.symtype.outstr = strNULL;
    yyval.symtype.size = 0;
;
    break;}
case 60:
#line 494 "../gnumach/mig/parser.y"
{ yyval.symtype = yyvsp[0].symtype; ;
    break;}
case 61:
#line 498 "../gnumach/mig/parser.y"
{ yyval.symtype = yyvsp[0].symtype; ;
    break;}
case 62:
#line 500 "../gnumach/mig/parser.y"
{
    if (yyvsp[-2].symtype.size != yyvsp[0].symtype.size)
    {
	if (yyvsp[-2].symtype.size == 0)
	    yyval.symtype.size = yyvsp[0].symtype.size;
	else if (yyvsp[0].symtype.size == 0)
	    yyval.symtype.size = yyvsp[-2].symtype.size;
	else
	{
	    error("sizes in IPCTypes (%d, %d) aren't equal",
		  yyvsp[-2].symtype.size, yyvsp[0].symtype.size);
	    yyval.symtype.size = 0;
	}
    }
    else
	yyval.symtype.size = yyvsp[-2].symtype.size;
    yyval.symtype.innumber = yyvsp[-2].symtype.innumber;
    yyval.symtype.instr = yyvsp[-2].symtype.instr;
    yyval.symtype.outnumber = yyvsp[0].symtype.outnumber;
    yyval.symtype.outstr = yyvsp[0].symtype.outstr;
;
    break;}
case 63:
#line 524 "../gnumach/mig/parser.y"
{ yyval.type = itPrevDecl(yyvsp[0].identifier); ;
    break;}
case 64:
#line 528 "../gnumach/mig/parser.y"
{ yyval.number = 0; ;
    break;}
case 65:
#line 530 "../gnumach/mig/parser.y"
{ yyval.number = 0; ;
    break;}
case 66:
#line 533 "../gnumach/mig/parser.y"
{ yyval.number = yyvsp[-2].number; ;
    break;}
case 67:
#line 537 "../gnumach/mig/parser.y"
{ yyval.number = yyvsp[-2].number; ;
    break;}
case 68:
#line 541 "../gnumach/mig/parser.y"
{ yyval.number = yyvsp[-2].number; ;
    break;}
case 69:
#line 545 "../gnumach/mig/parser.y"
{ yyval.type = itCStringDecl(yyvsp[-1].number, FALSE); ;
    break;}
case 70:
#line 548 "../gnumach/mig/parser.y"
{ yyval.type = itCStringDecl(yyvsp[-1].number, TRUE); ;
    break;}
case 71:
#line 552 "../gnumach/mig/parser.y"
{ yyval.number = yyvsp[-2].number + yyvsp[0].number;	;
    break;}
case 72:
#line 554 "../gnumach/mig/parser.y"
{ yyval.number = yyvsp[-2].number - yyvsp[0].number;	;
    break;}
case 73:
#line 556 "../gnumach/mig/parser.y"
{ yyval.number = yyvsp[-2].number * yyvsp[0].number;	;
    break;}
case 74:
#line 558 "../gnumach/mig/parser.y"
{ yyval.number = yyvsp[-2].number / yyvsp[0].number;	;
    break;}
case 75:
#line 560 "../gnumach/mig/parser.y"
{ yyval.number = yyvsp[0].number;	;
    break;}
case 76:
#line 562 "../gnumach/mig/parser.y"
{ yyval.number = yyvsp[-1].number;	;
    break;}
case 77:
#line 566 "../gnumach/mig/parser.y"
{ yyval.routine = yyvsp[0].routine; ;
    break;}
case 78:
#line 567 "../gnumach/mig/parser.y"
{ yyval.routine = yyvsp[0].routine; ;
    break;}
case 79:
#line 568 "../gnumach/mig/parser.y"
{ yyval.routine = yyvsp[0].routine; ;
    break;}
case 80:
#line 569 "../gnumach/mig/parser.y"
{ yyval.routine = yyvsp[0].routine; ;
    break;}
case 81:
#line 570 "../gnumach/mig/parser.y"
{ yyval.routine = yyvsp[0].routine; ;
    break;}
case 82:
#line 574 "../gnumach/mig/parser.y"
{ yyval.routine = rtMakeRoutine(yyvsp[-1].identifier, yyvsp[0].argument); ;
    break;}
case 83:
#line 578 "../gnumach/mig/parser.y"
{ yyval.routine = rtMakeSimpleRoutine(yyvsp[-1].identifier, yyvsp[0].argument); ;
    break;}
case 84:
#line 582 "../gnumach/mig/parser.y"
{ yyval.routine = rtMakeProcedure(yyvsp[-1].identifier, yyvsp[0].argument); ;
    break;}
case 85:
#line 586 "../gnumach/mig/parser.y"
{ yyval.routine = rtMakeSimpleProcedure(yyvsp[-1].identifier, yyvsp[0].argument); ;
    break;}
case 86:
#line 590 "../gnumach/mig/parser.y"
{ yyval.routine = rtMakeFunction(yyvsp[-2].identifier, yyvsp[-1].argument, yyvsp[0].type); ;
    break;}
case 87:
#line 594 "../gnumach/mig/parser.y"
{ yyval.argument = argNULL; ;
    break;}
case 88:
#line 596 "../gnumach/mig/parser.y"
{ yyval.argument = yyvsp[-1].argument; ;
    break;}
case 89:
#line 601 "../gnumach/mig/parser.y"
{ yyval.argument = yyvsp[0].argument; ;
    break;}
case 90:
#line 603 "../gnumach/mig/parser.y"
{
    yyval.argument = yyvsp[-2].argument;
    yyval.argument->argNext = yyvsp[0].argument;
;
    break;}
case 91:
#line 610 "../gnumach/mig/parser.y"
{
    yyval.argument = argAlloc();
    yyval.argument->argKind = yyvsp[-3].direction;
    yyval.argument->argName = yyvsp[-2].identifier;
    yyval.argument->argType = yyvsp[-1].type;
    yyval.argument->argFlags = yyvsp[0].flag;
;
    break;}
case 92:
#line 619 "../gnumach/mig/parser.y"
{ yyval.direction = akNone; ;
    break;}
case 93:
#line 620 "../gnumach/mig/parser.y"
{ yyval.direction = akIn; ;
    break;}
case 94:
#line 621 "../gnumach/mig/parser.y"
{ yyval.direction = akOut; ;
    break;}
case 95:
#line 622 "../gnumach/mig/parser.y"
{ yyval.direction = akInOut; ;
    break;}
case 96:
#line 623 "../gnumach/mig/parser.y"
{ yyval.direction = akRequestPort; ;
    break;}
case 97:
#line 624 "../gnumach/mig/parser.y"
{ yyval.direction = akReplyPort; ;
    break;}
case 98:
#line 625 "../gnumach/mig/parser.y"
{ yyval.direction = akSReplyPort; ;
    break;}
case 99:
#line 626 "../gnumach/mig/parser.y"
{ yyval.direction = akUReplyPort; ;
    break;}
case 100:
#line 627 "../gnumach/mig/parser.y"
{ yyval.direction = akWaitTime; ;
    break;}
case 101:
#line 628 "../gnumach/mig/parser.y"
{ yyval.direction = akMsgOption; ;
    break;}
case 102:
#line 629 "../gnumach/mig/parser.y"
{ yyval.direction = akMsgSeqno; ;
    break;}
case 103:
#line 633 "../gnumach/mig/parser.y"
{
    yyval.type = itLookUp(yyvsp[0].identifier);
    if (yyval.type == itNULL)
	error("type '%s' not defined", yyvsp[0].identifier);
;
    break;}
case 104:
#line 639 "../gnumach/mig/parser.y"
{ yyval.type = yyvsp[0].type; ;
    break;}
case 105:
#line 643 "../gnumach/mig/parser.y"
{ LookString(); ;
    break;}
case 106:
#line 647 "../gnumach/mig/parser.y"
{ LookFileName(); ;
    break;}
case 107:
#line 651 "../gnumach/mig/parser.y"
{ LookQString(); ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 487 "/usr/share/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;
}
#line 654 "../gnumach/mig/parser.y"


static const char *
import_name(statement_kind_t sk)
{
    switch (sk)
    {
      case skImport:
	return "Import";
      case skSImport:
	return "SImport";
      case skUImport:
	return "UImport";
      default:
	fatal("import_name(%d): not import statement", (int) sk);
	/*NOTREACHED*/
    }
}
