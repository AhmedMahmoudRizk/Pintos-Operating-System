#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>

#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "devices/input.h"
#include <stdio.h>

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
	//printf("%d,%d\n",sizeof(int),sizeof(char));
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	int * curr_p = f->esp;
	validate_Uaddr(curr_p);
  	int sys_call_num = * curr_p;
	struct file *fop;
	switch (sys_call_num)
	{
		case SYS_HALT:
		shutdown_power_off();
		break;

		case SYS_EXIT:
		validate_Uaddr((void *)(curr_p+1));
		exit_proc(*(curr_p+1));
		break;

		case SYS_EXEC:
		validate_Uaddr((void *)(curr_p+1));
		validate_Uaddr((void *)*(curr_p+1));
		f->eax = process_execute((const char *)*(curr_p+1));
		break;

		case SYS_WAIT:
		validate_Uaddr((void *)(curr_p+1));
		f->eax = process_wait(*(curr_p+1));
		break;

		case SYS_CREATE:
		validate_Uaddr((void *)(curr_p+1));
		validate_Uaddr((void *)*(curr_p+1));
		validate_Uaddr((void *)(curr_p+2));
		lock_acquire ((struct lock *)get_file_sys_lock());
		f->eax = filesys_create ((char *)*(curr_p+1),*(curr_p+2)); 
		lock_release ((struct lock *)get_file_sys_lock());
		break;

		case SYS_REMOVE:
		validate_Uaddr((void *)(curr_p+1));
		validate_Uaddr((void *)*(curr_p+1));
		lock_acquire ((struct lock *)get_file_sys_lock());
		f->eax = filesys_remove ((char *)*(curr_p+1)); 
		lock_release ((struct lock *)get_file_sys_lock());
		break;

		case SYS_OPEN:
		validate_Uaddr((void *)(curr_p+1));
		validate_Uaddr((void *)*(curr_p+1));

		lock_acquire ((struct lock *)get_file_sys_lock());
		fop = filesys_open ((char *)*(curr_p+1));
		lock_release ((struct lock *)get_file_sys_lock());

		if(!fop)
			f->eax = -1;
		else{
		struct file_descriptor *file = malloc(sizeof(file));
		file->fd = list_size(&thread_current()->opened_files)+2;
		file->opened_file = fop;
		list_push_back(&thread_current()->opened_files,&file->elem);
		f->eax = file->fd;
		}
		break;

		case SYS_FILESIZE:
		validate_Uaddr((void *)(curr_p+1));
		fop = get_opened_file(*(curr_p+1),true);
		if(!fop)
			f->eax = -1;
		else{
			f->eax = file_length (fop); 
		}
		break;

		case SYS_READ:
		validate_Uaddr((void *)(curr_p+3));
		validate_Uaddr((void *)(curr_p+2));
		validate_Uaddr((void *)*(curr_p+2));
		validate_Uaddr((void *)(curr_p+1));
		if((int)*(curr_p+1) == 0){
			input_getc ();
			f->eax = 1;
		}
		else{
		fop = get_opened_file(*(curr_p+1),true);
		if(!fop)
			f->eax = -1;
		else{
			//file_deny_write(fop);
			f->eax = file_read (fop, (void *)*(curr_p+2), *(curr_p+3)) ;
			//file_allow_write(fop);
		}
	}
		break;

		case SYS_WRITE:
		validate_Uaddr((void *)(curr_p+3));
		validate_Uaddr((void *)(curr_p+2));
		validate_Uaddr((void *)*(curr_p+2));
		validate_Uaddr((void *)(curr_p+1));
		if(*(curr_p+1)==1)
		{
		putbuf((const char *)*(curr_p+2),*(curr_p+3));
		f->eax = *(curr_p+3);
		}else{
			fop = get_opened_file(*(curr_p+1),true);
			if(!fop)
				f->eax = 0;
			else{
				f->eax = file_write (fop, (void *)*(curr_p+2), *(curr_p+3)) ;
			}
		}
		break;

		case SYS_SEEK:
		validate_Uaddr((void *)(curr_p+2));
		validate_Uaddr((void *)(curr_p+1));
		fop = get_opened_file(*(curr_p+1),true);
			if(fop){
				file_seek (fop,*(curr_p+2)) ;
			}
		break;

		case SYS_TELL:
		validate_Uaddr((void *)(curr_p+1));
		fop = get_opened_file(*(curr_p+1),true);
			if(fop){
				f->eax = file_tell (fop) ;
			}else
				f->eax = -1;
		break;

		case SYS_CLOSE:
		validate_Uaddr((void *)(curr_p+1));
		fop = get_opened_file(*(curr_p+1),true);
			if(fop){
				file_close (fop) ;
				get_opened_file(*(curr_p+1),false);
			}
		break;
		default:
		printf("ERROR\n");
	}
}
	void exit_proc(int Status){
		/*struct thread *cur = thread_current();
		cur->exit_status = Status; */
 		struct thread *ch = thread_current();
 		ch->exit_status = Status;
 		printf("%s: exit(%d)\n",ch->name,Status);

  		struct list_elem *e;
  		//printf("here %d\n",list_size(&thread_current()->parent->child_list));
 		 struct child *t=NULL;
  		for (e = list_begin (&thread_current()->parent->child_list); e != list_end (&thread_current()->parent->child_list);
	           e = list_next (e))
	       	 {
         	 t = list_entry (e, struct child, elem);
         	 if(t->tid == ch->tid)
         	 {
         	 	t->finished = true;
 				t->exit_status = ch->exit_status;
         	 }
       	 }

 		if(ch->parent->waited_child == t)
 			{
 				sema_up(&ch->parent->lock_of_parent);
 			}	
		thread_exit();
	}

	void validate_Uaddr(void* Uaddr){
		//printf("Congratulations - you have successfully dereferenced NULL: %d",*(int *)NULL);
		//printf("!Uaddr %d,!mapped page %d ,!NULL %d\n", !is_user_vaddr (Uaddr),!pagedir_get_page(thread_current()->pagedir,Uaddr),!(*Uaddr));
		if(!is_user_vaddr (Uaddr) || !pagedir_get_page(thread_current()->pagedir, Uaddr))  {
			exit_proc(-1);
		}
	}


	