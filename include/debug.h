#ifndef _DEBUG_H_
#define _DEBUG_H_

void panic_raw(const char *func, const char *msg);

#define panic(msg) panic_raw(__func__, msg)

#endif
