/*
 * search.c
 *
 * AttoEmacs, Hugh Barney, November 2015
 * Simple forward and reverse search.
 */

#include "header.h"

#define FWD_SEARCH 1
#define REV_SEARCH 2

void search()
{
	int cpos = 0;	
	int c;
	point_t o_point = curbp->b_point;
	point_t found;

	update_search_prompt(m_sprompt, searchtext);
	cpos = strlen(searchtext);

	for (;;) {
		c = getch();
		/* ignore control keys other than C-g, backspace, CR,  C-s, C-R, ESC */
		if (c < 32 && c != 07 && c != 0x08 && c != 0x13 && c != 0x12 && c != 0x1b)
			continue;

		switch(c) {
		case 0x1b: /* esc */
			searchtext[cpos] = '\0';
			return;

		case 0x07: /* ctrl-g */
			curbp->b_point = o_point;
			return;

		case 0x13: /* ctrl-s, do the search */
			found = search_forward(searchtext);
			display_search_result(found, FWD_SEARCH, m_sprompt, searchtext);
			break;

		case 0x12: /* ctrl-r, do the search */
			found = search_backwards(searchtext);
			display_search_result(found, REV_SEARCH, m_sprompt, searchtext);
			break;
			
		case 0x7f: /* del, erase */
		case 0x08: /* backspace */
			if (cpos == 0)
				continue;
			searchtext[--cpos] = '\0';
			update_search_prompt(m_sprompt, searchtext);
			break;

		default:	
			if (cpos < STRBUF_M - 1) {
				searchtext[cpos++] = c;
				searchtext[cpos] = '\0';
				update_search_prompt(m_sprompt, searchtext);
			}
			break;
		}
	}
}

void display_search_result(point_t found, int dir, char *prompt, char *search)
{
	if (found != -1 ) {
		curbp->b_point = found;
		msg("%s%s",prompt, search);
		display(curwp, TRUE);
	} else {
		msg("Failing %s%s",prompt, search);
		dispmsg();
		curbp->b_point = (dir == FWD_SEARCH ? 0 : pos(curbp, curbp->b_ebuf));
	}
}

void update_search_prompt(char *prompt, char *response)
{
	mvaddstr(MSGLINE, 0, prompt);
	/* if we have a value print it and go to end of it */
	if (response[0] != '\0')
		addstr(response);
	clrtoeol();
}

point_t search_forward(char *stext)
{
	point_t end_p = pos(curbp, curbp->b_ebuf);
	point_t p,pp;
	char* s;

	if (0 == strlen(stext))
		return curbp->b_point;

	for (p=curbp->b_point; p < end_p; p++) {
		for (s=stext, pp=p; *s == *(ptr(curbp, pp)) && *s !='\0' && pp < end_p; s++, pp++)
			;

		if (*s == '\0')
			return pp;
	}

	return -1;
}

point_t search_backwards(char *stext)
{
	point_t p,pp;
	char* s;
	
	if (0 == strlen(stext))
		return curbp->b_point;

	for (p=curbp->b_point; p > 0; p--) {
		for (s=stext, pp=p; *s == *(ptr(curbp, pp)) && *s != '\0' && pp > 0; s++, pp++)
			;
		
		if (*s == '\0') {
			if (p > 0)
				p--;
			return p;
		}
	}
	return -1;
}
