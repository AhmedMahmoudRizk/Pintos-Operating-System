#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void exit_proc(int Status);
void validate_Uaddr(void* Uaddr);

#endif /* userprog/syscall.h */
