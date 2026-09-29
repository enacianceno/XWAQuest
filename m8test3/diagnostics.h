#ifndef M8_DIAGNOSTICS_H
#define M8_DIAGNOSTICS_H
void M8_Diag(const char *format,...);
void M8_Memory(const char *checkpoint,const char *stage,int full);
void M8_MemoryPoll(void);
#endif
