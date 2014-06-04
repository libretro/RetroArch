#ifndef SYSTEMBUTTONS_PRX_H
#define SYSTEMBUTTONS_PRX_H

#ifdef __cplusplus
extern "C" {
#endif

unsigned int readSystemButtons(void);
void loadGame( const char* fileName, void * argp);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEMBUTTONS_PRX_H */
