#pragma once

#define MK_STR_(s)	#s
#define MK_STR(s)	MK_STR_(s)


#define APP_VER		0
#define APP_REV		3

#define APP_VER_S	MK_STR(APP_VER) "." MK_STR(APP_REV)

