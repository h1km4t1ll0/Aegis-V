#ifndef GCC_INTL_H
#define GCC_INTL_H

#define _(msgid) (msgid)
#define N_(msgid) msgid
#define G_(gmsgid) gmsgid
#define textdomain(domain) (domain)
#define bindtextdomain(domain, directory) (domain)
#define gettext(msgid) (msgid)
#define gcc_init_libintl()
#define gcc_gettext_width(s) 0

extern const char *open_quote;
extern const char *close_quote;

#endif
