#ifndef LCD_PARALLAX_H
#define LCD_PARALLAX_H

MODULE_EXPORT int  parallax_init (Driver *drvthis);
MODULE_EXPORT void parallax_close (Driver *drvthis);
MODULE_EXPORT int  parallax_width (Driver *drvthis);
MODULE_EXPORT int  parallax_height (Driver *drvthis);
MODULE_EXPORT int  parallax_cellwidth (Driver *drvthis);
MODULE_EXPORT int  parallax_cellheight (Driver *drvthis);
MODULE_EXPORT void parallax_clear (Driver *drvthis);
MODULE_EXPORT void parallax_flush (Driver *drvthis);
MODULE_EXPORT void parallax_string (Driver *drvthis, int x, int y, const char string[]);
MODULE_EXPORT void parallax_chr (Driver *drvthis, int x, int y, char c);

MODULE_EXPORT void parallax_vbar (Driver *drvthis, int x, int y, int len, int promille, int options);
MODULE_EXPORT void parallax_hbar (Driver *drvthis, int x, int y, int len, int promille, int options);
MODULE_EXPORT void parallax_num (Driver *drvthis, int x, int num);
MODULE_EXPORT int  parallax_icon (Driver *drvthis, int x, int y, int icon);
MODULE_EXPORT void parallax_set_char (Driver *drvthis, int n, unsigned char *dat);
MODULE_EXPORT int  parallax_get_free_chars(Driver *drvthis);

MODULE_EXPORT void parallax_backlight (Driver *drvthis, int promille);

MODULE_EXPORT const char *parallax_get_info (Driver *drvthis);

#endif
