/*
 * gap.c, Atto Emacs, Hugh Barney, Public Domain, 2015
 * Derived from: Anthony's Editor January 93, (Public Domain 1991, 1993 by Anthony Howe)
 */

#include <sys/stat.h>
#include "header.h"

/* Enlarge gap by n chars, position of gap cannot change */
int growgap(buffer_t *bp, point_t n)
{
	char_t *new;
	point_t buflen, newlen, xgap, xegap;
		
	ASSERT(bp->b_buf <= bp->b_gap);
	ASSERT(bp->b_gap <= bp->b_egap);
	ASSERT(bp->b_egap <= bp->b_ebuf);
	xgap = bp->b_gap - bp->b_buf;
	xegap = bp->b_egap - bp->b_buf;
	buflen = bp->b_ebuf - bp->b_buf;
    
	/* reduce number of reallocs by growing by a minimum amount */
	n = (n < MIN_GAP_EXPAND ? MIN_GAP_EXPAND : n);
	newlen = buflen + n * sizeof (char_t);

	if (buflen == 0) {
            if (newlen < 0 || MAX_SIZE_T < newlen) {
                fatal("%s: Failed to allocate required memory.\n");
                return (-1);
            }
		new = (char_t*) MALLOC((size_t) newlen);
		if (new == NULL) {
                    fatal("%s: Failed to allocate required memory.\n");	/* Cannot edit a file without a buffer. */
                    return (-1);
                }
	} else {
		if (newlen < 0 || MAX_SIZE_T < newlen) {
			msg("Failed to allocate required memory");
			return (FALSE);
		}
		new = (char_t*) REALLOC(bp->b_buf, (size_t) newlen);
		if (new == NULL) {
			msg("Failed to allocate required memory");    /* Report non-fatal error. */
			return (FALSE);
		}
	}

	/* Relocate pointers in new buffer and append the new
	 * extension to the end of the gap.
	 */
	bp->b_buf = new;
	bp->b_gap = bp->b_buf + xgap;      
	bp->b_ebuf = bp->b_buf + buflen;
	bp->b_egap = bp->b_buf + newlen;
	while (xegap < buflen--)
		*--bp->b_egap = *--bp->b_ebuf;
	bp->b_ebuf = bp->b_buf + newlen;

	ASSERT(bp->b_buf < bp->b_ebuf);          /* Buffer must exist. */
	ASSERT(bp->b_buf <= bp->b_gap);
	ASSERT(bp->b_gap < bp->b_egap);          /* Gap must grow only. */
	ASSERT(bp->b_egap <= bp->b_ebuf);
	return (TRUE);
}

point_t movegap(buffer_t *bp, point_t offset)
{
	char_t *p = ptr(bp, offset);
	while (p < bp->b_gap)
		*--bp->b_egap = *--bp->b_gap;
	while (bp->b_egap < p)
		*bp->b_gap++ = *bp->b_egap++;
	ASSERT(bp->b_gap <= bp->b_egap);
	ASSERT(bp->b_buf <= bp->b_gap);
	ASSERT(bp->b_egap <= bp->b_ebuf);
	return (pos(bp, bp->b_egap));
}

/* Given a buffer offset, convert it to a pointer into the buffer */
char_t * ptr(buffer_t *bp, register point_t offset)
{
	if (offset < 0)
		return (bp->b_buf);
	return (bp->b_buf+offset + (bp->b_buf + offset < bp->b_gap ? 0 : bp->b_egap-bp->b_gap));
}

/* Given a pointer into the buffer, convert it to a buffer offset */
point_t pos(buffer_t *bp, register char_t *cp)
{
	ASSERT(bp->b_buf <= cp && cp <= bp->b_ebuf);
	return (cp - bp->b_buf - (cp < bp->b_egap ? 0 : bp->b_egap - bp->b_gap));
}

int posix_file(char *fn)
{
	if (fn[0] == '_')
		return (FALSE);

	for (; *fn != '\0'; ++fn) {
            if (!isalnum((int)*fn) && *fn != '.' && *fn != '_' && *fn != '-' && *fn != '/')
			return (FALSE);
	}
	return (TRUE);
}

int save(char *fn)
{
	struct fs_file_t file;
	point_t length;
		
	if (!posix_file(fn)) {
		msg("Not a portable POSIX file name.");
		return (FALSE);
	}
	if (fs_open(&file, fn, FS_WRITE | FS_CREAT | FS_TRUNC) != 0) {
		msg("Failed to open file \"%s\".", fn);
		return (FALSE);
	}
	(void) movegap(curbp, (point_t) 0);
	length = (point_t) (curbp->b_ebuf - curbp->b_egap);
	if (fs_write(&file, curbp->b_egap, length) != length) {
		msg("Failed to write file \"%s\".", fn);
		return (FALSE);
	}
	if (fs_close(&file) != 0) {
		msg("Failed to close file \"%s\".", fn);
		return (FALSE);
	}
	curbp->b_flags &= ~B_MODIFIED;
	msg("File \"%s\" %ld bytes saved.", fn, pos(curbp, curbp->b_ebuf));
	return (TRUE);
}

int load_file(char *fn)
{
	/* reset the gap, make it the whole buffer */
	curbp->b_gap = curbp->b_buf;
	curbp->b_egap = curbp->b_ebuf;
	top();
	return insert_file(fn, FALSE);
}

/* reads file into buffer at point */
int insert_file(char *fn, int modflag)
{
	struct fs_file_t file;
	size_t len;
	struct fs_stat_t sb;

	if (fs_stat(fn, &sb) < 0) {
		msg("Failed to find file \"%s\".", fn);
		return (FALSE);
	}
	if (MAX_SIZE_T < sb.size) {
		msg("File \"%s\" is too big to load.", fn);
		return (FALSE);
	}
	if (curbp->b_egap - curbp->b_gap < sb.size * sizeof (char_t) && !growgap(curbp, sb.size))
		return (FALSE);
	if (fs_open(&file, fn, FS_READ) != 0) {
		msg("Failed to open file \"%s\".", fn);
		return (FALSE);
	}
	curbp->b_point = movegap(curbp, curbp->b_point);
	undoset();
	curbp->b_gap += len = fs_read(&file, curbp->b_gap, sb.size);

	if (fs_close(&file) != 0) {
		msg("Failed to close file \"%s\".", fn);
		return (FALSE);
	}
	curbp->b_flags &= (modflag ? B_MODIFIED : ~B_MODIFIED);
	msg("File \"%s\" %ld bytes read.", fn, len);
	return (TRUE);
}

/* Record a new undo location */
void undoset()
{
	curbp->b_ubuf.u_point = curbp->b_point;
	curbp->b_ubuf.u_gap = curbp->b_gap - curbp->b_buf;
	curbp->b_ubuf.u_egap = curbp->b_egap - curbp->b_buf;
}

/* Undo */
void undo()
{
	undo_t tmp;
	memcpy(&tmp, &(curbp->b_ubuf), sizeof (undo_t));
	undoset();
	curbp->b_point = tmp.u_point;
	curbp->b_gap = curbp->b_buf + tmp.u_gap;
	curbp->b_egap = curbp->b_buf + tmp.u_egap;
	curbp->b_flags |= B_MODIFIED;
}

/* additional support funtions not in original gap.c */

/* find the point for start of line ln */
point_t line_to_point(int ln)
{
	point_t end_p = pos(curbp, curbp->b_ebuf);
	point_t p, start;

	for (p=0, start=0; p < end_p; p++) {
		if ( *(ptr(curbp, p)) == '\n') {
			if (--ln == 0)
				return start;
			if (p + 1 < end_p) 
				start = p + 1;
		}
	}
	return -1;
}

/* scan buffer and fill in curline and lastline */
void get_line_stats(int *curline, int *lastline)
{
	point_t end_p = pos(curbp, curbp->b_ebuf);
	point_t p;
	int line;
    
	*curline = -1;
    
	for (p=0, line=0; p < end_p; p++) {
		line += (*(ptr(curbp,p)) == '\n') ? 1 : 0;
		*lastline = line;
        
		if (*curline == -1 && p == curbp->b_point) {
			*curline = (*(ptr(curbp,p)) == '\n') ? line : line + 1;
		}
	}

	*lastline = *lastline + 1;
	
	if (curbp->b_point == end_p)
		*curline = *lastline;
}
