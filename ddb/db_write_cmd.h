#ifndef	_DDB_DB_WRITE_CMD_H_
#define	_DDB_DB_WRITE_CMD_H_

#include <mach/boolean.h>
#include <machine/db_machdep.h>

/* Prototypes for functions exported by this module.
 */

void db_write_cmd(
	db_expr_t	address,
	boolean_t	have_addr,
	db_expr_t	count,
	char *		modif);

#endif	/* !_DDB_DB_WRITE_CMD_H_ */
