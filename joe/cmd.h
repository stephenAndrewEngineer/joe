/*
 *	Command execution
 *	Copyright
 *		(C) 1992 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */

/* Command entry */

struct cmd {
	const char *name;	/* Command name */
	int	flag;		/* Execution flags */
	int	(*func) (W *w, int k);	/* Function bound to name */
	MACRO	*m;		/* Macro bound to name */
	int	arg;		/* 0= arg is meaningless, 1= ok */
	const char *negarg;	/* Command to use if arg was negative */
};

extern int joe_beep;		/* Enable beep on command error */

/* Command execution flags */

#define EMINOR		  8	/* Full screen update not needed */
#define EMETA		0x10000	/* JM: Executes even when if flag is zero */

/* CMD *findcmd(char *s);
 * Return command address for given name
 */
CMD *findcmd(const char *s);
void addcmd(const char *s, MACRO *m);

/* Execute a command.  Returns return value of command */
int execmd(CMD *cmd, int k);
void do_auto_scroll();

extern B *cmdhist; /* Command history buffer */

int modify_logic(BW *bw,B *b);

extern int nolocks; /* Disable file locking */
extern int nomodcheck; /* Disable file modified check */
