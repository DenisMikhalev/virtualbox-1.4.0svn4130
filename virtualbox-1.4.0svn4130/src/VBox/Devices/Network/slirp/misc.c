/*
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#define WANT_SYS_IOCTL_H
#include <slirp.h>


#if 0
int x_port = -1;
int x_display = 0;
int x_screen = 0;

int
show_x(buff, inso)
	char *buff;
	struct socket *inso;
{
	if (x_port < 0) {
		lprint("X Redir: X not being redirected.\r\n");
	} else {
		lprint("X Redir: In sh/bash/zsh/etc. type: DISPLAY=%s:%d.%d; export DISPLAY\r\n",
		      inet_ntoa(our_addr), x_port, x_screen);
		lprint("X Redir: In csh/tcsh/etc. type:    setenv DISPLAY %s:%d.%d\r\n",
		      inet_ntoa(our_addr), x_port, x_screen);
		if (x_display)
		   lprint("X Redir: Redirecting to display %d\r\n", x_display);
	}

	return CFG_OK;
}


/*
 * XXX Allow more than one X redirection?
 */
void
redir_x(inaddr, start_port, display, screen)
	u_int32_t inaddr;
	int start_port;
	int display;
	int screen;
{
	int i;

	if (x_port >= 0) {
		lprint("X Redir: X already being redirected.\r\n");
		show_x(0, 0);
	} else {
		for (i = 6001 + (start_port-1); i <= 6100; i++) {
			if (solisten(htons(i), inaddr, htons(6000 + display), 0)) {
				/* Success */
				x_port = i - 6000;
				x_display = display;
				x_screen = screen;
				show_x(0, 0);
				return;
			}
		}
		lprint("X Redir: Error: Couldn't redirect a port for X. Weird.\r\n");
	}
}
#endif

#ifndef HAVE_INET_ATON
int
inet_aton(cp, ia)
	const char *cp;
	struct in_addr *ia;
{
	u_int32_t addr = inet_addr(cp);
	if (addr == 0xffffffff)
		return 0;
	ia->s_addr = addr;
	return 1;
}
#endif

/*
 * Get our IP address and put it in our_addr
 */
void
getouraddr(PNATState pData)
{
	char buff[256];
	struct hostent *he = NULL;

	if (gethostname(buff,256) == 0)
            he = gethostbyname(buff);
        if (he)
            our_addr = *(struct in_addr *)he->h_addr;
        if (our_addr.s_addr == 0)
            our_addr.s_addr = loopback_addr.s_addr;
}

#if SIZEOF_CHAR_P == 8

struct quehead_32 {
	u_int32_t qh_link;
	u_int32_t qh_rlink;
};

void
insque_32(PNATState pData, void *a, void *b)
{
	register struct quehead_32 *element = (struct quehead_32 *) a;
	register struct quehead_32 *head = (struct quehead_32 *) b;
	struct quehead_32 *link = u32_to_ptr(pData, head->qh_link, struct quehead_32 *);

	element->qh_link = head->qh_link;
	element->qh_rlink = ptr_to_u32(pData, head);
        Assert(link->qh_rlink == element->qh_rlink);
	link->qh_rlink = head->qh_link = ptr_to_u32(pData, element);
}

void
remque_32(PNATState pData, void *a)
{
	register struct quehead_32 *element = (struct quehead_32 *) a;
	struct quehead_32 *link = u32_to_ptr(pData, element->qh_link, struct quehead_32 *);
	struct quehead_32 *rlink = u32_to_ptr(pData, element->qh_rlink, struct quehead_32 *);

	u32ptr_done(pData, link->qh_rlink, element);
	link->qh_rlink = element->qh_rlink;
	rlink->qh_link = element->qh_link;
	element->qh_rlink = 0;
}

#endif /* SIZEOF_CHAR_P == 8 */

struct quehead {
	struct quehead *qh_link;
	struct quehead *qh_rlink;
};

void
insque(PNATState pData, void *a, void *b)
{
	register struct quehead *element = (struct quehead *) a;
	register struct quehead *head = (struct quehead *) b;
	element->qh_link = head->qh_link;
	head->qh_link = (struct quehead *)element;
	element->qh_rlink = (struct quehead *)head;
	((struct quehead *)(element->qh_link))->qh_rlink
	= (struct quehead *)element;
}

void
remque(PNATState pData, void *a)
{
  register struct quehead *element = (struct quehead *) a;
  ((struct quehead *)(element->qh_link))->qh_rlink = element->qh_rlink;
  ((struct quehead *)(element->qh_rlink))->qh_link = element->qh_link;
  element->qh_rlink = NULL;
  /*  element->qh_link = NULL;  TCP FIN1 crashes if you do this.  Why ? */
}

/* #endif */


int
add_exec(ex_ptr, do_pty, exec, addr, port)
	struct ex_list **ex_ptr;
	int do_pty;
	char *exec;
	int addr;
	int port;
{
	struct ex_list *tmp_ptr;

	/* First, check if the port is "bound" */
	for (tmp_ptr = *ex_ptr; tmp_ptr; tmp_ptr = tmp_ptr->ex_next) {
		if (port == tmp_ptr->ex_fport && addr == tmp_ptr->ex_addr)
		   return -1;
	}

	tmp_ptr = *ex_ptr;
	*ex_ptr = (struct ex_list *)malloc(sizeof(struct ex_list));
	(*ex_ptr)->ex_fport = port;
	(*ex_ptr)->ex_addr = addr;
	(*ex_ptr)->ex_pty = do_pty;
	(*ex_ptr)->ex_exec = strdup(exec);
	(*ex_ptr)->ex_next = tmp_ptr;
	return 0;
}

#ifndef HAVE_STRERROR

/*
 * For systems with no strerror
 */

extern int sys_nerr;
extern char *sys_errlist[];

char *
strerror(error)
	int error;
{
	if (error < sys_nerr)
	   return sys_errlist[error];
	else
	   return "Unknown error.";
}

#endif


#ifdef _WIN32

int
fork_exec(PNATState pData, struct socket *so, char *ex, int do_pty)
{
    /* not implemented */
    return 0;
}

#else

/*
 * XXX This is ugly
 * We create and bind a socket, then fork off to another
 * process, which connects to this socket, after which we
 * exec the wanted program.  If something (strange) happens,
 * the accept() call could block us forever.
 *
 * do_pty = 0   Fork/exec inetd style
 * do_pty = 1   Fork/exec using slirp.telnetd
 * do_ptr = 2   Fork/exec using pty
 */
int
fork_exec(PNATState pData, struct socket *so, char *ex, int do_pty)
{
	int s;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int opt;
        int master;
	char *argv[256];
#if 0
	char buff[256];
#endif
	/* don't want to clobber the original */
	char *bptr;
	char *curarg;
	int c, i, ret;

	DEBUG_CALL("fork_exec");
	DEBUG_ARG("so = %lx", (long)so);
	DEBUG_ARG("ex = %lx", (long)ex);
	DEBUG_ARG("do_pty = %lx", (long)do_pty);

	if (do_pty == 2) {
                AssertRelease(do_pty != 2);
                /* shut up gcc */
                s = 0;
                master = 0;
	} else {
		addr.sin_family = AF_INET;
		addr.sin_port = 0;
		addr.sin_addr.s_addr = INADDR_ANY;

		if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ||
		    bind(s, (struct sockaddr *)&addr, addrlen) < 0 ||
		    listen(s, 1) < 0) {
			lprint("Error: inet socket: %s\n", strerror(errno));
			closesocket(s);

			return 0;
		}
	}

	switch(fork()) {
	 case -1:
		lprint("Error: fork failed: %s\n", strerror(errno));
		close(s);
		if (do_pty == 2)
		   close(master);
		return 0;

	 case 0:
		/* Set the DISPLAY */
		if (do_pty == 2) {
			(void) close(master);
#ifdef TIOCSCTTY /* XXXXX */
			(void) setsid();
			ioctl(s, TIOCSCTTY, (char *)NULL);
#endif
		} else {
			getsockname(s, (struct sockaddr *)&addr, &addrlen);
			close(s);
			/*
			 * Connect to the socket
			 * XXX If any of these fail, we're in trouble!
	 		 */
			s = socket(AF_INET, SOCK_STREAM, 0);
			addr.sin_addr = loopback_addr;
                        do {
                            ret = connect(s, (struct sockaddr *)&addr, addrlen);
                        } while (ret < 0 && errno == EINTR);
		}

#if 0
		if (x_port >= 0) {
#ifdef HAVE_SETENV
			sprintf(buff, "%s:%d.%d", inet_ntoa(our_addr), x_port, x_screen);
			setenv("DISPLAY", buff, 1);
#else
			sprintf(buff, "DISPLAY=%s:%d.%d", inet_ntoa(our_addr), x_port, x_screen);
			putenv(buff);
#endif
		}
#endif
		dup2(s, 0);
		dup2(s, 1);
		dup2(s, 2);
		for (s = 3; s <= 255; s++)
		   close(s);

		i = 0;
		bptr = strdup(ex); /* No need to free() this */
		if (do_pty == 1) {
			/* Setup "slirp.telnetd -x" */
			argv[i++] = "slirp.telnetd";
			argv[i++] = "-x";
			argv[i++] = bptr;
		} else
		   do {
			/* Change the string into argv[] */
			curarg = bptr;
			while (*bptr != ' ' && *bptr != (char)0)
			   bptr++;
			c = *bptr;
			*bptr++ = (char)0;
			argv[i++] = strdup(curarg);
		   } while (c);

		argv[i] = 0;
		execvp(argv[0], argv);

		/* Ooops, failed, let's tell the user why */
		  {
			  char buff[256];

			  sprintf(buff, "Error: execvp of %s failed: %s\n",
				  argv[0], strerror(errno));
			  write(2, buff, strlen(buff)+1);
		  }
		close(0); close(1); close(2); /* XXX */
		exit(1);

	 default:
		if (do_pty == 2) {
			close(s);
			so->s = master;
		} else {
			/*
			 * XXX this could block us...
			 * XXX Should set a timer here, and if accept() doesn't
		 	 * return after X seconds, declare it a failure
		 	 * The only reason this will block forever is if socket()
		 	 * of connect() fail in the child process
		 	 */
                        do {
                            so->s = accept(s, (struct sockaddr *)&addr, &addrlen);
                        } while (so->s < 0 && errno == EINTR);
                        closesocket(s);
			opt = 1;
			setsockopt(so->s,SOL_SOCKET,SO_REUSEADDR,(char *)&opt,sizeof(int));
			opt = 1;
			setsockopt(so->s,SOL_SOCKET,SO_OOBINLINE,(char *)&opt,sizeof(int));
		}
		fd_nonblock(so->s);

		/* Append the telnet options now */
		if (so->so_m != 0 && do_pty == 1)  {
			sbappend(pData, so, so->so_m);
			so->so_m = 0;
		}

		return 1;
	}
}
#endif

#ifndef HAVE_STRDUP
char *
strdup(str)
	const char *str;
{
	char *bptr;

	bptr = (char *)malloc(strlen(str)+1);
	strcpy(bptr, str);

	return bptr;
}
#endif

#if 0
void
snooze_hup(num)
	int num;
{
	int s, ret;
#ifndef NO_UNIX_SOCKETS
	struct sockaddr_un sock_un;
#endif
	struct sockaddr_in sock_in;
	char buff[256];

	ret = -1;
	if (slirp_socket_passwd) {
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s < 0)
		   slirp_exit(1);
		sock_in.sin_family = AF_INET;
		sock_in.sin_addr.s_addr = slirp_socket_addr;
		sock_in.sin_port = htons(slirp_socket_port);
		if (connect(s, (struct sockaddr *)&sock_in, sizeof(sock_in)) != 0)
		   slirp_exit(1); /* just exit...*/
		sprintf(buff, "kill %s:%d", slirp_socket_passwd, slirp_socket_unit);
		write(s, buff, strlen(buff)+1);
	}
#ifndef NO_UNIX_SOCKETS
	  else {
		s = socket(AF_UNIX, SOCK_STREAM, 0);
		if (s < 0)
		   slirp_exit(1);
		sock_un.sun_family = AF_UNIX;
		strcpy(sock_un.sun_path, socket_path);
		if (connect(s, (struct sockaddr *)&sock_un,
			      sizeof(sock_un.sun_family) + sizeof(sock_un.sun_path)) != 0)
		   slirp_exit(1);
		sprintf(buff, "kill none:%d", slirp_socket_unit);
		write(s, buff, strlen(buff)+1);
	}
#endif
	slirp_exit(0);
}


void
snooze()
{
	sigset_t s;
	int i;

	/* Don't need our data anymore */
	/* XXX This makes SunOS barf */
/*	brk(0); */

	/* Close all fd's */
	for (i = 255; i >= 0; i--)
	   close(i);

	signal(SIGQUIT, slirp_exit);
	signal(SIGHUP, snooze_hup);
	sigemptyset(&s);

	/* Wait for any signal */
	sigsuspend(&s);

	/* Just in case ... */
	exit(255);
}

void
relay(s)
	int s;
{
	char buf[8192];
	int n;
	fd_set readfds;
	struct ttys *ttyp;

	/* Don't need our data anymore */
	/* XXX This makes SunOS barf */
/*	brk(0); */

	signal(SIGQUIT, slirp_exit);
	signal(SIGHUP, slirp_exit);
        signal(SIGINT, slirp_exit);
	signal(SIGTERM, slirp_exit);

	/* Fudge to get term_raw and term_restore to work */
	if (NULL == (ttyp = tty_attach (0, slirp_tty))) {
         lprint ("Error: tty_attach failed in misc.c:relay()\r\n");
         slirp_exit (1);
    }
	ttyp->fd = 0;
	ttyp->flags |= TTY_CTTY;
	term_raw(ttyp);

	while (1) {
		FD_ZERO(&readfds);

		FD_SET(0, &readfds);
		FD_SET(s, &readfds);

		n = select(s+1, &readfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0);

		if (n <= 0)
		   slirp_exit(0);

		if (FD_ISSET(0, &readfds)) {
			n = read(0, buf, 8192);
			if (n <= 0)
			   slirp_exit(0);
			n = writen(s, buf, n);
			if (n <= 0)
			   slirp_exit(0);
		}

		if (FD_ISSET(s, &readfds)) {
			n = read(s, buf, 8192);
			if (n <= 0)
			   slirp_exit(0);
			n = writen(0, buf, n);
			if (n <= 0)
			   slirp_exit(0);
		}
	}

	/* Just in case.... */
	exit(1);
}
#endif

#ifdef BAD_SPRINTF

#undef vsprintf
#undef sprintf

/*
 * Some BSD-derived systems have a sprintf which returns char *
 */

int
vsprintf_len(string, format, args)
	char *string;
	const char *format;
	va_list args;
{
	vsprintf(string, format, args);
	return strlen(string);
}

int
#ifdef __STDC__
sprintf_len(char *string, const char *format, ...)
#else
sprintf_len(va_alist) va_dcl
#endif
{
	va_list args;
#ifdef __STDC__
	va_start(args, format);
#else
	char *string;
	char *format;
	va_start(args);
	string = va_arg(args, char *);
	format = va_arg(args, char *);
#endif
	vsprintf(string, format, args);
	return strlen(string);
}

#endif

void
u_sleep(usec)
	int usec;
{
	struct timeval t;
	fd_set fdset;

	FD_ZERO(&fdset);

	t.tv_sec = 0;
	t.tv_usec = usec * 1000;

	select(0, &fdset, &fdset, &fdset, &t);
}

/*
 * Set fd blocking and non-blocking
 */

void
fd_nonblock(fd)
	int fd;
{
#ifdef FIONBIO
	int opt = 1;

	ioctlsocket(fd, FIONBIO, &opt);
#else
	int opt;

	opt = fcntl(fd, F_GETFL, 0);
	opt |= O_NONBLOCK;
	fcntl(fd, F_SETFL, opt);
#endif
}

void
fd_block(fd)
	int fd;
{
#ifdef FIONBIO
	int opt = 0;

	ioctlsocket(fd, FIONBIO, &opt);
#else
	int opt;

	opt = fcntl(fd, F_GETFL, 0);
	opt &= ~O_NONBLOCK;
	fcntl(fd, F_SETFL, opt);
#endif
}


#if 0
/*
 * invoke RSH
 */
int
rsh_exec(so,ns, user, host, args)
	struct socket *so;
	struct socket *ns;
	char *user;
	char *host;
	char *args;
{
	int fd[2];
	int fd0[2];
	int s;
	char buff[256];

	DEBUG_CALL("rsh_exec");
	DEBUG_ARG("so = %lx", (long)so);

	if (pipe(fd)<0) {
          lprint("Error: pipe failed: %s\n", strerror(errno));
          return 0;
	}
/* #ifdef HAVE_SOCKETPAIR */
#if 1
        if (socketpair(PF_UNIX,SOCK_STREAM,0, fd0) == -1) {
          close(fd[0]);
          close(fd[1]);
          lprint("Error: openpty failed: %s\n", strerror(errno));
          return 0;
        }
#else
        if (slirp_openpty(&fd0[0], &fd0[1]) == -1) {
          close(fd[0]);
          close(fd[1]);
          lprint("Error: openpty failed: %s\n", strerror(errno));
          return 0;
        }
#endif

	switch(fork()) {
	 case -1:
           lprint("Error: fork failed: %s\n", strerror(errno));
           close(fd[0]);
           close(fd[1]);
           close(fd0[0]);
           close(fd0[1]);
           return 0;

	 case 0:
           close(fd[0]);
           close(fd0[0]);

		/* Set the DISPLAY */
           if (x_port >= 0) {
#ifdef HAVE_SETENV
             sprintf(buff, "%s:%d.%d", inet_ntoa(our_addr), x_port, x_screen);
             setenv("DISPLAY", buff, 1);
#else
             sprintf(buff, "DISPLAY=%s:%d.%d", inet_ntoa(our_addr), x_port, x_screen);
             putenv(buff);
#endif
           }

           dup2(fd0[1], 0);
           dup2(fd0[1], 1);
           dup2(fd[1], 2);
           for (s = 3; s <= 255; s++)
             close(s);

           execlp("rsh","rsh","-l", user, host, args, NULL);

           /* Ooops, failed, let's tell the user why */

           sprintf(buff, "Error: execlp of %s failed: %s\n",
                   "rsh", strerror(errno));
           write(2, buff, strlen(buff)+1);
           close(0); close(1); close(2); /* XXX */
           exit(1);

        default:
          close(fd[1]);
          close(fd0[1]);
          ns->s=fd[0];
          so->s=fd0[0];

          return 1;
	}
}
#endif
