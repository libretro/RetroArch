#ifndef WRAPPER_H
#define WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Test Test;

Test* ctrTest(int argc, char *argv[]);

int CreateMainWindow(Test* p);

#ifdef __cplusplus
}
#endif

#endif // WRAPPER_H
