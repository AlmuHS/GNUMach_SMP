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


extern YYSTYPE yylval;
