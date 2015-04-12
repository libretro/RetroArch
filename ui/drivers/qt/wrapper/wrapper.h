#ifndef WRAPPER_H
#define WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Wimp Wimp;

Wimp* ctrWimp(int argc, char *argv[]);

int CreateMainWindow(Wimp* p);

#ifdef __cplusplus
}
#endif

#endif // WRAPPER_H
