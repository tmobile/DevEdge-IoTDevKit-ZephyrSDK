/*
 * Copyright (c) 2022 T-Mobile USA, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cdefs.h"
#include "debug.h"
#include "kermit.h"
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#if CONFIG_SHELL_BACKEND_RTT
#include <zephyr/shell/shell_rtt.h>
#endif
#include <SEGGER_RTT.h>
#include <zephyr/fs/fs.h>
#include <zephyr/device.h>

#define EOF -1

// UCHAR o_buf[OBUFLEN+8];			/* File output buffer */
// UCHAR i_buf[IBUFLEN+8];			/* File output buffer */
extern uint8_t mxfer_buf[];
UCHAR *i_buf = (UCHAR*)mxfer_buf;
UCHAR *o_buf = (UCHAR*)&mxfer_buf[2048];

bool zek_ctrl_c_sent;

const struct shell *sh; 
struct fs_file_t ifile, ofile;

#define LREAD

int pktmode(short on)
{
    return 1;
}

int devopen(char * dev)		/* Communications device/path */
{
#if CONFIG_SHELL_BACKEND_RTT
    sh = shell_backend_rtt_get_ptr();
#endif
    return 0;
}

int devsettings(char *a)
{
    ARG_UNUSED(a);
#if CONFIG_SHELL_BACKEND_RTT
    if (sh) {
        shell_uninit(sh, NULL);
        k_msleep(100);
    } 
#endif
    return 1;
}

int devrestore(void)
{
#if CONFIG_SHELL_BACKEND_RTT
    if (shell_backend_rtt_get_ptr() == sh) {
        bool log_backend = CONFIG_SHELL_RTT_INIT_LOG_LEVEL > 0;
        uint32_t level = (CONFIG_SHELL_RTT_INIT_LOG_LEVEL > LOG_LEVEL_DBG) ?
                CONFIG_LOG_MAX_LEVEL : CONFIG_SHELL_RTT_INIT_LOG_LEVEL;
        static const struct shell_backend_config_flags cfg_flags =
                        SHELL_DEFAULT_BACKEND_CONFIG_FLAGS;

        shell_init(sh, NULL, cfg_flags, log_backend, level);
    } 
#endif
    return 0;
}

int devclose(void)
{
    sh = NULL;
    return 0;
}

UCHAR z_getchar()
{
    char c;
    while (!SEGGER_RTT_HasKey() && !zek_ctrl_c_sent)
        k_yield();
    SEGGER_RTT_Read(0, &c, 1);
    return c;
}

size_t z_read(void *buf, size_t n)
{
    while (!SEGGER_RTT_HasKey() && !zek_ctrl_c_sent) k_yield();
    return SEGGER_RTT_Read(0, buf, n);
}

ssize_t z_write(void *buf, size_t cnt)
{
    return SEGGER_RTT_Write(0, buf, cnt);
}

/* I N C H K  --  Check if input waiting */
int inchk(struct k_data * k)
{ 
    return SEGGER_RTT_HasKey();
}

/*  R E A D P K T  --  Read a Kermit packet from the communications device  */
/*
  Call with:
    k   - Kermit struct pointer
    p   - pointer to read buffer
    len - length of read buffer

  When reading a packet, this function looks for start of Kermit packet
  (k->r_soh), then reads everything between it and the end of the packet
  (k->r_eom) into the indicated buffer.  Returns the number of bytes read, or:
     0   - timeout or other possibly correctable error;
    -1   - fatal error, such as loss of connection, or no buffer to read into.
*/
int readpkt(struct k_data * k, UCHAR *p, int len)
{
    //SEGGER_RTT_WriteString(0,"RX\n");
    int x, n, i, plen, read, s;
    short flag;
    bool lp;
    UCHAR c;

/*
  Timeout not implemented in this sample.
  It should not be needed.  All non-embedded Kermits that are capable of
  making connections are also capable of timing out, and only one Kermit
  needs to time out.  NOTE: This simple example waits for SOH and then
  reads everything up to the negotiated packet terminator.  A more robust
  version might be driven by the value of the packet-length field.
*/

#ifdef F_CTRLC
    short ccn;
    ccn = 0;
#endif /* F_CTRLC */

    if (!p) {		/* No buffer */
        return(-1);
    }
    flag = n = s = plen= 0;                       /* Init local variables */
    lp = false;
    while (1) {
#ifdef LREAD
        if (flag & !n) {
            x = z_getchar();
            plen = xunchar((x & 0x7f));
            if (zek_ctrl_c_sent) return -1;
            if (!plen)
                lp = true;
        } else if (flag && plen && n && !(lp && n < 6)) {
            //We have length, so...
            read = z_read(p, plen);
            if (zek_ctrl_c_sent) return -1;
            if (k->parity) {
                for (i = 0; i < read; i++) {
                    *p &= 0x7f;
                    p++;
                }
            } else {
                p += read;
            }
            plen -= read;
            n += read;
            continue;
        } else {
            x = z_getchar();
            if (zek_ctrl_c_sent) return -1;
        }
#else
        x = z_getchar();
#endif
        c = (k->parity) ? x & 0x7f : x & 0xff; /* Strip parity */
#ifdef LREAD
        if (lp) {
            if (n < 5) // generate sum
                s += c;
            if (n == 3)
                plen = 95 * xunchar(x);
            if (n == 4)
                plen += xunchar(x);
            if (n == 5)
                if (tochar((s + ((s & 192) / 64)) & 63) != x)
                    plen = 0; //Bad sum, cannot trust length
        }
#endif

#ifdef F_CTRLC
	/* In remote mode only: three consecutive ^C's to quit */
        if (k->remote && c == (UCHAR) 3) {
            if (++ccn > 2) {
                return(-1);
            }
        } else {
            ccn = 0;
        }
#endif /* F_CTRLC */

        if (!flag && c != k->r_soh)	/* No start of packet yet */
          continue;                     /* so discard these bytes. */
        if (c == k->r_soh) {		/* Start of packet */
            flag = 1;                   /* Remember */
            continue;                   /* But discard. */
        } else if (c == k->r_eom	/* Packet terminator */
		   || c == '\012'	/* 1.3: For HyperTerminal */
		   ) {
            return(n);
        } else {                        /* Contents of packet */
            if (n++ > k->r_maxlen)	/* Check length */
                return(0);
            else
                *p++ = x & 0xff;
        }
    }
    return(-1);
}

/*  T X _ D A T A  --  Writes n bytes of data to communication device.  */
/*
  Call with:
    k = pointer to Kermit struct.
    p = pointer to data to transmit.
    n = length.
  Returns:
    X_OK on success.
    X_ERROR on failure to write - i/o error.
*/
int tx_data(struct k_data * k, UCHAR *p, int n)
{
    //SEGGER_RTT_WriteString(0,"TX\n");
    int x;
    int max;

    max = 10;                           /* Loop breaker */

    while (n > 0) {                     /* Keep trying till done */
        x = z_write(p,n);
        if (x < 0 || --max < 1)         /* Errors are fatal */
            return(X_ERROR);
        n -= x;
	    p += x;
    }
    return(X_OK);                       /* Success */
}


/*  O P E N F I L E  --  Open output file  */
/*
  Call with:
    Pointer to filename.
    Size in bytes.
    Creation date in format yyyymmdd hh:mm:ss, e.g. 19950208 14:00:00
    Mode: 1 = read, 2 = create, 3 = append.
  Returns:
    X_OK on success.
    X_ERROR on failure, including rejection based on name, size, or date.    
*/
int openfile(struct k_data * k, UCHAR * s, int mode)
{
    //SEGGER_RTT_WriteString(0,"OPENFILE\n");
    switch (mode) {
        case 1:				/* Read */
            fs_file_t_init(&ifile);
            if (fs_open(&ifile, s, FS_O_READ)) {
                return(X_ERROR);
            }
            k->s_first   = 1;		/* Set up for getkpt */
            k->zinbuf[0] = '\0';		/* Initialize buffer */
            k->zinptr    = k->zinbuf;	/* Set up buffer pointer */
            k->zincnt    = 0;		/* and count */
            return(X_OK);

        case 2:				/* Write (create) */
            fs_file_t_init(&ofile);
            if (fs_open(&ofile, s, FS_O_CREATE | FS_O_WRITE)) {
                return(X_ERROR);
            }
            return(X_OK);
        case 3:				/* Append (not used) */
            fs_file_t_init(&ofile);
            if (fs_open(&ofile, s, FS_O_CREATE | FS_O_APPEND | FS_O_WRITE)) {
                return(X_ERROR);
            }
            return(X_OK);

      default:
        return(X_ERROR);
    }
}

/*  F I L E I N F O  --  Get info about existing file  */
/*
  Call with:
    Pointer to filename
    Pointer to buffer for date-time string
    Length of date-time string buffer (must be at least 18 bytes)
    Pointer to int file type:
       0: Prevailing type is text.
       1: Prevailing type is binary.
    Transfer mode (0 = auto, 1 = manual):
       0: Figure out whether file is text or binary and return type.
       1: (nonzero) Don't try to figure out file type. 
  Returns:
    X_ERROR on failure.
    0L or greater on success == file length.
    Date-time string set to yyyymmdd hh:mm:ss modtime of file.
    If date can't be determined, first byte of buffer is set to NUL.
    Type set to 0 (text) or 1 (binary) if mode == 0.
*/
#ifdef F_SCAN
#define SCANBUF 128
#define SCANSIZ 49152
#endif /* F_SCAN */

ULONG fileinfo(struct k_data * k,
	 UCHAR * filename, UCHAR * buf, int buflen, short * type, short mode)
{
    //SEGGER_RTT_WriteString(0,"FILEINFO\n");
    struct fs_dirent statbuf;

#ifdef F_SCAN
    char inbuf[SCANBUF];		/* and buffer */
#endif /* F_SCAN */

    if (!buf)
        return(X_ERROR);
    buf[0] = '\0';
    if (buflen < 18)
        return(X_ERROR);
    if (fs_stat(filename, &statbuf) < 0)
        return(X_ERROR);
#ifdef F_SCAN
/*
  Here we determine if the file is text or binary if the transfer mode is
  not forced.  This is an extremely crude sample, which diagnoses any file
  that contains a control character other than HT, LF, FF, or CR as binary.
  A more thorough content analysis can be done that accounts for various
  character sets as well as various forms of Unicode (UTF-8, UTF-16, etc).
  Or the diagnosis could be based wholly or in part on the filename.
  etc etc.  Or the implementation could skip this entirely by not defining
  F_SCAN and/or by always calling this routine with type set to -1.
*/
    if (!mode) {			/* File type determination requested */
        int isbinary = 1;
        struct fs_file_t file;
        fs_file_t_init(&file);
        int stat = fs_open(&file, filename, FS_O_READ);	/* Open the file for scanning */
        if (!stat) {
            int n = 0, count = 0;
            char c, * p;

            isbinary = 0;
            while (count < SCANSIZ && !isbinary) { /* Scan this much */
                n = fs_read(&file, inbuf, SCANBUF);
                if (n == EOF || n == 0)
                    break;
                count += n;
                p = inbuf;
                while (n--) {
                    c = *p++;
                    if (c < 32 || c == 127) {
                        if (c !=  9 &&	/* Tab */
                            c != 10 &&	/* LF */
                            c != 12 &&	/* FF */
                            c != 13) {	/* CR */
                            isbinary = 1;
                            break;
                        }
                    }
                }
            }
            fs_close(&file);
            *type = isbinary;
        }
    }
#endif /* F_SCAN */

    return ((ULONG)(statbuf.size));
}


/*  R E A D F I L E  --  Read data from a file  */
int readfile(struct k_data * k)
{
    //SEGGER_RTT_WriteString(0, "READFILE\n");
    if (!k->zinptr) {
	    return(X_ERROR);
    }
    if (k->zincnt < 1) {		/* Nothing in buffer - must refill */
        if (k->binary) {		/* Binary - just read raw buffers */
            k->dummy = 0;
            k->zincnt = fs_read(&ifile, k->zinbuf, k->zinlen);

        } else {			/* Text mode needs LF/CRLF handling */
            int c;			/* Current character */
            for (k->zincnt = 0; (k->zincnt < (k->zinlen - 2)); (k->zincnt)++) {
                char chr;
                c = fs_read(&ifile, &chr, 1);
                c = c ? chr : EOF;
                if (c == EOF)
                    break;

                if (c == '\n')		/* Have newline? */
                    k->zinbuf[(k->zincnt)++] = '\r'; /* Insert CR */
                k->zinbuf[k->zincnt] = c;
            }
        }
        k->zinbuf[k->zincnt] = '\0';	/* Terminate. */
        if (k->zincnt == 0)		/* Check for EOF */
            return -1;
        k->zinptr = k->zinbuf;		/* Not EOF - reset pointer */
    }
    (k->zincnt)--;			/* Return first byte. */

    return (*(k->zinptr)++ & 0xff);
}



/*  W R I T E F I L E  --  Write data to file  */
/*
  Call with:
    Kermit struct
    String pointer
    Length
  Returns:
    X_OK on success
    X_ERROR on failure, such as i/o error, space used up, etc
*/
int writefile(struct k_data * k, UCHAR * s, int n)
{
    int rc;
    rc = X_OK;

    if (k->binary) {			/* Binary mode, just write it */
        if (fs_write(&ofile, s, n) != n)
            rc = X_ERROR;
    } else {				/* Text mode, skip CRs */
        UCHAR * p, * q;
        int i;
        q = s;

        while (1) {
            for (p = q, i = 0; ((*p) && (*p != (UCHAR)13)); p++, i++) ;
            if (i > 0)
                if (fs_write(&ofile, s, n) != i)
                    rc = X_ERROR;
            if (!*p) break;
            q = p+1;
        }
    }
    return(rc);
}


/*  C L O S E F I L E  --  Close output file  */
/*
  Mode = 1 for input file, mode = 2 or 3 for output file.

  For output files, the character c is the character (if any) from the Z
  packet data field.  If it is D, it means the file transfer was canceled
  in midstream by the sender, and the file is therefore incomplete.  This
  routine should check for that and decide what to do.  It should be
  harmless to call this routine for a file that that is not open.
*/
int closefile(struct k_data * k, UCHAR c, int mode)
{
    int rc = X_OK;			/* Return code */
    int stat;
    switch (mode) {
        case 1:				/* Closing input file */
            stat = fs_close(&ifile);
            if (stat == -EALREADY) /* If not not open */
                break;			/* do nothing but succeed */
            if (stat < 0)
               rc = X_ERROR;
            break;
        case 2:				/* Closing output file */
        case 3:
            stat = fs_close(&ofile);
            if (stat == -EALREADY) /* If not not open */
                break;			/* do nothing but succeed */
            if (stat < 0) {		/* Try to close */
                rc = X_ERROR;
            } else if ((k->ikeep == 0) &&	/* Don't keep incomplete files */
                  (c == 'D')) {	/* This file was incomplete */
                if (k->filename) {
                    fs_unlink(k->filename);	/* Delete it. */
                }
            }
	        break;
        default:
	        rc = X_ERROR;
    }
    return(rc);
}
